#include "PixelPipeline.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace {

constexpr float kByteMax = 255.0f;
constexpr float kAe16White = 32768.0f;
constexpr uint64_t kParallelPixelThreshold = 512ULL * 512ULL;
constexpr int kMaxPixelWorkers = 4;

struct Rgb {
    float r;
    float g;
    float b;
};

size_t BytesPerPixel(HostColorFormat format) noexcept {
    switch (format) {
        case HostColorFormat::AE_ARGB_8u:
        case HostColorFormat::PR_BGRA_8u:
            return 4;
        case HostColorFormat::AE_ARGB_16u:
            return 8;
        case HostColorFormat::AE_ARGB_32f:
        case HostColorFormat::PR_BGRA_32f:
            return 16;
    }
    return 0;
}

uint64_t PitchMagnitude(intptr_t pitch) noexcept {
    if (pitch >= 0) {
        return static_cast<uint64_t>(pitch);
    }
    return static_cast<uint64_t>(-(pitch + 1)) + 1;
}

bool ValidateFrame(
    const void* pixels,
    uint32_t width,
    uint32_t height,
    intptr_t rowPitch,
    HostColorFormat format) noexcept
{
    if (!pixels || width == 0 || height == 0) {
        return false;
    }
    if (height > static_cast<uint32_t>((std::numeric_limits<int>::max)())) {
        return false;
    }

    const size_t bytesPerPixel = BytesPerPixel(format);
    if (bytesPerPixel == 0) {
        return false;
    }

    const uint64_t minimumPitch = static_cast<uint64_t>(width) * bytesPerPixel;
    const uint64_t pitchMagnitude = PitchMagnitude(rowPitch);
    if (pitchMagnitude < minimumPitch) {
        return false;
    }

    return height == 1 ||
        pitchMagnitude <= static_cast<uint64_t>(std::numeric_limits<intptr_t>::max()) /
            static_cast<uint64_t>(height - 1);
}

float FiniteOrZero(float value) noexcept {
    return std::isfinite(value) ? value : 0.0f;
}

uint8_t UnitToByte(float value) noexcept {
    const float scaled = std::clamp(FiniteOrZero(value), 0.0f, 1.0f) * kByteMax;
    return static_cast<uint8_t>(std::lround(scaled));
}

uint16_t UnitToAe16(float value) noexcept {
    const float scaled = std::clamp(FiniteOrZero(value), 0.0f, 1.0f) * kAe16White;
    return static_cast<uint16_t>(std::lround(scaled));
}

int PixelWorkerCount(uint32_t width, uint32_t height) noexcept {
#ifdef _OPENMP
    const uint64_t pixelCount = static_cast<uint64_t>(width) * height;
    if (pixelCount < kParallelPixelThreshold) {
        return 1;
    }
    return (std::min)(kMaxPixelWorkers, omp_get_max_threads());
#else
    static_cast<void>(width);
    static_cast<void>(height);
    return 1;
#endif
}

uint8_t BlendByte(uint8_t original, uint8_t processed, float factor) noexcept {
    if (factor == 1.0f) {
        return processed;
    }
    const float value = static_cast<float>(original) +
        (static_cast<float>(processed) - static_cast<float>(original)) * factor;
    const float clamped = std::clamp(value, 0.0f, kByteMax);
    return static_cast<uint8_t>(clamped + 0.5f);
}

Rgb ComposeOutput(
    Rgb original,
    Rgb processed,
    uint32_t x,
    uint32_t width,
    const DLSSParameters& params) noexcept
{
    original = {
        FiniteOrZero(original.r),
        FiniteOrZero(original.g),
        FiniteOrZero(original.b)
    };
    const float intensityGain = DLSSIntensityGain(params.intensity);
    processed = {
        original.r + (FiniteOrZero(processed.r) - original.r) * intensityGain,
        original.g + (FiniteOrZero(processed.g) - original.g) * intensityGain,
        original.b + (FiniteOrZero(processed.b) - original.b) * intensityGain
    };

    if (params.outputView == DLSS_VIEW_DIFF_10X) {
        return {
            std::clamp(0.5f + (processed.r - original.r) * 10.0f, 0.0f, 1.0f),
            std::clamp(0.5f + (processed.g - original.g) * 10.0f, 0.0f, 1.0f),
            std::clamp(0.5f + (processed.b - original.b) * 10.0f, 0.0f, 1.0f)
        };
    }

    if (params.outputView == DLSS_VIEW_SPLIT_COMPARE) {
        if (x < width / 2) {
            return original;
        }
        if (x == width / 2) {
            return { 1.0f, 1.0f, 1.0f };
        }
        return processed;
    }

    const float mix = std::clamp(FiniteOrZero(params.outputMix), 0.0f, 1.0f);
    return {
        original.r + (processed.r - original.r) * mix,
        original.g + (processed.g - original.g) * mix,
        original.b + (processed.b - original.b) * mix
    };
}

bool IsExactBypass(const DLSSParameters& params) noexcept {
    return params.outputView == DLSS_VIEW_PROCESSED &&
        FiniteOrZero(params.outputMix) <= 0.0f;
}

} // namespace

namespace PixelPipeline {

bool GetRgba8BufferSize(uint32_t width, uint32_t height, size_t& size) noexcept {
    size = 0;
    if (width == 0 || height == 0) {
        return false;
    }

    constexpr size_t channels = 4;
    if (static_cast<size_t>(width) > std::numeric_limits<size_t>::max() / channels) {
        return false;
    }

    const size_t rowSize = static_cast<size_t>(width) * channels;
    if (static_cast<size_t>(height) > std::numeric_limits<size_t>::max() / rowSize) {
        return false;
    }

    size = rowSize * static_cast<size_t>(height);
    return true;
}

bool ConvertToRgba8(
    const void* inputPixels,
    uint32_t width,
    uint32_t height,
    intptr_t rowPitch,
    HostColorFormat format,
    uint8_t* rgbaPixels,
    size_t rgbaSize) noexcept
{
    size_t requiredSize = 0;
    if (!ValidateFrame(inputPixels, width, height, rowPitch, format) ||
        !GetRgba8BufferSize(width, height, requiredSize) ||
        !rgbaPixels || rgbaSize < requiredSize)
    {
        return false;
    }

    const auto* source = static_cast<const uint8_t*>(inputPixels);
    const int workerCount = PixelWorkerCount(width, height);

    #pragma omp parallel for schedule(static) if(workerCount > 1) num_threads(workerCount)
    for (int y = 0; y < static_cast<int>(height); ++y) {
        const auto* sourceRow = source + static_cast<intptr_t>(y) * rowPitch;
        auto* destinationRow = rgbaPixels + static_cast<size_t>(y) * width * 4;

        for (uint32_t x = 0; x < width; ++x) {
            auto* destination = destinationRow + static_cast<size_t>(x) * 4;

            switch (format) {
                case HostColorFormat::AE_ARGB_8u: {
                    const auto* pixel = sourceRow + static_cast<size_t>(x) * 4;
                    destination[0] = pixel[1];
                    destination[1] = pixel[2];
                    destination[2] = pixel[3];
                    destination[3] = pixel[0];
                    break;
                }
                case HostColorFormat::AE_ARGB_16u: {
                    const auto* pixel = reinterpret_cast<const uint16_t*>(sourceRow) + static_cast<size_t>(x) * 4;
                    destination[0] = UnitToByte(pixel[1] / kAe16White);
                    destination[1] = UnitToByte(pixel[2] / kAe16White);
                    destination[2] = UnitToByte(pixel[3] / kAe16White);
                    destination[3] = UnitToByte(pixel[0] / kAe16White);
                    break;
                }
                case HostColorFormat::AE_ARGB_32f: {
                    const auto* pixel = reinterpret_cast<const float*>(sourceRow) + static_cast<size_t>(x) * 4;
                    destination[0] = UnitToByte(pixel[1]);
                    destination[1] = UnitToByte(pixel[2]);
                    destination[2] = UnitToByte(pixel[3]);
                    destination[3] = UnitToByte(pixel[0]);
                    break;
                }
                case HostColorFormat::PR_BGRA_8u: {
                    const auto* pixel = sourceRow + static_cast<size_t>(x) * 4;
                    destination[0] = pixel[2];
                    destination[1] = pixel[1];
                    destination[2] = pixel[0];
                    destination[3] = pixel[3];
                    break;
                }
                case HostColorFormat::PR_BGRA_32f: {
                    const auto* pixel = reinterpret_cast<const float*>(sourceRow) + static_cast<size_t>(x) * 4;
                    destination[0] = UnitToByte(pixel[2]);
                    destination[1] = UnitToByte(pixel[1]);
                    destination[2] = UnitToByte(pixel[0]);
                    destination[3] = UnitToByte(pixel[3]);
                    break;
                }
            }
        }
    }

    return true;
}

bool WriteFromRgba8(
    const void* originalPixels,
    const uint8_t* processedRgba,
    size_t processedSize,
    void* outputPixels,
    uint32_t width,
    uint32_t height,
    intptr_t originalRowPitch,
    intptr_t outputRowPitch,
    HostColorFormat format,
    const DLSSParameters& params) noexcept
{
    size_t requiredSize = 0;
    if (!ValidateFrame(originalPixels, width, height, originalRowPitch, format) ||
        !ValidateFrame(outputPixels, width, height, outputRowPitch, format) ||
        !GetRgba8BufferSize(width, height, requiredSize) ||
        !processedRgba || processedSize < requiredSize)
    {
        return false;
    }

    const auto* original = static_cast<const uint8_t*>(originalPixels);
    auto* output = static_cast<uint8_t*>(outputPixels);
    const size_t activeRowBytes = static_cast<size_t>(width) * BytesPerPixel(format);
    const int workerCount = PixelWorkerCount(width, height);

    if (IsExactBypass(params)) {
        #pragma omp parallel for schedule(static) if(workerCount > 1) num_threads(workerCount)
        for (int y = 0; y < static_cast<int>(height); ++y) {
            std::memmove(
                output + static_cast<intptr_t>(y) * outputRowPitch,
                original + static_cast<intptr_t>(y) * originalRowPitch,
                activeRowBytes);
        }
        return true;
    }

    if (params.outputView == DLSS_VIEW_PROCESSED &&
        (format == HostColorFormat::AE_ARGB_8u || format == HostColorFormat::PR_BGRA_8u))
    {
        const float mix = std::clamp(FiniteOrZero(params.outputMix), 0.0f, 1.0f);
        const float blendFactor = DLSSIntensityGain(params.intensity) * mix;

        #pragma omp parallel for schedule(static) if(workerCount > 1) num_threads(workerCount)
        for (int y = 0; y < static_cast<int>(height); ++y) {
            const auto* originalRow = original + static_cast<intptr_t>(y) * originalRowPitch;
            const auto* processedRow = processedRgba + static_cast<size_t>(y) * width * 4;
            auto* outputRow = output + static_cast<intptr_t>(y) * outputRowPitch;

            for (uint32_t x = 0; x < width; ++x) {
                const auto* sourcePixel = originalRow + static_cast<size_t>(x) * 4;
                const auto* processedPixel = processedRow + static_cast<size_t>(x) * 4;
                auto* destinationPixel = outputRow + static_cast<size_t>(x) * 4;

                if (format == HostColorFormat::AE_ARGB_8u) {
                    destinationPixel[0] = sourcePixel[0];
                    destinationPixel[1] = BlendByte(sourcePixel[1], processedPixel[0], blendFactor);
                    destinationPixel[2] = BlendByte(sourcePixel[2], processedPixel[1], blendFactor);
                    destinationPixel[3] = BlendByte(sourcePixel[3], processedPixel[2], blendFactor);
                } else {
                    destinationPixel[0] = BlendByte(sourcePixel[0], processedPixel[2], blendFactor);
                    destinationPixel[1] = BlendByte(sourcePixel[1], processedPixel[1], blendFactor);
                    destinationPixel[2] = BlendByte(sourcePixel[2], processedPixel[0], blendFactor);
                    destinationPixel[3] = sourcePixel[3];
                }
            }
        }
        return true;
    }

    #pragma omp parallel for schedule(static) if(workerCount > 1) num_threads(workerCount)
    for (int y = 0; y < static_cast<int>(height); ++y) {
        const auto* originalRow = original + static_cast<intptr_t>(y) * originalRowPitch;
        const auto* processedRow = processedRgba + static_cast<size_t>(y) * width * 4;
        auto* outputRow = output + static_cast<intptr_t>(y) * outputRowPitch;

        for (uint32_t x = 0; x < width; ++x) {
            const auto* processedPixel = processedRow + static_cast<size_t>(x) * 4;
            const Rgb processed = {
                processedPixel[0] / kByteMax,
                processedPixel[1] / kByteMax,
                processedPixel[2] / kByteMax
            };

            switch (format) {
                case HostColorFormat::AE_ARGB_8u: {
                    const auto* sourcePixel = originalRow + static_cast<size_t>(x) * 4;
                    auto* destinationPixel = outputRow + static_cast<size_t>(x) * 4;
                    const Rgb result = ComposeOutput(
                        { sourcePixel[1] / kByteMax, sourcePixel[2] / kByteMax, sourcePixel[3] / kByteMax },
                        processed,
                        x,
                        width,
                        params);
                    destinationPixel[0] = sourcePixel[0];
                    destinationPixel[1] = UnitToByte(result.r);
                    destinationPixel[2] = UnitToByte(result.g);
                    destinationPixel[3] = UnitToByte(result.b);
                    break;
                }
                case HostColorFormat::AE_ARGB_16u: {
                    const auto* sourcePixel = reinterpret_cast<const uint16_t*>(originalRow) + static_cast<size_t>(x) * 4;
                    auto* destinationPixel = reinterpret_cast<uint16_t*>(outputRow) + static_cast<size_t>(x) * 4;
                    const Rgb result = ComposeOutput(
                        { sourcePixel[1] / kAe16White, sourcePixel[2] / kAe16White, sourcePixel[3] / kAe16White },
                        processed,
                        x,
                        width,
                        params);
                    destinationPixel[0] = sourcePixel[0];
                    destinationPixel[1] = UnitToAe16(result.r);
                    destinationPixel[2] = UnitToAe16(result.g);
                    destinationPixel[3] = UnitToAe16(result.b);
                    break;
                }
                case HostColorFormat::AE_ARGB_32f: {
                    const auto* sourcePixel = reinterpret_cast<const float*>(originalRow) + static_cast<size_t>(x) * 4;
                    auto* destinationPixel = reinterpret_cast<float*>(outputRow) + static_cast<size_t>(x) * 4;
                    const Rgb result = ComposeOutput(
                        { sourcePixel[1], sourcePixel[2], sourcePixel[3] },
                        processed,
                        x,
                        width,
                        params);
                    destinationPixel[0] = sourcePixel[0];
                    destinationPixel[1] = result.r;
                    destinationPixel[2] = result.g;
                    destinationPixel[3] = result.b;
                    break;
                }
                case HostColorFormat::PR_BGRA_8u: {
                    const auto* sourcePixel = originalRow + static_cast<size_t>(x) * 4;
                    auto* destinationPixel = outputRow + static_cast<size_t>(x) * 4;
                    const Rgb result = ComposeOutput(
                        { sourcePixel[2] / kByteMax, sourcePixel[1] / kByteMax, sourcePixel[0] / kByteMax },
                        processed,
                        x,
                        width,
                        params);
                    destinationPixel[0] = UnitToByte(result.b);
                    destinationPixel[1] = UnitToByte(result.g);
                    destinationPixel[2] = UnitToByte(result.r);
                    destinationPixel[3] = sourcePixel[3];
                    break;
                }
                case HostColorFormat::PR_BGRA_32f: {
                    const auto* sourcePixel = reinterpret_cast<const float*>(originalRow) + static_cast<size_t>(x) * 4;
                    auto* destinationPixel = reinterpret_cast<float*>(outputRow) + static_cast<size_t>(x) * 4;
                    const Rgb result = ComposeOutput(
                        { sourcePixel[2], sourcePixel[1], sourcePixel[0] },
                        processed,
                        x,
                        width,
                        params);
                    destinationPixel[0] = result.b;
                    destinationPixel[1] = result.g;
                    destinationPixel[2] = result.r;
                    destinationPixel[3] = sourcePixel[3];
                    break;
                }
            }
        }
    }

    return true;
}

} // namespace PixelPipeline

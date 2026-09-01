#include "PixelPipeline.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>

namespace {

int g_failures = 0;

void Check(bool condition, const char* expression, int line) {
    if (!condition) {
        std::cerr << "FAILED line " << line << ": " << expression << '\n';
        ++g_failures;
    }
}

#define CHECK(expression) Check((expression), #expression, __LINE__)

void TestArgb8ConversionAndAlpha() {
    const std::array<uint8_t, 8> input = {
        0, 10, 20, 30,
        255, 40, 50, 60
    };
    std::array<uint8_t, 8> rgba = {};

    CHECK(PixelPipeline::ConvertToRgba8(
        input.data(), 2, 1, 8, HostColorFormat::AE_ARGB_8u, rgba.data(), rgba.size()));
    CHECK((rgba == std::array<uint8_t, 8>{ 10, 20, 30, 0, 40, 50, 60, 255 }));

    const std::array<uint8_t, 8> processed = {
        200, 150, 100, 255,
        90, 80, 70, 255
    };
    std::array<uint8_t, 8> output = {};
    DLSSParameters parameters;

    CHECK(PixelPipeline::WriteFromRgba8(
        input.data(),
        processed.data(),
        processed.size(),
        output.data(),
        2,
        1,
        8,
        8,
        HostColorFormat::AE_ARGB_8u,
        parameters));
    CHECK(output[0] == 0);
    CHECK(output[1] == 200 && output[2] == 150 && output[3] == 100);
    CHECK(output[4] == 255);
}

void TestArgb16Conversion() {
    const std::array<uint16_t, 4> input = { 0, 32768, 16384, 0 };
    std::array<uint8_t, 4> rgba = {};

    CHECK(PixelPipeline::ConvertToRgba8(
        input.data(), 1, 1, 8, HostColorFormat::AE_ARGB_16u, rgba.data(), rgba.size()));
    CHECK(rgba[0] == 255 && rgba[1] == 128 && rgba[2] == 0 && rgba[3] == 0);

    const std::array<uint8_t, 4> processed = { 0, 255, 255, 255 };
    std::array<uint16_t, 4> output = {};
    DLSSParameters parameters;

    CHECK(PixelPipeline::WriteFromRgba8(
        input.data(),
        processed.data(),
        processed.size(),
        output.data(),
        1,
        1,
        8,
        8,
        HostColorFormat::AE_ARGB_16u,
        parameters));
    CHECK(output[0] == 0);
    CHECK(output[1] == 0 && output[2] == 32768 && output[3] == 32768);
}

void TestFloatBypassPreservesOriginal() {
    const std::array<float, 4> input = { 0.0f, -0.25f, 2.5f, 0.5f };
    const std::array<uint8_t, 4> processed = { 255, 255, 255, 255 };
    std::array<float, 4> output = {};
    DLSSParameters parameters;
    parameters.outputMix = 0.0f;

    CHECK(PixelPipeline::WriteFromRgba8(
        input.data(),
        processed.data(),
        processed.size(),
        output.data(),
        1,
        1,
        16,
        16,
        HostColorFormat::AE_ARGB_32f,
        parameters));
    CHECK(std::memcmp(input.data(), output.data(), sizeof(input)) == 0);

    parameters.outputMix = 1.0f;
    output.fill(-1.0f);
    CHECK(PixelPipeline::WriteFromRgba8(
        input.data(),
        processed.data(),
        processed.size(),
        output.data(),
        1,
        1,
        16,
        16,
        HostColorFormat::AE_ARGB_32f,
        parameters));
    CHECK(output[0] == 0.0f);
    CHECK(output[1] == 1.0f && output[2] == 1.0f && output[3] == 1.0f);
}

void TestPremiereChannelOrder() {
    const std::array<uint8_t, 4> input = { 30, 20, 10, 0 };
    std::array<uint8_t, 4> rgba = {};
    CHECK(PixelPipeline::ConvertToRgba8(
        input.data(), 1, 1, 4, HostColorFormat::PR_BGRA_8u, rgba.data(), rgba.size()));
    CHECK((rgba == std::array<uint8_t, 4>{ 10, 20, 30, 0 }));

    const std::array<uint8_t, 4> processed = { 255, 0, 0, 255 };
    std::array<uint8_t, 4> output = {};
    DLSSParameters parameters;
    CHECK(PixelPipeline::WriteFromRgba8(
        input.data(),
        processed.data(),
        processed.size(),
        output.data(),
        1,
        1,
        4,
        4,
        HostColorFormat::PR_BGRA_8u,
        parameters));
    CHECK((output == std::array<uint8_t, 4>{ 0, 0, 255, 0 }));
}

void TestSplitView() {
    const std::array<uint8_t, 12> input = {
        255, 10, 20, 30,
        255, 40, 50, 60,
        255, 70, 80, 90
    };
    const std::array<uint8_t, 12> processed = {
        1, 2, 3, 255,
        4, 5, 6, 255,
        7, 8, 9, 255
    };
    std::array<uint8_t, 12> output = {};
    DLSSParameters parameters;
    parameters.outputView = DLSS_VIEW_SPLIT_COMPARE;

    CHECK(PixelPipeline::WriteFromRgba8(
        input.data(),
        processed.data(),
        processed.size(),
        output.data(),
        3,
        1,
        12,
        12,
        HostColorFormat::AE_ARGB_8u,
        parameters));
    CHECK((output == std::array<uint8_t, 12>{
        255, 10, 20, 30,
        255, 255, 255, 255,
        255, 7, 8, 9
    }));
}

void TestValidation() {
    const std::array<uint8_t, 8> input = {};
    std::array<uint8_t, 8> rgba = {};
    CHECK(!PixelPipeline::ConvertToRgba8(
        input.data(), 2, 1, 4, HostColorFormat::AE_ARGB_8u, rgba.data(), rgba.size()));
    CHECK(!PixelPipeline::ConvertToRgba8(
        input.data(), 2, 1, 8, HostColorFormat::AE_ARGB_8u, rgba.data(), rgba.size() - 1));

    size_t requiredSize = 0;
    CHECK(!PixelPipeline::GetRgba8BufferSize(UINT32_MAX, UINT32_MAX, requiredSize));
}

void TestNegativeRowPitch() {
    const std::array<uint8_t, 8> input = {
        255, 10, 20, 30,
        128, 40, 50, 60
    };
    std::array<uint8_t, 8> rgba = {};

    CHECK(PixelPipeline::ConvertToRgba8(
        input.data() + 4,
        1,
        2,
        -4,
        HostColorFormat::AE_ARGB_8u,
        rgba.data(),
        rgba.size()));
    CHECK((rgba == std::array<uint8_t, 8>{ 40, 50, 60, 128, 10, 20, 30, 255 }));
}

void TestNeuralControlRange() {
    CHECK(NormalizeDLSSControl(-0.5f) == DLSS_CONTROL_MIN);
    CHECK(NormalizeDLSSControl(4.5f) == 4.5f);
    CHECK(NormalizeDLSSControl(6.0f) == DLSS_CONTROL_MAX);
    CHECK(NormalizeDLSSControl(std::numeric_limits<float>::infinity()) == DLSS_CONTROL_DEFAULT);
    CHECK(NormalizeDLSSControl(std::numeric_limits<float>::quiet_NaN()) == DLSS_CONTROL_DEFAULT);
    CHECK(DLSSRuntimeIntensity(0.5f) == 0.5f);
    CHECK(DLSSRuntimeIntensity(2.0f) == DLSS_RUNTIME_INTENSITY_MAX);
    CHECK(DLSSIntensityGain(0.5f) == 1.0f);
    CHECK(DLSSIntensityGain(2.0f) == 2.0f);
}

void TestIntensityOverdrive() {
    const std::array<uint8_t, 4> input = { 255, 100, 100, 100 };
    const std::array<uint8_t, 4> processed = { 120, 90, 80, 255 };
    std::array<uint8_t, 4> output = {};
    DLSSParameters parameters;
    parameters.intensity = 2.0f;

    CHECK(PixelPipeline::WriteFromRgba8(
        input.data(),
        processed.data(),
        processed.size(),
        output.data(),
        1,
        1,
        4,
        4,
        HostColorFormat::AE_ARGB_8u,
        parameters));
    CHECK((output == std::array<uint8_t, 4>{ 255, 140, 80, 60 }));
}

} // namespace

int main() {
    TestArgb8ConversionAndAlpha();
    TestArgb16Conversion();
    TestFloatBypassPreservesOriginal();
    TestPremiereChannelOrder();
    TestSplitView();
    TestValidation();
    TestNegativeRowPitch();
    TestNeuralControlRange();
    TestIntensityOverdrive();

    if (g_failures != 0) {
        std::cerr << g_failures << " test assertion(s) failed\n";
        return 1;
    }

    std::cout << "PixelPipelineTests passed\n";
    return 0;
}

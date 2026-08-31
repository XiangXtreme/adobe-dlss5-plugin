#pragma once

#include "PluginDef.h"

#include <cstddef>
#include <cstdint>

enum class HostColorFormat {
    AE_ARGB_8u,
    AE_ARGB_16u,
    AE_ARGB_32f,
    PR_BGRA_8u,
    PR_BGRA_32f
};

namespace PixelPipeline {

bool GetRgba8BufferSize(uint32_t width, uint32_t height, size_t& size) noexcept;

bool ConvertToRgba8(
    const void* inputPixels,
    uint32_t width,
    uint32_t height,
    intptr_t rowPitch,
    HostColorFormat format,
    uint8_t* rgbaPixels,
    size_t rgbaSize) noexcept;

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
    const DLSSParameters& params) noexcept;

} // namespace PixelPipeline

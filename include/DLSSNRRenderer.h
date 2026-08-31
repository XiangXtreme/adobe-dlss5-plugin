#pragma once

#include "PixelPipeline.h"

#include <cstdint>
#include <mutex>
#include <vector>

class DLSSNRRenderer {
public:
    DLSSNRRenderer() = default;
    ~DLSSNRRenderer();

    void Shutdown();

    bool ProcessFrame(
        const void* inputPixels,
        void* outputPixels,
        uint32_t width,
        uint32_t height,
        intptr_t inRowPitch,
        intptr_t outRowPitch,
        HostColorFormat format,
        const DLSSParameters& params,
        bool resetHistory = false
    );

private:
    std::vector<uint8_t> m_inRgbaBuffer;
    std::vector<uint8_t> m_outRgbaBuffer;
    std::mutex m_renderMutex;
};

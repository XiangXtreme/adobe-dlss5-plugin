#include "DLSSNRRenderer.h"

#include <Windows.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

namespace fs = std::filesystem;

using DlssnrInitFn = int (*)(int, int, int, const wchar_t*, const wchar_t*);
using DlssnrCreateFeatureFn = int (*)(int, int, int);
using DlssnrProcessFn = int (*)(const void*, const void*, const void*, void*, int);
using DlssnrSetOptionsFn = void (*)(int, int, float, float, float, float, int, int, int, int, float, float);
using DlssnrShutdownFn = void (*)();
using DlssnrResizeFn = int (*)(int, int, int);

namespace {

void LogRenderer(const std::string& message) {
    OutputDebugStringA(("[DLSS_Renderer] " + message + "\n").c_str());

    try {
        wchar_t localAppData[32768] = {};
        const DWORD length = GetEnvironmentVariableW(
            L"LOCALAPPDATA",
            localAppData,
            static_cast<DWORD>(std::size(localAppData)));
        if (length == 0 || length >= std::size(localAppData)) {
            return;
        }

        const fs::path logDirectory = fs::path(localAppData) / L"DLSS_Neural_Video";
        fs::create_directories(logDirectory);
        std::ofstream logFile(logDirectory / L"plugin.log", std::ios::app);
        if (logFile) {
            logFile << "[DLSS_Renderer] " << message << '\n';
        }
    } catch (...) {
        // Diagnostics must never interrupt host rendering.
    }
}

std::wstring GetModuleDirectory(HMODULE module) {
    std::vector<wchar_t> buffer(32768);
    const DWORD length = GetModuleFileNameW(module, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) {
        return {};
    }
    return fs::path(std::wstring(buffer.data(), length)).parent_path().wstring();
}

class DLSSHostEngine {
public:
    static DLSSHostEngine& Instance() {
        static DLSSHostEngine instance;
        return instance;
    }

    ~DLSSHostEngine() {
        Shutdown();
    }

    bool Process(
        int width,
        int height,
        const DLSSParameters& parameters,
        const void* inputRgba,
        void* outputRgba,
        bool resetHistory)
    {
        if (!EnsureInitialized(width, height)) {
            return false;
        }

        SetOptions(parameters);
        AllocateZeroGuidanceBuffers(width, height);

        return m_process(
            inputRgba,
            m_zeroMotionVectors.data(),
            m_zeroDepth.data(),
            outputRgba,
            resetHistory ? 1 : 0) != 0;
    }

    void Shutdown() noexcept {
        if (m_runtimeInitialized && m_shutdown) {
            m_shutdown();
        }

        m_runtimeInitialized = false;
        m_featureInitialized = false;
        m_width = 0;
        m_height = 0;
        m_zeroMotionVectors.clear();
        m_zeroDepth.clear();

        if (m_module) {
            FreeLibrary(m_module);
            m_module = nullptr;
        }

        m_init = nullptr;
        m_createFeature = nullptr;
        m_process = nullptr;
        m_setOptions = nullptr;
        m_shutdown = nullptr;
        m_resize = nullptr;
    }

private:
    static std::wstring GetPluginDirectory() {
        HMODULE module = nullptr;
        if (!GetModuleHandleExW(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(&DLSSHostEngine::Instance),
                &module) ||
            !module)
        {
            return {};
        }
        return GetModuleDirectory(module);
    }

    static void AddSearchDirectory(std::vector<fs::path>& directories, const fs::path& directory) {
        if (!directory.empty() &&
            std::find(directories.begin(), directories.end(), directory) == directories.end())
        {
            directories.push_back(directory);
        }
    }

    static std::vector<fs::path> GetSearchDirectories() {
        std::vector<fs::path> directories;
        AddSearchDirectory(directories, GetPluginDirectory());

        wchar_t commonProgramFiles[32768] = {};
        const DWORD commonLength = GetEnvironmentVariableW(
            L"CommonProgramFiles",
            commonProgramFiles,
            static_cast<DWORD>(std::size(commonProgramFiles)));
        if (commonLength > 0 && commonLength < std::size(commonProgramFiles)) {
            AddSearchDirectory(
                directories,
                fs::path(commonProgramFiles) / L"Adobe" / L"Plug-ins" / L"7.0" / L"MediaCore");
        }

        AddSearchDirectory(
            directories,
            fs::path(L"C:\\Program Files\\Adobe\\Common\\Plug-ins\\7.0\\MediaCore"));

        std::vector<wchar_t> executablePath(32768);
        const DWORD executableLength = GetModuleFileNameW(
            nullptr,
            executablePath.data(),
            static_cast<DWORD>(executablePath.size()));
        if (executableLength > 0 && executableLength < executablePath.size()) {
            const fs::path executableDirectory =
                fs::path(std::wstring(executablePath.data(), executableLength)).parent_path();
            AddSearchDirectory(directories, executableDirectory);
            AddSearchDirectory(directories, executableDirectory / L"Plug-Ins" / L"Common");
            AddSearchDirectory(directories, executableDirectory / L"Plug-ins" / L"Effects");
        }

        return directories;
    }

    bool Load() {
        if (m_module) {
            return true;
        }

        for (const fs::path& directory : GetSearchDirectories()) {
            const fs::path hostPath = directory / L"dlssnr_host.dll";
            std::error_code error;
            if (!fs::is_regular_file(hostPath, error)) {
                continue;
            }

            m_module = LoadLibraryExW(
                hostPath.c_str(),
                nullptr,
                LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
            if (m_module) {
                LogRenderer("Loaded runtime host from: " + hostPath.string());
                break;
            }
        }

        if (!m_module) {
            LogRenderer("Unable to locate dlssnr_host.dll");
            return false;
        }

        m_init = reinterpret_cast<DlssnrInitFn>(GetProcAddress(m_module, "dlssnr_init"));
        m_createFeature = reinterpret_cast<DlssnrCreateFeatureFn>(GetProcAddress(m_module, "dlssnr_create_feature"));
        m_process = reinterpret_cast<DlssnrProcessFn>(GetProcAddress(m_module, "dlssnr_process"));
        m_setOptions = reinterpret_cast<DlssnrSetOptionsFn>(GetProcAddress(m_module, "dlssnr_set_options"));
        m_shutdown = reinterpret_cast<DlssnrShutdownFn>(GetProcAddress(m_module, "dlssnr_shutdown"));
        m_resize = reinterpret_cast<DlssnrResizeFn>(GetProcAddress(m_module, "dlssnr_resize"));

        if (!m_init || !m_createFeature || !m_process || !m_setOptions || !m_shutdown || !m_resize) {
            LogRenderer("Runtime host is missing required exports");
            Shutdown();
            return false;
        }

        return true;
    }

    static std::wstring GetRuntimeModelPath() {
        for (const fs::path& directory : GetSearchDirectories()) {
            const fs::path modelPath = directory / L"nvngx_dlssnr.dll";
            std::error_code error;
            if (fs::is_regular_file(modelPath, error)) {
                return modelPath.wstring();
            }
        }
        return {};
    }

    static std::wstring GetRuntimeLogPath() {
        wchar_t localAppData[32768] = {};
        const DWORD length = GetEnvironmentVariableW(
            L"LOCALAPPDATA",
            localAppData,
            static_cast<DWORD>(std::size(localAppData)));
        if (length == 0 || length >= std::size(localAppData)) {
            return {};
        }

        try {
            const fs::path directory = fs::path(localAppData) / L"DLSS_Neural_Video";
            fs::create_directories(directory);
            return (directory / L"dlss_run.log").wstring();
        } catch (...) {
            return {};
        }
    }

    bool EnsureInitialized(int width, int height) {
        constexpr int preset = 1;

        if (!Load()) {
            return false;
        }

        if (!m_featureInitialized) {
            const std::wstring modelPath = GetRuntimeModelPath();
            if (modelPath.empty()) {
                LogRenderer("Unable to locate nvngx_dlssnr.dll");
                return false;
            }

            const std::wstring logPath = GetRuntimeLogPath();
            if (m_init(width, height, preset, modelPath.c_str(), logPath.c_str()) == 0) {
                LogRenderer("dlssnr_init failed");
                return false;
            }
            m_runtimeInitialized = true;

            if (m_createFeature(width, height, preset) == 0) {
                LogRenderer("dlssnr_create_feature failed");
                Shutdown();
                return false;
            }

            m_featureInitialized = true;
            m_width = width;
            m_height = height;
            AllocateZeroGuidanceBuffers(width, height);
            LogRenderer("Initialized neural runtime for " + std::to_string(width) + "x" + std::to_string(height));
            return true;
        }

        if (m_width != width || m_height != height) {
            if (m_resize(width, height, preset) == 0) {
                LogRenderer("dlssnr_resize failed");
                return false;
            }
            m_width = width;
            m_height = height;
            AllocateZeroGuidanceBuffers(width, height);
            LogRenderer("Resized neural runtime to " + std::to_string(width) + "x" + std::to_string(height));
        }

        return true;
    }

    void AllocateZeroGuidanceBuffers(int width, int height) {
        const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
        const size_t motionVectorCount = pixelCount * 2;
        if (m_zeroMotionVectors.size() != motionVectorCount) {
            m_zeroMotionVectors.assign(motionVectorCount, 0.0f);
        }
        if (m_zeroDepth.size() != pixelCount) {
            m_zeroDepth.assign(pixelCount, 0.0f);
        }
    }

    void SetOptions(const DLSSParameters& parameters) const {
        m_setOptions(
            1,
            std::clamp(parameters.style, 0, 3),
            DLSSRuntimeIntensity(parameters.intensity),
            NormalizeDLSSControl(parameters.localTone),
            NormalizeDLSSControl(parameters.localStructure),
            NormalizeDLSSControl(parameters.skinStructure),
            0,
            0,
            0,
            2,
            1.0f,
            1.0f);
    }

    HMODULE m_module = nullptr;
    bool m_runtimeInitialized = false;
    bool m_featureInitialized = false;
    int m_width = 0;
    int m_height = 0;

    std::vector<float> m_zeroMotionVectors;
    std::vector<float> m_zeroDepth;

    DlssnrInitFn m_init = nullptr;
    DlssnrCreateFeatureFn m_createFeature = nullptr;
    DlssnrProcessFn m_process = nullptr;
    DlssnrSetOptionsFn m_setOptions = nullptr;
    DlssnrShutdownFn m_shutdown = nullptr;
    DlssnrResizeFn m_resize = nullptr;
};

} // namespace

DLSSNRRenderer::~DLSSNRRenderer() {
    Shutdown();
}

void DLSSNRRenderer::Shutdown() {
    std::lock_guard<std::mutex> lock(m_renderMutex);
    DLSSHostEngine::Instance().Shutdown();
    m_inRgbaBuffer.clear();
    m_outRgbaBuffer.clear();
}

bool DLSSNRRenderer::ProcessFrame(
    const void* inputPixels,
    void* outputPixels,
    uint32_t width,
    uint32_t height,
    intptr_t inRowPitch,
    intptr_t outRowPitch,
    HostColorFormat format,
    const DLSSParameters& parameters,
    bool resetHistory)
{
    if (width > static_cast<uint32_t>(std::numeric_limits<int>::max()) ||
        height > static_cast<uint32_t>(std::numeric_limits<int>::max()))
    {
        return false;
    }

    size_t requiredBytes = 0;
    if (!PixelPipeline::GetRgba8BufferSize(width, height, requiredBytes)) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_renderMutex);

    if (m_inRgbaBuffer.size() != requiredBytes) {
        m_inRgbaBuffer.resize(requiredBytes);
    }
    if (m_outRgbaBuffer.size() != requiredBytes) {
        m_outRgbaBuffer.resize(requiredBytes);
    }

    if (!PixelPipeline::ConvertToRgba8(
            inputPixels,
            width,
            height,
            inRowPitch,
            format,
            m_inRgbaBuffer.data(),
            m_inRgbaBuffer.size()))
    {
        return false;
    }

    auto& engine = DLSSHostEngine::Instance();
    if (!engine.Process(
            static_cast<int>(width),
            static_cast<int>(height),
            parameters,
            m_inRgbaBuffer.data(),
            m_outRgbaBuffer.data(),
            resetHistory))
    {
        LogRenderer("Neural processing failed");
        return false;
    }

    return PixelPipeline::WriteFromRgba8(
        inputPixels,
        m_outRgbaBuffer.data(),
        m_outRgbaBuffer.size(),
        outputPixels,
        width,
        height,
        inRowPitch,
        outRowPitch,
        format,
        parameters);
}

#include "AEConfig.h"

#ifdef AE_OS_WIN
    #include <Windows.h>
#endif

#include "entry.h"
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AE_Macros.h"
#include "Param_Utils.h"
#include "Smart_Utils.h"
#include "AEFX_SuiteHandlerTemplate.h"
#include "AEGP_SuiteHandler.h"
#include "PrSDKAESupport.h"

#include "PluginDef.h"
#include "DLSSNRRenderer.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <fstream>
#include <chrono>

static std::unique_ptr<DLSSNRRenderer> g_dlssRenderer;
static std::mutex g_pluginMutex;

struct TemporalRenderState {
    PF_ProgPtr effectRef = nullptr;
    A_long time = 0;
    A_long timeStep = 0;
    A_u_long timeScale = 0;
    bool valid = false;
};

static TemporalRenderState g_temporalState;

static void LogMessage(const std::string& msg) {
    OutputDebugStringA(("[DLSS_Plugin] " + msg + "\n").c_str());
    try {
        char localAppData[MAX_PATH];
        if (GetEnvironmentVariableA("LOCALAPPDATA", localAppData, MAX_PATH) > 0) {
            std::string dir = std::string(localAppData) + "\\DLSS_Neural_Video";
            CreateDirectoryA(dir.c_str(), nullptr);
            std::string logPath = dir + "\\plugin.log";
            std::ofstream logFile(logPath, std::ios::app);
            if (logFile.is_open()) {
                auto now = std::chrono::system_clock::now();
                auto in_time_t = std::chrono::system_clock::to_time_t(now);
                logFile << "[" << in_time_t << "] " << msg << std::endl;
            }
        }
    } catch (...) {}
}

template <typename Reader>
static PF_Err ReadParameter(PF_InData* in_data, A_long parameterId, Reader&& reader) {
    PF_ParamDef definition;
    AEFX_CLR_STRUCT(definition);

    PF_Err err = PF_CHECKOUT_PARAM(
        in_data,
        parameterId,
        in_data->current_time,
        in_data->time_step,
        in_data->time_scale,
        &definition);

    if (!err) {
        reader(definition);
    }

    const PF_Err checkinError = PF_CHECKIN_PARAM(in_data, &definition);
    return err ? err : checkinError;
}

static PF_Err ReadRenderParameters(PF_InData* in_data, DLSSParameters& parameters) {
    PF_Err err = PF_Err_NONE;

    ERR(ReadParameter(in_data, DLSS_ENABLE, [&](const PF_ParamDef& definition) {
        parameters.enabled = definition.u.bd.value != 0;
    }));
    ERR(ReadParameter(in_data, DLSS_STYLE, [&](const PF_ParamDef& definition) {
        parameters.style = definition.u.pd.value >= 1 ? definition.u.pd.value - 1 : 0;
    }));
    ERR(ReadParameter(in_data, DLSS_INTENSITY, [&](const PF_ParamDef& definition) {
        parameters.intensity = static_cast<float>(definition.u.fs_d.value);
    }));
    ERR(ReadParameter(in_data, DLSS_LOCAL_TONE, [&](const PF_ParamDef& definition) {
        parameters.localTone = static_cast<float>(definition.u.fs_d.value);
    }));
    ERR(ReadParameter(in_data, DLSS_LOCAL_STRUCTURE, [&](const PF_ParamDef& definition) {
        parameters.localStructure = static_cast<float>(definition.u.fs_d.value);
    }));
    ERR(ReadParameter(in_data, DLSS_SKIN_STRUCTURE, [&](const PF_ParamDef& definition) {
        parameters.skinStructure = static_cast<float>(definition.u.fs_d.value);
    }));
    ERR(ReadParameter(in_data, DLSS_OUTPUT_VIEW, [&](const PF_ParamDef& definition) {
        parameters.outputView = definition.u.pd.value;
    }));
    ERR(ReadParameter(in_data, DLSS_OUTPUT_MIX, [&](const PF_ParamDef& definition) {
        parameters.outputMix = static_cast<float>(definition.u.fs_d.value);
    }));

    return err;
}

static PF_Err GetHostColorFormat(
    PF_InData* in_data,
    PF_OutData* out_data,
    const PF_EffectWorld* world,
    HostColorFormat& format)
{
    AEFX_SuiteScoper<PF_WorldSuite2> worldSuite(
        in_data,
        kPFWorldSuite,
        kPFWorldSuiteVersion2,
        out_data);

    PF_PixelFormat pixelFormat = PF_PixelFormat_INVALID;
    PF_Err err = worldSuite->PF_GetPixelFormat(world, &pixelFormat);
    if (err) {
        return err;
    }

    switch (pixelFormat) {
        case PF_PixelFormat_ARGB32:
            format = HostColorFormat::AE_ARGB_8u;
            return PF_Err_NONE;
        case PF_PixelFormat_ARGB64:
            format = HostColorFormat::AE_ARGB_16u;
            return PF_Err_NONE;
        case PF_PixelFormat_ARGB128:
            format = HostColorFormat::AE_ARGB_32f;
            return PF_Err_NONE;
        case PF_PixelFormat_BGRA32:
            format = HostColorFormat::PR_BGRA_8u;
            return PF_Err_NONE;
        case PrPixelFormat_BGRA_4444_32f:
            format = HostColorFormat::PR_BGRA_32f;
            return PF_Err_NONE;
        default:
            LogMessage("Unsupported host pixel format: " + std::to_string(pixelFormat));
            return PF_Err_BAD_CALLBACK_PARAM;
    }
}

static bool IsSequentialFrame(const PF_InData* in_data) {
    if (!g_temporalState.valid ||
        g_temporalState.effectRef != in_data->effect_ref ||
        g_temporalState.timeScale != in_data->time_scale ||
        g_temporalState.timeStep != in_data->time_step ||
        in_data->time_step == 0)
    {
        return false;
    }

    const int64_t expectedTime =
        static_cast<int64_t>(g_temporalState.time) + static_cast<int64_t>(g_temporalState.timeStep);
    return expectedTime == static_cast<int64_t>(in_data->current_time);
}

static void UpdateTemporalState(const PF_InData* in_data, bool valid) {
    g_temporalState.effectRef = in_data->effect_ref;
    g_temporalState.time = in_data->current_time;
    g_temporalState.timeStep = in_data->time_step;
    g_temporalState.timeScale = in_data->time_scale;
    g_temporalState.valid = valid;
}

static bool RequiresNeuralProcessing(const DLSSParameters& parameters) {
    return parameters.enabled &&
        !(parameters.outputView == DLSS_VIEW_PROCESSED && parameters.outputMix <= 0.0f);
}

static bool ProcessWithRuntime(
    PF_InData* in_data,
    const PF_EffectWorld* input,
    PF_EffectWorld* output,
    HostColorFormat format,
    const DLSSParameters& parameters)
{
    if (input->width <= 0 || input->height <= 0 ||
        input->width != output->width || input->height != output->height)
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(g_pluginMutex);

    if (!g_dlssRenderer) {
        g_dlssRenderer = std::make_unique<DLSSNRRenderer>();
    }

    const bool resetHistory = !IsSequentialFrame(in_data);
    const bool processed = g_dlssRenderer->ProcessFrame(
        input->data,
        output->data,
        static_cast<uint32_t>(input->width),
        static_cast<uint32_t>(input->height),
        input->rowbytes,
        output->rowbytes,
        format,
        parameters,
        resetHistory);

    UpdateTemporalState(in_data, processed);
    return processed;
}

static PF_Err GlobalSetdown() {
    std::lock_guard<std::mutex> lock(g_pluginMutex);
    if (g_dlssRenderer) {
        g_dlssRenderer->Shutdown();
        g_dlssRenderer.reset();
    }
    g_temporalState = {};
    return PF_Err_NONE;
}

static PF_Err
About(
    PF_InData* in_data,
    PF_OutData* out_data,
    PF_ParamDef* params[],
    PF_LayerDef* output)
{
    static_cast<void>(in_data);
    static_cast<void>(params);
    static_cast<void>(output);
    PF_SPRINTF(
        out_data->return_msg,
        "%s v1.0.0\r\nNVIDIA DLSS 5 Neural Rendering Filter for After Effects & Premiere Pro.\r\nBased on TensorRT NGX Feature 18 Architecture.",
        PLUG_IN_NAME);
    return PF_Err_NONE;
}

static PF_Err
GlobalSetup(
    PF_InData* in_data,
    PF_OutData* out_data,
    PF_ParamDef* params[],
    PF_LayerDef* output)
{
    static_cast<void>(params);
    static_cast<void>(output);
    LogMessage("GlobalSetup called");
    PF_Err err = PF_Err_NONE;
    out_data->my_version = PF_VERSION(1, 0, 0, PF_Stage_RELEASE, 1);
    out_data->out_flags = PF_OutFlag_DEEP_COLOR_AWARE;
    out_data->out_flags2 = PF_OutFlag2_FLOAT_COLOR_AWARE | PF_OutFlag2_SUPPORTS_SMART_RENDER | PF_OutFlag2_SUPPORTS_THREADED_RENDERING;

    if (in_data->appl_id == kAppID_Premiere) {
        AEFX_SuiteScoper<PF_PixelFormatSuite1> pixelFormatSuite(
            in_data,
            kPFPixelFormatSuite,
            kPFPixelFormatSuiteVersion1,
            out_data
        );
        ERR(pixelFormatSuite->ClearSupportedPixelFormats(in_data->effect_ref));
        ERR(pixelFormatSuite->AddSupportedPixelFormat(in_data->effect_ref, PrPixelFormat_BGRA_4444_32f));
        ERR(pixelFormatSuite->AddSupportedPixelFormat(in_data->effect_ref, PrPixelFormat_BGRA_4444_8u));
    }

    return err;
}

static PF_Err
ParamsSetup(
    PF_InData* in_data,
    PF_OutData* out_data,
    PF_ParamDef* params[],
    PF_LayerDef* output)
{
    static_cast<void>(in_data);
    static_cast<void>(params);
    static_cast<void>(output);
    LogMessage("ParamsSetup called");
    PF_Err err = PF_Err_NONE;
    PF_ParamDef def;

    // 1. Enable DLSS checkbox (ID: DLSS_ENABLE = 1)
    AEFX_CLR_STRUCT(def);
    PF_ADD_CHECKBOX(
        "Enable DLSS Neural Rendering",
        "Enable",
        TRUE,
        0,
        DLSS_ENABLE
    );

    // 2. Style Popup (ID: DLSS_STYLE = 2) - Clean Universal ASCII
    AEFX_CLR_STRUCT(def);
    PF_ADD_POPUP(
        "Style",
        4,
        1,
        "Default|Natural|Cinema|Style 3",
        DLSS_STYLE
    );

    // 3. Intensity Slider (ID: DLSS_INTENSITY = 3)
    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX(
        "Intensity",
        0.0f,
        2.0f,
        0.0f,
        2.0f,
        1.0f,
        PF_Precision_HUNDREDTHS,
        0,
        0,
        DLSS_INTENSITY
    );

    // 4. Local Tone Slider (ID: DLSS_LOCAL_TONE = 4)
    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX(
        "Local Tone",
        0.0f,
        2.0f,
        0.0f,
        2.0f,
        1.0f,
        PF_Precision_HUNDREDTHS,
        0,
        0,
        DLSS_LOCAL_TONE
    );

    // 5. Local Structure Slider (ID: DLSS_LOCAL_STRUCTURE = 5)
    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX(
        "Local Structure",
        0.0f,
        2.0f,
        0.0f,
        2.0f,
        1.0f,
        PF_Precision_HUNDREDTHS,
        0,
        0,
        DLSS_LOCAL_STRUCTURE
    );

    // 6. Skin Structure Slider (ID: DLSS_SKIN_STRUCTURE = 6)
    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX(
        "Skin Structure",
        0.0f,
        2.0f,
        0.0f,
        2.0f,
        1.0f,
        PF_Precision_HUNDREDTHS,
        0,
        0,
        DLSS_SKIN_STRUCTURE
    );

    // 7. Output View Popup (ID: DLSS_OUTPUT_VIEW = 7)
    AEFX_CLR_STRUCT(def);
    PF_ADD_POPUP(
        "Output View",
        3,
        1,
        "Processed|Difference x10|Split Compare",
        DLSS_OUTPUT_VIEW
    );

    // 8. Output Mix Slider (ID: DLSS_OUTPUT_MIX = 8)
    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX(
        "Output Mix",
        0.0f,
        1.0f,
        0.0f,
        1.0f,
        1.0f,
        PF_Precision_HUNDREDTHS,
        0,
        0,
        DLSS_OUTPUT_MIX
    );

    out_data->num_params = DLSS_NUM_PARAMS;
    LogMessage("ParamsSetup completed, num_params = " + std::to_string(out_data->num_params));
    return err;
}

static PF_Err
PreRender(
    PF_InData* in_data,
    PF_OutData* out_data,
    PF_PreRenderExtra* extra)
{
    static_cast<void>(out_data);
    PF_Err err = PF_Err_NONE;
    PF_RenderRequest req = extra->input->output_request;
    req.preserve_rgb_of_zero_alpha = TRUE;

    PF_CheckoutResult in_result;
    AEFX_CLR_STRUCT(in_result);
    ERR(extra->cb->checkout_layer(
        in_data->effect_ref,
        DLSS_INPUT,
        DLSS_INPUT,
        &req,
        in_data->current_time,
        in_data->time_step,
        in_data->time_scale,
        &in_result
    ));

    if (!err) {
        UnionLRect(&in_result.result_rect, &extra->output->result_rect);
        UnionLRect(&in_result.max_result_rect, &extra->output->max_result_rect);
    }

    return err;
}

static PF_Err
SmartRender(
    PF_InData* in_data,
    PF_OutData* out_data,
    PF_SmartRenderExtra* extra)
{
    PF_Err err = PF_Err_NONE;
    PF_Err err2 = PF_Err_NONE;
    PF_EffectWorld* input_worldP = nullptr;
    PF_EffectWorld* output_worldP = nullptr;
    bool inputCheckedOut = false;

    ERR((extra->cb->checkout_layer_pixels(in_data->effect_ref, DLSS_INPUT, &input_worldP)));
    inputCheckedOut = !err;
    ERR(extra->cb->checkout_output(in_data->effect_ref, &output_worldP));

    if (!err) {
        if (!input_worldP || !output_worldP || !input_worldP->data || !output_worldP->data) {
            err = PF_Err_BAD_CALLBACK_PARAM;
        } else {
            try {
                HostColorFormat format = HostColorFormat::AE_ARGB_8u;
                DLSSParameters params;
                ERR(ReadRenderParameters(in_data, params));

                bool processed = false;
                if (!err && RequiresNeuralProcessing(params)) {
                    ERR(GetHostColorFormat(in_data, out_data, input_worldP, format));
                    if (!err) {
                        processed = ProcessWithRuntime(in_data, input_worldP, output_worldP, format, params);
                    }
                }

                if (!err && !processed) {
                    ERR(PF_COPY(input_worldP, output_worldP, nullptr, nullptr));
                }
            } catch (...) {
                LogMessage("Exception in SmartRender");
                err = PF_Err_INTERNAL_STRUCT_DAMAGED;
            }
        }
    }

    if (inputCheckedOut) {
        ERR2(extra->cb->checkin_layer_pixels(in_data->effect_ref, DLSS_INPUT));
    }

    return err;
}

static PF_Err
Render(
    PF_InData* in_data,
    PF_OutData* out_data,
    PF_ParamDef* params[],
    PF_LayerDef* output)
{
    static_cast<void>(out_data);
    PF_Err err = PF_Err_NONE;
    PF_EffectWorld* input_worldP = &params[DLSS_INPUT]->u.ld;
    PF_EffectWorld* output_worldP = output;

    if (input_worldP && output_worldP && input_worldP->data && output_worldP->data) {
        DLSSParameters dlssParams;
        dlssParams.enabled = params[DLSS_ENABLE]->u.bd.value != 0;
        dlssParams.style = (params[DLSS_STYLE]->u.pd.value >= 1) ? (params[DLSS_STYLE]->u.pd.value - 1) : 0;
        dlssParams.intensity = static_cast<float>(params[DLSS_INTENSITY]->u.fs_d.value);
        dlssParams.localTone = static_cast<float>(params[DLSS_LOCAL_TONE]->u.fs_d.value);
        dlssParams.localStructure = static_cast<float>(params[DLSS_LOCAL_STRUCTURE]->u.fs_d.value);
        dlssParams.skinStructure = static_cast<float>(params[DLSS_SKIN_STRUCTURE]->u.fs_d.value);
        dlssParams.outputView = params[DLSS_OUTPUT_VIEW]->u.pd.value;
        dlssParams.outputMix = static_cast<float>(params[DLSS_OUTPUT_MIX]->u.fs_d.value);

        bool processed = false;
        if (RequiresNeuralProcessing(dlssParams)) {
            HostColorFormat format = HostColorFormat::AE_ARGB_8u;
            ERR(GetHostColorFormat(in_data, out_data, input_worldP, format));
            if (!err) {
                processed = ProcessWithRuntime(in_data, input_worldP, output_worldP, format, dlssParams);
            }
        }

        if (!err && !processed) {
            ERR(PF_COPY(input_worldP, output_worldP, nullptr, nullptr));
        }
    }

    return err;
}

extern "C" DllExport
PF_Err PluginDataEntryFunction2(
    PF_PluginDataPtr inPtr,
    PF_PluginDataCB2 inPluginDataCallBackPtr,
    SPBasicSuite* inSPBasicSuitePtr,
    const char* inHostName,
    const char* inHostVersion)
{
    static_cast<void>(inSPBasicSuitePtr);
    static_cast<void>(inHostName);
    static_cast<void>(inHostVersion);
    PF_Err result = PF_Err_INVALID_CALLBACK;

    result = PF_REGISTER_EFFECT_EXT2(
        inPtr,
        inPluginDataCallBackPtr,
        PLUG_IN_NAME,
        PLUG_IN_MATCH_NAME,
        PLUG_IN_CATEGORY,
        AE_RESERVED_INFO,
        "EffectMain",
        "https://github.com/SAOG0721/DaVinci-Resolve-DLSS5");

    return result;
}

extern "C" DllExport
PF_Err EffectMain(
    PF_Cmd cmd,
    PF_InData* in_data,
    PF_OutData* out_data,
    PF_ParamDef* params[],
    PF_LayerDef* output,
    void* extra)
{
    PF_Err err = PF_Err_NONE;

    try {
        switch (cmd) {
            case PF_Cmd_ABOUT:
                err = About(in_data, out_data, params, output);
                break;
            case PF_Cmd_GLOBAL_SETUP:
                err = GlobalSetup(in_data, out_data, params, output);
                break;
            case PF_Cmd_GLOBAL_SETDOWN:
                err = GlobalSetdown();
                break;
            case PF_Cmd_PARAMS_SETUP:
                err = ParamsSetup(in_data, out_data, params, output);
                break;
            case PF_Cmd_RENDER:
                err = Render(in_data, out_data, params, output);
                break;
            case PF_Cmd_SMART_PRE_RENDER:
                err = PreRender(in_data, out_data, reinterpret_cast<PF_PreRenderExtra*>(extra));
                break;
            case PF_Cmd_SMART_RENDER:
                err = SmartRender(in_data, out_data, reinterpret_cast<PF_SmartRenderExtra*>(extra));
                break;
            default:
                break;
        }
    } catch (...) {
        LogMessage("Exception in EffectMain");
        err = PF_Err_INTERNAL_STRUCT_DAMAGED;
    }

    return err;
}

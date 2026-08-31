#pragma once

#ifndef PLUG_IN_NAME
#define PLUG_IN_NAME "DLSS Neural Video"
#endif

#ifndef PLUG_IN_MATCH_NAME
#define PLUG_IN_MATCH_NAME "ADBE DLSS_Neural_Video"
#endif

#ifndef PLUG_IN_CATEGORY
#define PLUG_IN_CATEGORY "DLSS Experimental"
#endif

// Parameter IDs
enum {
    DLSS_INPUT = 0,
    DLSS_ENABLE,
    DLSS_STYLE,
    DLSS_INTENSITY,
    DLSS_LOCAL_TONE,
    DLSS_LOCAL_STRUCTURE,
    DLSS_SKIN_STRUCTURE,
    DLSS_OUTPUT_VIEW,
    DLSS_OUTPUT_MIX,
    DLSS_NUM_PARAMS
};

// Style Choices
enum DLSSStyle {
    DLSS_STYLE_DEFAULT = 1,
    DLSS_STYLE_NATURAL,
    DLSS_STYLE_CINEMA,
    DLSS_STYLE_CUSTOM
};

// Output View Modes
enum DLSSOutputView {
    DLSS_VIEW_PROCESSED = 1,
    DLSS_VIEW_DIFF_10X,
    DLSS_VIEW_SPLIT_COMPARE
};

// Parameter Struct
struct DLSSParameters {
    bool enabled = true;
    int style = 0;
    float intensity = 1.0f;
    float localTone = 1.0f;
    float localStructure = 1.0f;
    float skinStructure = 1.0f;
    int outputView = DLSS_VIEW_PROCESSED;
    float outputMix = 1.0f;
};

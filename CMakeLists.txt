cmake_minimum_required(VERSION 3.25)
project(Adobe_DLSS5_Neural_Video
    VERSION 1.0.2
    DESCRIPTION "NVIDIA DLSS 5 Neural Rendering Plugin for Adobe After Effects & Premiere Pro"
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Output target suffix for Adobe Effect plugins
set(CMAKE_SHARED_LIBRARY_SUFFIX ".aex")

# SDK Paths
set(AE_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/Adobe_AE_SDK/Examples")
set(PIPL_TOOL "${AE_SDK_DIR}/Resources/PiPLtool.exe")

if(NOT EXISTS "${AE_SDK_DIR}/Headers/AE_Effect.h" OR NOT EXISTS "${PIPL_TOOL}")
    message(FATAL_ERROR
        "Adobe After Effects SDK not found. Extract the SDK so that "
        "third_party/Adobe_AE_SDK/Examples contains Headers and Resources.")
endif()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${AE_SDK_DIR}/Headers
    ${AE_SDK_DIR}/Headers/SP
    ${AE_SDK_DIR}/Headers/Win
    ${AE_SDK_DIR}/Resources
    ${AE_SDK_DIR}/Util
)

# Custom Command for Preprocessing and Compiling PiPL Resource
set(PIPL_R "${CMAKE_CURRENT_SOURCE_DIR}/src/DLSSNeuralVideoPiPL.r")
set(PIPL_RR "${CMAKE_CURRENT_BINARY_DIR}/DLSSNeuralVideoPiPL.rr")
set(PIPL_RRC "${CMAKE_CURRENT_BINARY_DIR}/DLSSNeuralVideoPiPL.rrc")
set(PIPL_RC "${CMAKE_CURRENT_SOURCE_DIR}/src/DLSSNeuralVideo.rc")

add_custom_command(
    OUTPUT ${PIPL_RRC}
    COMMAND ${CMAKE_CXX_COMPILER} /nologo /EP /DMSWindows /D_WINDOWS /DWIN32
        /I "${AE_SDK_DIR}/Headers"
        /I "${AE_SDK_DIR}/Headers/Win"
        /I "${AE_SDK_DIR}/Resources"
        "${PIPL_R}" > "${PIPL_RR}"
    COMMAND "${PIPL_TOOL}" "${PIPL_RR}" "${PIPL_RRC}"
    DEPENDS "${PIPL_R}"
    COMMENT "Compiling Adobe PiPL Resource: DLSSNeuralVideoPiPL.r"
)

set_source_files_properties(${PIPL_RC} PROPERTIES
    COMPILE_FLAGS "/i \"${CMAKE_CURRENT_BINARY_DIR}\" /i \"${AE_SDK_DIR}/Resources\""
    OBJECT_DEPENDS "${PIPL_RRC}"
)

# Source files
set(PLUGIN_SOURCES
    src/PluginMain.cpp
    src/DLSSNRRenderer.cpp
    src/PixelPipeline.cpp
    ${PIPL_RC}
    ${AE_SDK_DIR}/Util/AEGP_SuiteHandler.cpp
    ${AE_SDK_DIR}/Util/MissingSuiteError.cpp
    ${AE_SDK_DIR}/Util/Smart_Utils.cpp
)

add_library(DLSS_Neural_Video SHARED
    ${PLUGIN_SOURCES}
)

# Set output name
set_target_properties(DLSS_Neural_Video PROPERTIES
    OUTPUT_NAME "DLSS_Neural_Video"
    PREFIX ""
)

# MSVC Compiler Options and Optimization Flags
if(MSVC)
    target_compile_definitions(DLSS_Neural_Video PRIVATE
        _WINDOWS
        WIN32
        MSWindows
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
    target_compile_options(DLSS_Neural_Video PRIVATE
        /W4
        /MP
        /EHsc
        /openmp
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:Release>:/Oi>
        $<$<CONFIG:Release>:/Ot>
        $<$<CONFIG:Release>:/GL>
    )
    target_link_options(DLSS_Neural_Video PRIVATE
        $<$<CONFIG:Release>:/LTCG>
        $<$<CONFIG:Release>:/OPT:REF>
        $<$<CONFIG:Release>:/OPT:ICF>
    )
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(PixelPipelineTests
        tests/PixelPipelineTests.cpp
        src/PixelPipeline.cpp
    )
    target_include_directories(PixelPipelineTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    if(MSVC)
        target_compile_options(PixelPipelineTests PRIVATE /W4 /EHsc /openmp)
    endif()
    add_test(NAME PixelPipelineTests COMMAND PixelPipelineTests)
endif()

message(STATUS "Configured ${PROJECT_NAME} (DLSS_Neural_Video.aex) targeting Windows x64")

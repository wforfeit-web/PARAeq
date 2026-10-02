cmake_minimum_required(VERSION 3.22)
project(ParaEQ VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build a universal binary on Mac (Apple Silicon + Intel)
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "" FORCE)
endif()

# Use a local JUCE folder if present, otherwise download it automatically
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# On Mac, auto-copy the built plugin into ~/Library/Audio/Plug-Ins
if(APPLE)
    set(PARAEQ_COPY TRUE)
else()
    set(PARAEQ_COPY FALSE)
endif()

juce_add_plugin(ParaEQ
    COMPANY_NAME "ParaEQ"
    PLUGIN_MANUFACTURER_CODE Prqq
    PLUGIN_CODE Peq1
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "ParaEQ"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx EQ
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD ${PARAEQ_COPY})

target_sources(ParaEQ PRIVATE
    PluginProcessor.cpp
    PluginEditor.cpp)

target_compile_definitions(ParaEQ PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(ParaEQ
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

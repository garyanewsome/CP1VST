cmake_minimum_required(VERSION 3.15)

# Build a universal binary (Intel + Apple Silicon) on macOS. This must be set
# before project() — CMake locks in the target architecture(s) at that point,
# so setting it later has no effect on macOS.
set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "Target architectures for macOS" FORCE)
set(CMAKE_OSX_DEPLOYMENT_TARGET "14.0" CACHE STRING "Target macOS deployment version" FORCE)

project(CP1 VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    juce
    URL https://github.com/juce-framework/JUCE/archive/refs/tags/8.0.4.zip
)
FetchContent_MakeAvailable(juce)

juce_add_plugin(CP1
    COMPANY_NAME "Gary A. Newsome"
    COMPANY_COPYRIGHT "Gary A. Newsome"
    BUNDLE_ID "com.garyanewsome.CP1"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE "Gary"
    PLUGIN_CODE "CP1 "
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "CP1"
)

juce_generate_juce_header(CP1)

target_sources(CP1
    PRIVATE
        PluginProcessor.cpp
        PluginProcessor.h
        PluginEditor.cpp
        PluginEditor.h
        CyberLookAndFeel.h
        Presets.h
)

target_compile_definitions(CP1
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(CP1
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

cmake_minimum_required(VERSION 3.22)

project(ARMONIO VERSION 0.0.1)

add_subdirectory(JUCE)

juce_add_plugin(Armonio
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        FORMATS VST3 Standalone
        COPY_PLUGIN_AFTER_BUILD TRUE
        PRODUCT_NAME "Armonio")

target_sources(Armonio
        PRIVATE
        PluginEditor.cpp
        PluginEditor.h
        PluginProcessor.cpp
        PluginProcessor.h
        SynthAudioSource.cpp
        SynthAudioSource.h
        WaveformGenerator.h
        WavetableOscillator.h
        WavetableSound.h)

target_compile_definitions(Armonio
        PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Armonio
        PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_devices
        PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
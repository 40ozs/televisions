option(VFA_ENABLE_VST3 "Build the VST3 target" ON)
option(VFA_ENABLE_STANDALONE "Build the Standalone target" ON)
option(VFA_ENABLE_AAX "Build the AAX target (requires AAX_SDK_PATH)" OFF)
set(AAX_SDK_PATH "" CACHE PATH "Path to the AAX SDK (required when VFA_ENABLE_AAX is ON)")

set(VFA_FORMATS "")
if(VFA_ENABLE_VST3)
    list(APPEND VFA_FORMATS VST3)
endif()
if(APPLE)
    list(APPEND VFA_FORMATS AU)
    message(STATUS "VFA: AU enabled (macOS toolchain detected)")
else()
    message(STATUS "VFA: AU BLOCKED — requires a macOS toolchain; it is enabled automatically when configuring on macOS")
endif()
if(VFA_ENABLE_AAX)
    if(AAX_SDK_PATH AND EXISTS "${AAX_SDK_PATH}/Interfaces")
        juce_set_aax_sdk_path("${AAX_SDK_PATH}")
        list(APPEND VFA_FORMATS AAX)
        message(STATUS "VFA: AAX enabled with SDK at ${AAX_SDK_PATH}")
    else()
        message(FATAL_ERROR "VFA: AAX requested but AAX_SDK_PATH does not point at an AAX SDK")
    endif()
else()
    message(STATUS "VFA: AAX BLOCKED — SDK not present on this machine. Enable later with -DVFA_ENABLE_AAX=ON -DAAX_SDK_PATH=<path>")
endif()
if(VFA_ENABLE_STANDALONE)
    list(APPEND VFA_FORMATS Standalone)
endif()

# ---------------------------------------------------------------- DSP sources
# Framework-independent DSP core (vfa_dsp). Compiled twice by design:
# once into the standalone static library used by headless DSP tests, and
# once into the plugin shared-code target. They are never linked together,
# which avoids duplicate JUCE module objects (see ADR-002 / ADR-014).
set(VFA_DSP_SOURCES
    Source/DSP/Engine.cpp
    Source/DSP/DeliveryCurves/DeliveryCurveModule.cpp
    Source/DSP/Optical/OpticalModel.cpp
    Source/DSP/Magnetic/MagneticModel.cpp
    Source/DSP/Broadcast/BroadcastChain.cpp
    Source/DSP/Transport/WowFlutterEngine.cpp
    Source/DSP/GenerationLoss/GenerationModel.cpp
    Source/DSP/Noise/NoiseArtifactEngine.cpp
    Source/DSP/Dynamics/PeriodDynamics.cpp
    Source/DSP/Reproduction/Reproduction.cpp
)

set(VFA_PLUGIN_SOURCES
    Source/Plugin/PluginProcessor.cpp
    Source/Plugin/PluginEditor.cpp
    Source/Parameters/ParameterLayout.cpp
    Source/Parameters/EraProfiles.cpp
    Source/Presets/FactoryPresets.cpp
)

# ------------------------------------------------------------------- vfa_dsp
add_library(vfa_dsp STATIC ${VFA_DSP_SOURCES})
target_include_directories(vfa_dsp PUBLIC Source)
target_compile_definitions(vfa_dsp
    PRIVATE JUCE_STANDALONE_APPLICATION=1
    PUBLIC
        JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
        JUCE_MODAL_LOOPS_PERMITTED=0
        VFA_REPO_ROOT="${CMAKE_SOURCE_DIR}"
)
target_link_libraries(vfa_dsp
    PRIVATE
        juce::juce_dsp
        juce::juce_recommended_config_flags
)
# Propagate JUCE header paths/definitions to test executables without
# propagating the module *sources* (INTERFACE_SOURCES stay private).
target_include_directories(vfa_dsp PUBLIC "${CMAKE_SOURCE_DIR}/external/JUCE/modules")
target_compile_definitions(vfa_dsp PUBLIC
    JUCE_MODULE_AVAILABLE_juce_core=1
    JUCE_MODULE_AVAILABLE_juce_events=1
    JUCE_MODULE_AVAILABLE_juce_audio_basics=1
    JUCE_MODULE_AVAILABLE_juce_audio_formats=1
    JUCE_MODULE_AVAILABLE_juce_dsp=1
    $<$<CONFIG:Debug>:DEBUG=1 _DEBUG=1>
    $<$<NOT:$<CONFIG:Debug>>:NDEBUG=1>
)

# -------------------------------------------------------------------- Plugin
juce_add_plugin(VintageFilmAudio
    COMPANY_NAME "Forty Ounce Audio"
    BUNDLE_ID com.fortyounceaudio.vintagefilmaudio
    PLUGIN_MANUFACTURER_CODE Foza
    PLUGIN_CODE Vfta
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    FORMATS ${VFA_FORMATS}
    PRODUCT_NAME "Vintage Film and TV"
    VST3_CATEGORIES Fx Restoration
    AU_MAIN_TYPE kAudioUnitType_Effect
)

target_sources(VintageFilmAudio PRIVATE ${VFA_PLUGIN_SOURCES} ${VFA_DSP_SOURCES})
target_include_directories(VintageFilmAudio PRIVATE Source)
target_compile_definitions(VintageFilmAudio
    PUBLIC
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_MODAL_LOOPS_PERMITTED=0
        JUCE_DISPLAY_SPLASH_SCREEN=1
        VFA_REPO_ROOT="${CMAKE_SOURCE_DIR}"
)
target_link_libraries(VintageFilmAudio
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
)

# Project-code warnings (not applied to JUCE module sources, which are
# compiled through their own interface targets).
set(VFA_WARNINGS -Wall -Wextra -Wshadow -Wno-unused-parameter)

# --------------------------------------------------------------------- Tests
add_executable(VfaDspTests
    Tests/Harness/TestMain.cpp
    Tests/DSP/RngTest.cpp
    Tests/DSP/DeliveryCurveTest.cpp
    Tests/DSP/OpticalDistinctnessTest.cpp
    Tests/DSP/MediumTest.cpp
    Tests/DSP/WowFlutterTest.cpp
    Tests/DSP/GenerationTest.cpp
    Tests/DSP/NoiseTest.cpp
    Tests/DSP/BroadcastTest.cpp
    Tests/Performance/PerfTest.cpp
)
target_include_directories(VfaDspTests PRIVATE Tests)
target_link_libraries(VfaDspTests PRIVATE vfa_dsp)
target_compile_definitions(VfaDspTests PRIVATE JUCE_STANDALONE_APPLICATION=1)

add_executable(VfaPluginTests
    Tests/Harness/TestMain.cpp
    Tests/Unit/ParameterManifestTest.cpp
    Tests/Unit/MacroTest.cpp
    Tests/Unit/GainStagingTest.cpp
    Tests/Unit/LayoutTest.cpp
    Tests/State/SaveLoadTest.cpp
    Tests/State/PresetTest.cpp
    Tests/State/MigrationTest.cpp
    Tests/State/EraProfileTest.cpp
    Tests/Realtime/AllocationTest.cpp
    Tests/Realtime/StressTest.cpp
    Tests/Realtime/ClickTest.cpp
)
target_include_directories(VfaPluginTests PRIVATE Tests Source
    "${CMAKE_SOURCE_DIR}/external/JUCE/modules")
target_link_libraries(VfaPluginTests PRIVATE VintageFilmAudio)
# The plugin links JUCE modules privately; the test executable only needs the
# headers (objects come from the plugin static lib). Keep definitions in sync
# with the plugin's PUBLIC ones (propagated via the link above).
target_compile_definitions(VfaPluginTests PRIVATE
    JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
    JUCE_MODULE_AVAILABLE_juce_core=1
    JUCE_MODULE_AVAILABLE_juce_events=1
    JUCE_MODULE_AVAILABLE_juce_data_structures=1
    JUCE_MODULE_AVAILABLE_juce_audio_basics=1
    JUCE_MODULE_AVAILABLE_juce_audio_formats=1
    JUCE_MODULE_AVAILABLE_juce_audio_processors=1
    JUCE_MODULE_AVAILABLE_juce_audio_utils=1
    JUCE_MODULE_AVAILABLE_juce_gui_basics=1
    JUCE_MODULE_AVAILABLE_juce_gui_extra=1
    JUCE_MODULE_AVAILABLE_juce_graphics=1
    JUCE_MODULE_AVAILABLE_juce_dsp=1
    JUCE_STANDALONE_APPLICATION=1
)

foreach(t IN ITEMS VfaDspTests VfaPluginTests)
    target_compile_options(${t} PRIVATE ${VFA_WARNINGS})
endforeach()

add_test(NAME DspSuite COMMAND VfaDspTests)
add_test(NAME PluginSuite COMMAND VfaPluginTests)
set_tests_properties(DspSuite PluginSuite PROPERTIES TIMEOUT 1800)

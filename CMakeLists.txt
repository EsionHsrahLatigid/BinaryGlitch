cmake_minimum_required(VERSION 3.22)

project(BinaryGlitch VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BINARYGLITCH_BUILD_PLUGIN "Build the JUCE plug-in targets" ON)
option(BINARYGLITCH_BUILD_TESTS "Build BinaryGlitch tests" ON)
set(BINARYGLITCH_JUCE_PATH "" CACHE PATH "Path to a local JUCE checkout. If empty, FetchContent downloads JUCE.")
set(BINARYGLITCH_ARTIFACT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/artifacts" CACHE PATH
    "Directory for final VST3, AU, and Standalone products")

if(BINARYGLITCH_BUILD_PLUGIN)
    if(BINARYGLITCH_JUCE_PATH)
        add_subdirectory("${BINARYGLITCH_JUCE_PATH}" "${CMAKE_CURRENT_BINARY_DIR}/JUCE")
    else()
        include(FetchContent)
        FetchContent_Declare(
            JUCE
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG 8.0.15
            GIT_SHALLOW TRUE)
        FetchContent_MakeAvailable(JUCE)
    endif()

    set(BINARYGLITCH_FORMATS VST3 Standalone)
    if(APPLE)
        list(APPEND BINARYGLITCH_FORMATS AU)
    endif()

    juce_add_plugin(BinaryGlitch
        VERSION ${PROJECT_VERSION}
        COMPANY_NAME "EsionHsrahLatigid"
        BUNDLE_ID "jp.ehl.binaryglitch"
        PRODUCT_NAME "BinaryGlitch"
        DESCRIPTION "Binary buffer fault texture audio effect"
        PLUGIN_MANUFACTURER_CODE EHL_
        PLUGIN_CODE Bgch
        FORMATS ${BINARYGLITCH_FORMATS}
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        VST3_CATEGORIES "Fx" "Distortion"
        AU_MAIN_TYPE kAudioUnitType_Effect
        COPY_PLUGIN_AFTER_BUILD FALSE)

    target_sources(BinaryGlitch PRIVATE
        Source/Dsp/ByteSource.h
        Source/Dsp/Comb.h
        Source/Dsp/FaultEngine.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h)

    target_include_directories(BinaryGlitch PRIVATE Source)
    target_link_libraries(BinaryGlitch
        PRIVATE
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    target_compile_definitions(BinaryGlitch PUBLIC
        JUCE_REPORT_APP_USAGE=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_WEB_BROWSER=0)

    juce_generate_juce_header(BinaryGlitch)

    set(binaryglitch_juce_products "${CMAKE_CURRENT_BINARY_DIR}/BinaryGlitch_artefacts/$<CONFIG>")
    set(binaryglitch_staged_products "${BINARYGLITCH_ARTIFACT_DIR}/$<CONFIG>")
    set(binaryglitch_artifact_dependencies BinaryGlitch_VST3 BinaryGlitch_Standalone)
    set(binaryglitch_artifact_commands
        COMMAND ${CMAKE_COMMAND} -E make_directory "${binaryglitch_staged_products}"
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${binaryglitch_staged_products}/VST3"
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${binaryglitch_staged_products}/Standalone"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${binaryglitch_juce_products}/VST3" "${binaryglitch_staged_products}/VST3"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${binaryglitch_juce_products}/Standalone" "${binaryglitch_staged_products}/Standalone")

    if(TARGET BinaryGlitch_AU)
        list(APPEND binaryglitch_artifact_dependencies BinaryGlitch_AU)
        list(APPEND binaryglitch_artifact_commands
            COMMAND ${CMAKE_COMMAND} -E remove_directory "${binaryglitch_staged_products}/AU"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${binaryglitch_juce_products}/AU" "${binaryglitch_staged_products}/AU")
    endif()

    if(APPLE)
        list(APPEND binaryglitch_artifact_commands
            COMMAND codesign --force --deep --sign -
                    "${binaryglitch_staged_products}/VST3/BinaryGlitch.vst3"
            COMMAND codesign --force --deep --sign -
                    "${binaryglitch_staged_products}/Standalone/BinaryGlitch.app"
            COMMAND codesign --force --deep --sign -
                    "${binaryglitch_staged_products}/AU/BinaryGlitch.component")
    endif()

    add_custom_target(BinaryGlitch_Artifacts
        ${binaryglitch_artifact_commands}
        DEPENDS ${binaryglitch_artifact_dependencies}
        COMMENT "Staging BinaryGlitch products in ${BINARYGLITCH_ARTIFACT_DIR}/$<CONFIG>")

    if(BINARYGLITCH_BUILD_TESTS)
        enable_testing()
        add_executable(BinaryGlitchPluginSmokeTests Tests/PluginSmokeTests.cpp)
        target_include_directories(BinaryGlitchPluginSmokeTests PRIVATE
            Source
            "${CMAKE_CURRENT_BINARY_DIR}/BinaryGlitch_artefacts/JuceLibraryCode")
        target_link_libraries(BinaryGlitchPluginSmokeTests PRIVATE BinaryGlitch juce::juce_audio_utils)
        add_test(NAME BinaryGlitchPluginSmokeTests COMMAND BinaryGlitchPluginSmokeTests)
    endif()
endif()

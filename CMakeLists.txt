cmake_minimum_required(VERSION 3.22)

project(BinaryGlitch VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BINARYGLITCH_BUILD_PLUGIN "Build the JUCE plug-in targets" ON)
option(BINARYGLITCH_BUILD_TESTS "Build BinaryGlitch tests" ON)
set(BINARYGLITCH_JUCE_PATH "" CACHE PATH "Path to a local JUCE checkout. If empty, FetchContent downloads JUCE.")
set(EHL_JUCE_SOURCE_DIR "" CACHE PATH "Existing JUCE source checkout; avoids network when set")
set(BINARYGLITCH_ARTIFACT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/artifacts" CACHE PATH "Stable product staging root")

if(BINARYGLITCH_BUILD_PLUGIN)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)

    if(EHL_JUCE_SOURCE_DIR)
        if(NOT EXISTS "${EHL_JUCE_SOURCE_DIR}/CMakeLists.txt")
            message(FATAL_ERROR "EHL_JUCE_SOURCE_DIR does not contain JUCE: ${EHL_JUCE_SOURCE_DIR}")
        endif()
        add_subdirectory("${EHL_JUCE_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/juce")
    elseif(BINARYGLITCH_JUCE_PATH)
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

    set(EHL_JUCE_DESIGN_BUILD_TESTS OFF CACHE BOOL "Build EHL JUCE design module tests" FORCE)
    add_subdirectory(modules/juce-ehl-design-module)

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
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
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
            EHL::JuceDesign
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

    set(ehl_stage_platform "linux-x64")
    if(APPLE)
        set(ehl_stage_platform "macos-arm64")
    elseif(WIN32)
        set(ehl_stage_platform "windows-x64")
    endif()
    set(binaryglitch_juce_products "${CMAKE_CURRENT_BINARY_DIR}/BinaryGlitch_artefacts/$<CONFIG>")
    set(binaryglitch_staged_products "${BINARYGLITCH_ARTIFACT_ROOT}/plugin-release/${ehl_stage_platform}")
    set(binaryglitch_artifact_dependencies BinaryGlitch_VST3 BinaryGlitch_Standalone)
    set(binaryglitch_artifact_commands
        COMMAND ${CMAKE_COMMAND} -E make_directory "${binaryglitch_staged_products}/vst3"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${binaryglitch_staged_products}/standalone"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${binaryglitch_staged_products}/au")

    if(APPLE)
        list(APPEND binaryglitch_artifact_dependencies BinaryGlitch_AU)
        list(APPEND binaryglitch_artifact_commands
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${binaryglitch_staged_products}/vst3/binaryglitch_vst3_plugin.vst3"
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${binaryglitch_staged_products}/standalone/binaryglitch_standalone_plugin.app"
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${binaryglitch_staged_products}/au/binaryglitch_au_plugin.component"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${binaryglitch_juce_products}/VST3/BinaryGlitch.vst3" "${binaryglitch_staged_products}/vst3/binaryglitch_vst3_plugin.vst3"
            COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/NormalizeVst3ModuleInfo.py" "${binaryglitch_staged_products}/vst3/binaryglitch_vst3_plugin.vst3/Contents/Resources/moduleinfo.json"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${binaryglitch_juce_products}/Standalone/BinaryGlitch.app" "${binaryglitch_staged_products}/standalone/binaryglitch_standalone_plugin.app"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${binaryglitch_juce_products}/AU/BinaryGlitch.component" "${binaryglitch_staged_products}/au/binaryglitch_au_plugin.component"
            COMMAND codesign --force --deep --sign - "${binaryglitch_staged_products}/vst3/binaryglitch_vst3_plugin.vst3"
            COMMAND codesign --force --deep --sign - "${binaryglitch_staged_products}/standalone/binaryglitch_standalone_plugin.app"
            COMMAND codesign --force --deep --sign - "${binaryglitch_staged_products}/au/binaryglitch_au_plugin.component")
    elseif(WIN32)
        list(APPEND binaryglitch_artifact_commands
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${binaryglitch_staged_products}/vst3/binaryglitch_vst3_plugin.vst3"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${binaryglitch_juce_products}/VST3/BinaryGlitch.vst3" "${binaryglitch_staged_products}/vst3/binaryglitch_vst3_plugin.vst3"
            COMMAND ${CMAKE_COMMAND} -E copy "${binaryglitch_juce_products}/Standalone/BinaryGlitch.exe" "${binaryglitch_staged_products}/standalone/binaryglitch_standalone_plugin.exe")
    else()
        list(APPEND binaryglitch_artifact_commands
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${binaryglitch_juce_products}/VST3/BinaryGlitch.vst3" "${binaryglitch_staged_products}/vst3/binaryglitch_vst3_plugin.vst3"
            COMMAND ${CMAKE_COMMAND} -E copy "${binaryglitch_juce_products}/Standalone/BinaryGlitch" "${binaryglitch_staged_products}/standalone/binaryglitch_standalone_plugin")
    endif()

    list(APPEND binaryglitch_artifact_commands
        COMMAND ${CMAKE_COMMAND}
            -DPRODUCT_NAME=BinaryGlitch
            -DSLUG=binaryglitch
            -DBUNDLE_ID=jp.ehl.binaryglitch
            -DPLUGIN_CODE=Bgch
            -DSTAGE_DIR=${binaryglitch_staged_products}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/WriteArtifactsManifest.cmake")

    add_custom_target(ehl_stage_products
        ${binaryglitch_artifact_commands}
        DEPENDS ${binaryglitch_artifact_dependencies}
        COMMENT "Staging BinaryGlitch products under ${BINARYGLITCH_ARTIFACT_ROOT}/plugin-release")

    if(BINARYGLITCH_BUILD_TESTS)
        enable_testing()
        add_executable(BinaryGlitchPluginSmokeTests Tests/PluginSmokeTests.cpp)
        target_include_directories(BinaryGlitchPluginSmokeTests PRIVATE
            Source
            "${CMAKE_CURRENT_BINARY_DIR}/BinaryGlitch_artefacts/JuceLibraryCode")
        target_link_libraries(BinaryGlitchPluginSmokeTests PRIVATE BinaryGlitch juce::juce_audio_utils)
        add_test(NAME BinaryGlitchPluginSmokeTests COMMAND BinaryGlitchPluginSmokeTests)
        add_executable(BinaryGlitchEditorTests Tests/EditorTests.cpp)
        target_include_directories(BinaryGlitchEditorTests PRIVATE
            Source
            "${CMAKE_CURRENT_BINARY_DIR}/BinaryGlitch_artefacts/JuceLibraryCode")
        target_link_libraries(BinaryGlitchEditorTests PRIVATE BinaryGlitch EHL::JuceDesign juce::juce_audio_utils)
        add_test(NAME BinaryGlitchEditorTests COMMAND BinaryGlitchEditorTests)
        add_test(
            NAME BinaryGlitchArtifactChecks
            COMMAND ${CMAKE_COMMAND}
                    -DSTAGE_DIR=${binaryglitch_staged_products}
                    -DSLUG=binaryglitch
                    -DEXPECT_AU=$<BOOL:${APPLE}>
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/Tests/VerifyArtifacts.cmake)
    endif()
endif()

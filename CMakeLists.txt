cmake_minimum_required(VERSION 3.22)

project(PPsVaultTracker VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE 8, pinned release, fetched at configure time (nothing vendored, AGPLv3)
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.14
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_gui_app(PPsVaultTracker
    PRODUCT_NAME "PPsVaultTracker"
    COMPANY_NAME "TheUNBoRN"
    VERSION      ${PROJECT_VERSION})

juce_generate_juce_header(PPsVaultTracker)

target_sources(PPsVaultTracker PRIVATE
    src/Main.cpp
    src/engine/HostEngine.cpp
    src/sequencer/SequencerNode.cpp
    src/ui/MainComponent.cpp
    src/ui/MixerView.cpp
    src/ui/PatternEditor.cpp)

target_include_directories(PPsVaultTracker PRIVATE src)

target_compile_definitions(PPsVaultTracker PRIVATE
    JUCE_PLUGINHOST_VST3=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:PPsVaultTracker,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:PPsVaultTracker,JUCE_VERSION>")

target_link_libraries(PPsVaultTracker PRIVATE
    juce::juce_audio_utils
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

enable_testing()
add_subdirectory(tests)

cmake_minimum_required(VERSION 3.22)

project(PPsVaultTracker VERSION 1.0.0 LANGUAGES C CXX)

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

# embedded assets: Orbitron font (SIL OFL 1.1, see THIRD_PARTY_LICENSES.md)
juce_add_binary_data(Assets SOURCES
    assets/OrbitronMedium.otf
    assets/OrbitronBold.otf)

juce_generate_juce_header(PPsVaultTracker)

target_sources(PPsVaultTracker PRIVATE
    src/Main.cpp
    src/engine/HostEngine.cpp
    src/io/MidiExport.cpp
    src/io/ModImport.cpp
    src/io/ProjectIO.cpp
    src/sequencer/SequencerNode.cpp
    src/ui/ArrangementView.cpp
    src/ui/CcLanePanel.cpp
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
    Assets
    juce::juce_audio_utils
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

# Optional libopenmpt (module import, BSD-3-Clause) — never vendored.
#   Windows: -DPVT_LIBOPENMPT_DIR=<official devel package root (inc/, lib/amd64/, bin/amd64/)>
#   Linux:   -DPVT_LIBOPENMPT_SYSTEM=ON (apt install libopenmpt-dev)
set(PVT_LIBOPENMPT_DIR "" CACHE PATH "libopenmpt Windows devel package root")
option(PVT_LIBOPENMPT_SYSTEM "Link the system libopenmpt (Linux)" OFF)

if(PVT_LIBOPENMPT_SYSTEM)
    target_compile_definitions(PPsVaultTracker PRIVATE PVT_HAS_LIBOPENMPT=1)
    target_link_libraries(PPsVaultTracker PRIVATE openmpt)
elseif(PVT_LIBOPENMPT_DIR)
    if(NOT EXISTS "${PVT_LIBOPENMPT_DIR}/inc/libopenmpt/libopenmpt.hpp")
        message(FATAL_ERROR "PVT_LIBOPENMPT_DIR does not look like the libopenmpt devel package")
    endif()
    target_compile_definitions(PPsVaultTracker PRIVATE PVT_HAS_LIBOPENMPT=1)
    target_include_directories(PPsVaultTracker PRIVATE "${PVT_LIBOPENMPT_DIR}/inc")
    target_link_libraries(PPsVaultTracker PRIVATE "${PVT_LIBOPENMPT_DIR}/lib/amd64/libopenmpt.lib")
    add_custom_command(TARGET PPsVaultTracker POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${PVT_LIBOPENMPT_DIR}/bin/amd64"
            "$<TARGET_FILE_DIR:PPsVaultTracker>")
endif()

enable_testing()
add_subdirectory(tests)

# JUCE-free unit tests: model + tracker clock (fast to build, run in CI).
add_executable(sanity_tests SanityTests.cpp)
set_target_properties(sanity_tests PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
add_test(NAME sanity COMMAND sanity_tests)

add_executable(model_tests ModelTests.cpp)
target_include_directories(model_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(model_tests PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
add_test(NAME model COMMAND model_tests)

add_executable(edit_tests EditTests.cpp)
target_include_directories(edit_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(edit_tests PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
add_test(NAME edit COMMAND edit_tests)

add_executable(mix_tests MixTests.cpp)
target_include_directories(mix_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(mix_tests PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
add_test(NAME mix COMMAND mix_tests)

# io_tests links juce_core + juce_audio_basics (var/JSON + MidiFile) — a real
# console app so JUCE gets its platform glue
juce_add_console_app(io_tests PRODUCT_NAME "io_tests")
target_sources(io_tests PRIVATE IoTests.cpp
    ${CMAKE_SOURCE_DIR}/src/io/ProjectIO.cpp
    ${CMAKE_SOURCE_DIR}/src/io/MidiExport.cpp)
target_include_directories(io_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(io_tests PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
target_link_libraries(io_tests PRIVATE juce::juce_core juce::juce_audio_basics
    juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
set_target_properties(io_tests PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
add_test(NAME io COMMAND io_tests)

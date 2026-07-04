# JUCE-free unit tests: model + tracker clock (fast to build, run in CI).
add_executable(sanity_tests SanityTests.cpp)
set_target_properties(sanity_tests PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
add_test(NAME sanity COMMAND sanity_tests)

add_executable(model_tests ModelTests.cpp)
target_include_directories(model_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(model_tests PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
add_test(NAME model COMMAND model_tests)

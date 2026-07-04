# Phase-1 skeleton: a trivial sanity target proving the CTest wiring works.
# Real unit tests (model, serialisation, sequencer) arrive with phase 2.
add_executable(sanity_tests SanityTests.cpp)
set_target_properties(sanity_tests PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
add_test(NAME sanity COMMAND sanity_tests)

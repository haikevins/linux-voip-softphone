add_executable(unit_tests
    unit/test_build_info.cpp
    unit/test_call_state.cpp
    unit/test_call_state_machine.cpp
    unit/test_unique_fd.cpp
)

target_link_libraries(unit_tests
    PRIVATE
        voip::core
        GTest::gtest_main
)

project_set_warnings(unit_tests)
project_enable_sanitizers(unit_tests)

include(GoogleTest)
gtest_discover_tests(unit_tests)

add_executable(unit_tests
    unit/test_build_info.cpp
    unit/test_call_state.cpp
    unit/test_call_state_machine.cpp
    unit/test_unique_fd.cpp
    unit/test_unix_domain_socket_client.cpp
    unit/test_unix_domain_socket_server.cpp
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

add_test(
    NAME LinuxProcessLifecycleTest
    COMMAND lab_process_lifecycle --self-test
)
set_tests_properties(LinuxProcessLifecycleTest PROPERTIES LABELS "linux;process")

add_test(
    NAME LinuxExecChildReplacementTest
    COMMAND lab_exec_child --self-test
)
set_tests_properties(LinuxExecChildReplacementTest PROPERTIES LABELS "linux;process;exec")

add_test(
    NAME LinuxSignalHandlingTest
    COMMAND lab_signal_handling --self-test
)
set_tests_properties(LinuxSignalHandlingTest PROPERTIES LABELS "linux;signal")

add_executable(ipc_integration_tests
    integration/test_unix_domain_socket_ipc.cpp
)

target_link_libraries(ipc_integration_tests
    PRIVATE
        voip::core
        GTest::gtest_main
)

project_set_warnings(ipc_integration_tests)
project_enable_sanitizers(ipc_integration_tests)

gtest_discover_tests(ipc_integration_tests
    PROPERTIES
        LABELS "ipc;integration;linux"
)

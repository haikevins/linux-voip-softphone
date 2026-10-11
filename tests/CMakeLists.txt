add_executable(unit_tests
    unit/test_call_manager.cpp
    unit/test_audio_device_selection.cpp
    unit/test_build_info.cpp
    unit/test_call_state.cpp
    unit/test_call_state_machine.cpp
    unit/test_ipv4_endpoint.cpp
    unit/test_rtp_statistics.cpp
    unit/test_tcp_client_socket.cpp
    unit/test_tcp_server_socket.cpp
    unit/test_udp_socket.cpp
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

add_executable(tcp_network_integration_tests
    integration/test_tcp_network.cpp
)

target_link_libraries(tcp_network_integration_tests
    PRIVATE
        voip::core
        GTest::gtest_main
)

project_set_warnings(tcp_network_integration_tests)
project_enable_sanitizers(tcp_network_integration_tests)

gtest_discover_tests(tcp_network_integration_tests
    PROPERTIES
        LABELS "network;tcp;integration;linux"
)

add_executable(udp_network_integration_tests
    integration/test_udp_network.cpp
)

target_link_libraries(udp_network_integration_tests
    PRIVATE
        voip::core
        GTest::gtest_main
)

project_set_warnings(udp_network_integration_tests)
project_enable_sanitizers(udp_network_integration_tests)

gtest_discover_tests(udp_network_integration_tests
    PROPERTIES
        LABELS "network;udp;integration;linux"
)

if(ENABLE_PJSIP)
    add_executable(pjsip_config_tests
        unit/test_pjsua2_audio_device_manager.cpp
        unit/test_pjsua2_call_audio_bridge.cpp
        unit/test_pjsua2_rtp_diagnostics.cpp
        unit/test_sip_account_config.cpp
        unit/test_sip_endpoint_config.cpp
        unit/test_sip_incoming_call_cli.cpp
        unit/test_sip_outgoing_call_cli.cpp
        unit/test_sip_registration_cli.cpp
    )

    target_link_libraries(pjsip_config_tests
        PRIVATE
            voip::pjsip
            GTest::gtest_main
    )

    project_set_warnings(pjsip_config_tests)
    project_enable_sanitizers(pjsip_config_tests)

    gtest_discover_tests(pjsip_config_tests
        PROPERTIES
            LABELS "pjsip;unit"
    )

    add_test(
        NAME PjsipCliContractIntegrationTest
        COMMAND
            bash
            ${CMAKE_CURRENT_SOURCE_DIR}/integration/test_pjsip_cli_contract.sh
            $<TARGET_FILE:voip_sip_register>
            $<TARGET_FILE:voip_sip_call>
            $<TARGET_FILE:voip_sip_listen>
    )
    set_tests_properties(PjsipCliContractIntegrationTest PROPERTIES
        LABELS "pjsip;integration;cli"
    )
endif()

# Qt-only registration bridge tests use injected fake sessions; no network or
# real SIP account is required, including with ENABLE_PJSIP=OFF.
if(ENABLE_QT_GUI)
    add_executable(qt_registration_backend_tests
        qt/test_qt_registration_backend.cpp
    )
    target_link_libraries(qt_registration_backend_tests PRIVATE
        voip_gui_bridge
        Qt6::Core
        GTest::gtest
    )
    project_set_warnings(qt_registration_backend_tests)
    project_enable_sanitizers(qt_registration_backend_tests)
    add_test(NAME QtRegistrationBackendTest
        COMMAND $<TARGET_FILE:qt_registration_backend_tests>
    )
    set_tests_properties(QtRegistrationBackendTest PROPERTIES LABELS "qt;gui;unit" TIMEOUT 20)
endif()

# Qt's rendering backend is software-only for this short, non-network smoke test.
# It checks QML loading and ViewModel binding; real calls belong to later commits.
if(ENABLE_QT_GUI)
    add_test(
        NAME QtGuiShellSmokeTest
        COMMAND $<TARGET_FILE:voip_qt_gui> --smoke-test
    )
    set_tests_properties(QtGuiShellSmokeTest PROPERTIES
        LABELS "qt;gui;smoke"
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
        TIMEOUT 15
    )
endif()

# Concurrency and PJSIP lifetime contract tests use the normal repository
# unit/integration taxonomy. They are ordinary canonical CTest entries rather
# than a phase-specific CMake subtree.
find_package(Threads REQUIRED)

function(add_plain_cpp_test target source ctest_name labels)
    add_executable(${target} ${source})
    target_compile_features(${target} PRIVATE cxx_std_20)
    target_include_directories(${target} PRIVATE "${PROJECT_SOURCE_DIR}/src")
    target_link_libraries(${target} PRIVATE Threads::Threads)
    project_set_warnings(${target})
    project_enable_sanitizers(${target})
    add_test(NAME ${ctest_name} COMMAND $<TARGET_FILE:${target}>)
    set_tests_properties(${ctest_name} PROPERTIES
        LABELS "${labels}"
        TIMEOUT 30
    )
endfunction()

add_plain_cpp_test(
    thread_safe_queue_tests
    unit/test_thread_safe_queue.cpp
    ThreadSafeQueueTest
    "threading;concurrency;unit;tsan"
)
add_plain_cpp_test(
    serial_task_worker_tests
    unit/test_serial_task_worker.cpp
    SerialTaskWorkerTest
    "threading;concurrency;unit;tsan"
)
add_plain_cpp_test(
    thread_ownership_tests
    unit/test_thread_ownership.cpp
    ThreadOwnershipTest
    "threading;concurrency;unit;tsan"
)
add_plain_cpp_test(
    mpmc_stress_tests
    integration/test_mpmc_stress.cpp
    MpmcStressTest
    "threading;concurrency;integration;stress;tsan"
)
set_tests_properties(MpmcStressTest PROPERTIES TIMEOUT 90)

add_plain_cpp_test(
    pjsip_call_slot_cleanup_policy_tests
    unit/test_pjsip_call_slot_cleanup.cpp
    PjsipCallSlotCleanupPolicyTest
    "threading;pjsip;unit;tsan"
)
add_plain_cpp_test(
    pjsip_account_shutdown_attempt_tests
    unit/test_pjsip_account_shutdown_attempt.cpp
    PjsipAccountShutdownAttemptTest
    "threading;pjsip;unit"
)
add_plain_cpp_test(
    pjsip_callback_activity_tests
    unit/test_pjsip_callback_activity.cpp
    PjsipCallbackActivityTest
    "threading;pjsip;unit;tsan"
)
add_plain_cpp_test(
    pjsip_shutdown_failure_paths_tests
    unit/test_pjsip_shutdown_failure_paths.cpp
    PjsipShutdownFailurePathsTest
    "threading;pjsip;unit"
)
add_plain_cpp_test(
    pjsip_callback_ingress_races_tests
    unit/test_pjsip_callback_ingress_races.cpp
    PjsipCallbackIngressRacesTest
    "threading;pjsip;unit"
)
add_plain_cpp_test(
    pjsip_shutdown_ownership_policy_tests
    unit/test_pjsip_shutdown_ownership_policy.cpp
    PjsipShutdownOwnershipPolicyTest
    "threading;pjsip;unit"
)
add_plain_cpp_test(
    pjsip_shutdown_authorization_tests
    unit/test_pjsip_shutdown_authorization.cpp
    PjsipShutdownAuthorizationTest
    "threading;pjsip;unit"
)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_test(
    NAME Phase8R2BLabSafetyTest
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/integration/test_phase8_r2b_lab_safety.py"
)
set_tests_properties(Phase8R2BLabSafetyTest PROPERTIES
    LABELS "pjsip;integration;audit"
    TIMEOUT 30
)

add_test(
    NAME PjsipProductionLifetimeAuditTest
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/integration/test_pjsip_production_lifetime_audit.py"
)
set_tests_properties(PjsipProductionLifetimeAuditTest PROPERTIES
    LABELS "pjsip;integration;audit"
    TIMEOUT 30
)

add_test(
    NAME PjsipShutdownIntegrationContractTest
    COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/integration/test_pjsip_shutdown_integration_contract.py"
)
set_tests_properties(PjsipShutdownIntegrationContractTest PROPERTIES
    LABELS "pjsip;integration;audit"
    TIMEOUT 30
)

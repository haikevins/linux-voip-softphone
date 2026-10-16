add_test(
    NAME Phase10RtspLabHelpTest
    COMMAND $<TARGET_FILE:rtsp_lab> --help
)
set_tests_properties(Phase10RtspLabHelpTest PROPERTIES
    LABELS "phase10;rtsp;smoke"
)

add_executable(phase10_rtsp_protocol_tests
    test_rtsp_protocol.cpp
)

target_link_libraries(phase10_rtsp_protocol_tests
    PRIVATE
        voip::rtsp_protocol
        GTest::gtest_main
)

project_set_warnings(phase10_rtsp_protocol_tests)
project_enable_sanitizers(phase10_rtsp_protocol_tests)

gtest_discover_tests(phase10_rtsp_protocol_tests
    PROPERTIES
        LABELS "phase10;rtsp;unit"
)

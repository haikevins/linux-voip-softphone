add_test(
    NAME Phase10RtspLabHelpTest
    COMMAND $<TARGET_FILE:rtsp_lab> --help
)
set_tests_properties(Phase10RtspLabHelpTest PROPERTIES
    LABELS "phase10;rtsp;smoke"
)

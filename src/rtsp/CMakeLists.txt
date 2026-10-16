add_executable(rtsp_lab
    main.cpp
)

set_target_properties(rtsp_lab PROPERTIES OUTPUT_NAME "rtsp-lab")

project_set_warnings(rtsp_lab)
project_enable_sanitizers(rtsp_lab)

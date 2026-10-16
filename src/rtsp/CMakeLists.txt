add_library(rtsp_protocol
    RtspMessage.cpp
    RtspParser.cpp
)

add_library(voip::rtsp_protocol ALIAS rtsp_protocol)

target_include_directories(rtsp_protocol
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

project_set_warnings(rtsp_protocol)
project_enable_sanitizers(rtsp_protocol)

add_executable(rtsp_lab
    main.cpp
)

set_target_properties(rtsp_lab PROPERTIES OUTPUT_NAME "rtsp-lab")

target_link_libraries(rtsp_lab
    PRIVATE
        voip::rtsp_protocol
)

project_set_warnings(rtsp_lab)
project_enable_sanitizers(rtsp_lab)

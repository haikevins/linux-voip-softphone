add_library(voip_core
    core/BuildInfo.cpp
)

add_library(voip::core ALIAS voip_core)

target_include_directories(voip_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

project_set_warnings(voip_core)
project_enable_sanitizers(voip_core)

add_executable(linux-voip-softphone
    app/main.cpp
)

target_link_libraries(linux-voip-softphone
    PRIVATE
        voip::core
)

project_set_warnings(linux-voip-softphone)
project_enable_sanitizers(linux-voip-softphone)

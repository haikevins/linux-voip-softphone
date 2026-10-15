qt_add_executable(voip_gui
    main.cpp
)

set_target_properties(voip_gui PROPERTIES OUTPUT_NAME "linux-voip-softphone-gui")

qt_add_qml_module(voip_gui
    URI VoipSoftphone
    VERSION 1.0
    RESOURCE_PREFIX "/qt/qml"
    QML_FILES
        Main.qml
)

target_link_libraries(voip_gui
    PRIVATE
        voip::core
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
)

project_set_warnings(voip_gui)
project_enable_sanitizers(voip_gui)

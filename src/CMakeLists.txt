add_library(voip_core
    application/CallManager.cpp
    audio/AudioDeviceSelection.cpp
    core/BuildInfo.cpp
    core/CallState.cpp
    core/CallStateMachine.cpp
    core/StateChangeSink.cpp
    ipc/UnixDomainSocketClient.cpp
    ipc/UnixDomainSocketServer.cpp
    network/Ipv4Endpoint.cpp
    network/RtpStatistics.cpp
    network/TcpClientSocket.cpp
    network/TcpServerSocket.cpp
    network/UdpSocket.cpp
    platform/linux/UniqueFd.cpp
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

if(ENABLE_PJSIP)
    add_library(voip_pjsip
        pjsip/SipAccountConfig.cpp
        pjsip/SipEndpointConfig.cpp
        pjsip/Pjsua2AudioDeviceManager.cpp
        pjsip/Pjsua2CallAudioBridge.cpp
        pjsip/Pjsua2RtpDiagnostics.cpp
        pjsip/SipIncomingCallCli.cpp
        pjsip/SipIncomingCallClient.cpp
        pjsip/SipOutgoingCallCli.cpp
        pjsip/SipOutgoingCallClient.cpp
        pjsip/SipRegistrationCli.cpp
        pjsip/SipRegistrationClient.cpp
    )

    add_library(voip::pjsip ALIAS voip_pjsip)

    target_include_directories(voip_pjsip
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_link_libraries(voip_pjsip
        PUBLIC
            voip::core
            PJSIP::PJSIP
    )

    project_set_warnings(voip_pjsip)
    project_enable_sanitizers(voip_pjsip)

    add_executable(voip_sip_register
        app/sip_register_main.cpp
    )

    set_target_properties(voip_sip_register PROPERTIES OUTPUT_NAME "voip-sip-register")

    target_link_libraries(voip_sip_register
        PRIVATE
            voip::pjsip
    )

    project_set_warnings(voip_sip_register)
    project_enable_sanitizers(voip_sip_register)

    add_executable(voip_sip_call
        app/sip_call_main.cpp
    )

    set_target_properties(voip_sip_call PROPERTIES OUTPUT_NAME "voip-sip-call")

    target_link_libraries(voip_sip_call
        PRIVATE
            voip::pjsip
    )

    project_set_warnings(voip_sip_call)
    project_enable_sanitizers(voip_sip_call)

    add_executable(voip_sip_listen
        app/sip_listen_main.cpp
    )

    set_target_properties(voip_sip_listen PROPERTIES OUTPUT_NAME "voip-sip-listen")

    target_link_libraries(voip_sip_listen
        PRIVATE
            voip::pjsip
    )

    project_set_warnings(voip_sip_listen)
    project_enable_sanitizers(voip_sip_listen)
endif()

# Phase 7 Commit 1: an optional Qt shell with an inert C++ ViewModel.
# No SIP engine, credentials, or call-media objects are exposed to QML.
if(ENABLE_QT_GUI)
    qt_add_executable(voip_qt_gui
        app/qt_gui_main.cpp
        gui/GuiViewModel.cpp
        gui/GuiViewModel.hpp
    )

    set_target_properties(voip_qt_gui PROPERTIES
        OUTPUT_NAME "voip-qt-gui"
        AUTOMOC ON
    )

    qt_add_resources(voip_qt_gui "phase7_gui_qml"
        PREFIX "/"
        FILES gui/Main.qml
    )

    target_link_libraries(voip_qt_gui
        PRIVATE
            voip::core
            Qt6::Quick
            Qt6::QuickControls2
    )

    project_set_warnings(voip_qt_gui)
    project_enable_sanitizers(voip_qt_gui)
endif()

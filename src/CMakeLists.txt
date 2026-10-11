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

    if(ENABLE_PHASE8_R2A_LAB)
        add_executable(phase8_r2a_production_stop_reuse
            "${PROJECT_SOURCE_DIR}/labs/pjsip/production-stop-reuse/phase8_r2a_production_stop_reuse.cpp"
        )
        target_link_libraries(phase8_r2a_production_stop_reuse PRIVATE voip::pjsip)
        project_set_warnings(phase8_r2a_production_stop_reuse)
        project_enable_sanitizers(phase8_r2a_production_stop_reuse)
    endif()
endif()

# Phase 7 Commit 3: Qt bridge is testable without an actual SIP server.
# Production PJSIP registration is enabled only when both options are ON.
if(ENABLE_QT_GUI)
    add_library(voip_gui_bridge
        gui/GuiViewModel.cpp
        gui/GuiViewModel.hpp
        gui/QtRegistrationBackend.cpp
        gui/QtRegistrationBackend.hpp
    )
    set_target_properties(voip_gui_bridge PROPERTIES AUTOMOC ON)
    target_link_libraries(voip_gui_bridge PUBLIC voip::core Qt6::Core)
    target_include_directories(voip_gui_bridge PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    project_set_warnings(voip_gui_bridge)
    project_enable_sanitizers(voip_gui_bridge)

    qt_add_executable(voip_qt_gui
        app/qt_gui_main.cpp
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
            voip_gui_bridge
            Qt6::Quick
            Qt6::QuickControls2
    )

    if(ENABLE_PJSIP)
        target_sources(voip_qt_gui PRIVATE
            gui/PjsipRegistrationSession.cpp
            gui/PjsipRegistrationSession.hpp
        )
        target_link_libraries(voip_qt_gui PRIVATE voip::pjsip)
        target_compile_definitions(voip_qt_gui PRIVATE VOIP_GUI_WITH_PJSIP=1)
    endif()

    project_set_warnings(voip_qt_gui)
    project_enable_sanitizers(voip_qt_gui)
endif()

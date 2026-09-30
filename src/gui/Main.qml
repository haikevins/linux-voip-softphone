import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    objectName: "phase7GuiShell"
    width: 440
    height: 680
    minimumWidth: 360
    minimumHeight: 560
    visible: true
    title: qsTr("Linux VoIP | Phase 7")
    color: "#111827"
    property int callElapsedSeconds: 0

    function formatCallDuration(totalSeconds) {
        const minutes = Math.floor(totalSeconds / 60)
        const seconds = totalSeconds % 60
        const minuteText = minutes < 10 ? "0" + minutes : minutes.toString()
        const secondText = seconds < 10 ? "0" + seconds : seconds.toString()
        return minuteText + ":" + secondText
    }

    Timer {
        interval: 1000
        repeat: true
        running: softphone.callConnected
        onTriggered: window.callElapsedSeconds += 1
    }

    Connections {
        target: softphone
        function onStatusChanged() {
            if (!softphone.callConnected)
                window.callElapsedSeconds = 0
        }
    }

    ScrollView {
        anchors.fill: parent
        anchors.margins: 16
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: parent.width
            spacing: 14

            Label {
                text: qsTr("Linux VoIP")
                color: "#f9fafb"
                font.pixelSize: 30
                font.bold: true
                Layout.fillWidth: true
            }

            Rectangle {
                color: "#1f2937"
                radius: 10
                implicitHeight: 68
                Layout.fillWidth: true

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 10

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        Label {
                            text: qsTr("Registration")
                            color: "#9ca3af"
                            font.pixelSize: 12
                        }
                        Label {
                            text: softphone.registrationStatus
                            color: "#fbbf24"
                            font.pixelSize: 15
                        }
                    }
                    Button {
                        text: qsTr("Register")
                        enabled: softphone.registrationActionsEnabled
                        Accessible.name: qsTr("Register SIP account")
                        onClicked: softphone.registerAccount()
                    }
                }
            }

            Label {
                text: qsTr("Destination")
                color: "#e5e7eb"
                font.pixelSize: 16
            }

            TextField {
                id: destinationField
                objectName: "destinationField"
                Layout.fillWidth: true
                placeholderText: qsTr("Enter an extension")
                maximumLength: 64
                font.pixelSize: 22
                inputMethodHints: Qt.ImhDialableCharactersOnly
                Accessible.name: qsTr("Destination extension")
            }

            GridLayout {
                columns: 3
                columnSpacing: 10
                rowSpacing: 10
                Layout.fillWidth: true

                Repeater {
                    model: ["1", "2", "3", "4", "5", "6", "7", "8", "9", "*", "0", "#"]

                    Button {
                        required property string modelData
                        text: modelData
                        font.pixelSize: 22
                        Layout.fillWidth: true
                        Layout.preferredHeight: 52
                        onClicked: destinationField.text += text
                    }
                }
            }

            RowLayout {
                spacing: 12
                Layout.fillWidth: true

                Button {
                    text: qsTr("Clear")
                    Layout.fillWidth: true
                    onClicked: destinationField.clear()
                }
                Button {
                    text: qsTr("Delete")
                    Layout.fillWidth: true
                    onClicked: destinationField.text = destinationField.text.slice(0, -1)
                }
            }

            Label {
                text: softphone.callStatus
                color: "#e5e7eb"
                font.pixelSize: 16
                Layout.fillWidth: true
            }

            Label {
                text: softphone.activePeer
                visible: text.length > 0
                color: "#d1d5db"
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            Label {
                objectName: "callDurationLabel"
                text: qsTr("Duration: %1").arg(window.formatCallDuration(window.callElapsedSeconds))
                visible: softphone.callConnected
                color: "#93c5fd"
                font.pixelSize: 15
                Layout.fillWidth: true
            }

            Rectangle {
                id: rtpDiagnosticsPanel
                objectName: "rtpDiagnosticsPanel"
                visible: softphone.callConnected && softphone.rtpDiagnosticsAvailable
                color: "#172033"
                radius: 10
                implicitHeight: rtpDiagnosticsColumn.implicitHeight + 24
                Layout.fillWidth: true

                ColumnLayout {
                    id: rtpDiagnosticsColumn
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 6

                    Label {
                        text: qsTr("Live RTP / Audio")
                        color: "#f9fafb"
                        font.pixelSize: 16
                        font.bold: true
                        Layout.fillWidth: true
                    }

                    Label {
                        text: qsTr("Codec: %1").arg(softphone.rtpCodec)
                        color: "#d1d5db"
                        Layout.fillWidth: true
                    }

                    Label {
                        text: qsTr("Packets: RX %1   TX %2   Lost %3")
                              .arg(softphone.rtpPacketsReceived)
                              .arg(softphone.rtpPacketsSent)
                              .arg(softphone.rtpPacketsLost)
                        color: "#d1d5db"
                        Layout.fillWidth: true
                    }

                    Label {
                        text: qsTr("Loss: %1%   Jitter: %2 ms   RTT: %3 ms")
                              .arg(softphone.rtpPacketLossPercent.toFixed(2))
                              .arg(softphone.rtpJitterMs.toFixed(2))
                              .arg(softphone.rtpRoundTripTimeMs.toFixed(2))
                        color: "#93c5fd"
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
            }

            Rectangle {
                objectName: "incomingCallPanel"
                visible: softphone.incomingCall
                color: "#1f2937"
                radius: 10
                implicitHeight: incomingColumn.implicitHeight + 24
                Layout.fillWidth: true

                ColumnLayout {
                    id: incomingColumn
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 10

                    Label {
                        text: qsTr("Incoming call")
                        color: "#f9fafb"
                        font.pixelSize: 18
                        font.bold: true
                        Layout.fillWidth: true
                    }

                    Label {
                        text: softphone.activePeer.length > 0
                              ? qsTr("From: %1").arg(softphone.activePeer)
                              : qsTr("From: Unknown caller")
                        color: "#d1d5db"
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        Button {
                            objectName: "answerButton"
                            text: qsTr("Answer")
                            Layout.fillWidth: true
                            Layout.preferredHeight: 52
                            enabled: softphone.answerActionsEnabled
                            Accessible.name: qsTr("Answer incoming SIP call")
                            onClicked: softphone.answerCall()
                        }
                        Button {
                            objectName: "declineButton"
                            text: qsTr("Decline")
                            Layout.fillWidth: true
                            Layout.preferredHeight: 52
                            enabled: softphone.declineActionsEnabled
                            Accessible.name: qsTr("Decline incoming SIP call")
                            onClicked: softphone.declineCall()
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                visible: !softphone.incomingCall
                Button {
                    text: qsTr("Call")
                    Layout.fillWidth: true
                    Layout.preferredHeight: 52
                    enabled: softphone.callActionsEnabled && destinationField.text.length > 0
                    Accessible.name: qsTr("Place outgoing SIP call")
                    onClicked: softphone.startCall(destinationField.text)
                }
                Button {
                    text: qsTr("Hang up")
                    Layout.fillWidth: true
                    Layout.preferredHeight: 52
                    enabled: softphone.hangupActionsEnabled
                    Accessible.name: qsTr("End active SIP call")
                    onClicked: softphone.hangupCall()
                }
            }

            Button {
                text: qsTr("Clear error")
                visible: softphone.recoveryActionsEnabled
                enabled: softphone.recoveryActionsEnabled
                Layout.fillWidth: true
                onClicked: softphone.recoverCall()
            }

            Label {
                text: softphone.backendAvailable
                      ? qsTr("Incoming and outgoing SIP calls share the registered account.")
                      : qsTr("GUI preview only. Start with --sip-register to enable live SIP calls.")
                color: "#9ca3af"
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                Layout.fillWidth: true
            }

            Label {
                text: softphone.lastError
                visible: text.length > 0
                color: "#fca5a5"
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            Item { Layout.fillHeight: true }
        }
    }
}

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

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 16

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

        Button {
            text: qsTr("Call")
            Layout.fillWidth: true
            Layout.preferredHeight: 52
            enabled: softphone.callActionsEnabled
            Accessible.name: qsTr("Call - unavailable during registration-only integration")
        }

        Label {
            text: softphone.backendAvailable
                  ? qsTr("Registration-only SIP integration. Calls and audio are not wired yet.")
                  : qsTr("GUI preview only. Start with --sip-register to enable live registration.")
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

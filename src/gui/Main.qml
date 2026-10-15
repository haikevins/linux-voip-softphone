import QtQuick

Rectangle {
    id: root

    width: 480
    height: 720
    color: "#101418"

    property string destination: ""
    property string transport: "udp"
    property bool showDiagnostics: false

    function appendDigit(digit) {
        if (destination.length < 32)
            destination += digit
    }

    function fieldBorder(active) {
        return active ? "#6db5ff" : "#39434d"
    }

    component ActionButton: Rectangle {
        id: button
        property string label: ""
        property bool enabled: true
        property color buttonColor: "#2478d4"
        signal clicked()

        implicitHeight: 48
        radius: 10
        color: enabled ? buttonColor : "#30363d"
        opacity: enabled ? 1.0 : 0.65

        Text {
            anchors.centerIn: parent
            text: button.label
            color: "white"
            font.pixelSize: 16
            font.bold: true
        }

        MouseArea {
            anchors.fill: parent
            enabled: button.enabled
            cursorShape: Qt.PointingHandCursor
            onClicked: button.clicked()
        }
    }

    component InputField: Rectangle {
        id: frame
        property alias text: editor.text
        property alias echoMode: editor.echoMode
        property string placeholderText: ""

        implicitHeight: 44
        radius: 8
        color: "#171d22"
        border.width: 1
        border.color: root.fieldBorder(editor.activeFocus)

        TextInput {
            id: editor
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            verticalAlignment: TextInput.AlignVCenter
            color: "#f5f7fa"
            selectionColor: "#2478d4"
            selectedTextColor: "white"
            font.pixelSize: 15
            clip: true

            Text {
                anchors.fill: parent
                verticalAlignment: Text.AlignVCenter
                text: frame.placeholderText
                color: "#73808c"
                font.pixelSize: 15
                visible: editor.text.length === 0 && !editor.activeFocus
            }
        }
    }

    Column {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 18

        Row {
            width: parent.width
            height: 40
            spacing: 12

            Text {
                width: parent.width - diagnosticsButton.width - parent.spacing
                anchors.verticalCenter: parent.verticalCenter
                text: "Linux VoIP"
                color: "#f5f7fa"
                font.pixelSize: 24
                font.bold: true
            }

            Rectangle {
                id: diagnosticsButton
                width: 104
                height: 36
                radius: 8
                color: root.showDiagnostics ? "#325f86" : "#222a31"

                Text {
                    anchors.centerIn: parent
                    text: root.showDiagnostics ? "Back" : "Diagnostics"
                    color: "#dce6ef"
                    font.pixelSize: 13
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.showDiagnostics = !root.showDiagnostics
                }
            }
        }

        Rectangle {
            width: parent.width
            height: 46
            radius: 10
            color: "#171d22"
            border.width: 1
            border.color: "#28323b"

            Row {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                spacing: 10

                Rectangle {
                    width: 10
                    height: 10
                    radius: 5
                    anchors.verticalCenter: parent.verticalCenter
                    color: callViewModel.stateText === "Ready" || callViewModel.stateText === "Connected"
                           ? "#39c172"
                           : callViewModel.stateText === "Failed" ? "#e0565b" : "#e6aa46"
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: callViewModel.stateText
                    color: "#e8edf2"
                    font.pixelSize: 15
                    font.bold: true
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: callViewModel.accountText.length > 0 ? callViewModel.accountText : "Not registered"
                    color: "#8e9aa5"
                    font.pixelSize: 13
                }
            }
        }

        Item {
            width: parent.width
            height: parent.height - 40 - 46 - 54

            Column {
                anchors.fill: parent
                spacing: 14
                visible: root.showDiagnostics

                Text {
                    text: "Diagnostics"
                    color: "#f5f7fa"
                    font.pixelSize: 22
                    font.bold: true
                }

                Repeater {
                    model: [
                        ["State", callViewModel.stateText],
                        ["Account", callViewModel.accountText.length > 0 ? callViewModel.accountText : "-"],
                        ["Peer", callViewModel.peer.length > 0 ? callViewModel.peer : "-"],
                        ["Direction", callViewModel.peer.length === 0 ? "-" : (callViewModel.incomingCall ? "Incoming" : "Outgoing")],
                        ["Duration", callViewModel.callDurationText]
                    ]

                    delegate: Rectangle {
                        required property var modelData
                        width: parent.width
                        height: 52
                        radius: 8
                        color: "#171d22"

                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 14
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData[0]
                            color: "#8e9aa5"
                            font.pixelSize: 14
                        }

                        Text {
                            anchors.right: parent.right
                            anchors.rightMargin: 14
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData[1]
                            color: "#e8edf2"
                            font.pixelSize: 14
                            font.bold: true
                        }
                    }
                }

                Rectangle {
                    width: parent.width
                    height: errorDiagnosticText.implicitHeight + 28
                    radius: 8
                    color: "#2a1b1d"
                    visible: callViewModel.errorText.length > 0

                    Text {
                        id: errorDiagnosticText
                        anchors.fill: parent
                        anchors.margins: 14
                        text: callViewModel.errorText
                        color: "#ff9c9f"
                        font.pixelSize: 13
                        wrapMode: Text.Wrap
                    }
                }
            }

            Column {
                anchors.fill: parent
                spacing: 12
                visible: !root.showDiagnostics && callViewModel.canRegister

                Text {
                    text: callViewModel.stateText === "Failed" ? "Registration failed" : "SIP account"
                    color: "#f5f7fa"
                    font.pixelSize: 22
                    font.bold: true
                }

                InputField {
                    id: usernameField
                    width: parent.width
                    placeholderText: "Username (for example 1001)"
                }

                InputField {
                    id: passwordField
                    width: parent.width
                    placeholderText: "Password"
                    echoMode: TextInput.Password
                }

                InputField {
                    id: serverField
                    width: parent.width
                    placeholderText: "SIP server / Asterisk host"
                }

                Row {
                    width: parent.width
                    spacing: 10

                    InputField {
                        id: serverPortField
                        width: (parent.width - parent.spacing) / 2
                        text: "5060"
                        placeholderText: "Server port"
                    }

                    InputField {
                        id: listenPortField
                        width: (parent.width - parent.spacing) / 2
                        text: "0"
                        placeholderText: "Listen port"
                    }
                }

                Row {
                    width: parent.width
                    spacing: 10

                    ActionButton {
                        width: (parent.width - parent.spacing) / 2
                        label: "UDP"
                        buttonColor: root.transport === "udp" ? "#2478d4" : "#303942"
                        onClicked: root.transport = "udp"
                    }

                    ActionButton {
                        width: (parent.width - parent.spacing) / 2
                        label: "TCP"
                        buttonColor: root.transport === "tcp" ? "#2478d4" : "#303942"
                        onClicked: root.transport = "tcp"
                    }
                }

                ActionButton {
                    width: parent.width
                    label: callViewModel.stateText === "Failed" ? "Retry registration" : "Register"
                    enabled: usernameField.text.length > 0 && passwordField.text.length > 0 && serverField.text.length > 0
                    onClicked: callViewModel.registerAccount(usernameField.text,
                                                             passwordField.text,
                                                             serverField.text,
                                                             parseInt(serverPortField.text),
                                                             parseInt(listenPortField.text),
                                                             root.transport)
                }

                Text {
                    width: parent.width
                    visible: callViewModel.errorText.length > 0
                    text: callViewModel.errorText
                    color: "#ff8589"
                    font.pixelSize: 13
                    wrapMode: Text.Wrap
                }
            }

            Column {
                anchors.fill: parent
                spacing: 14
                visible: !root.showDiagnostics && callViewModel.canCall

                Text {
                    text: "Destination"
                    color: "#f5f7fa"
                    font.pixelSize: 22
                    font.bold: true
                }

                Rectangle {
                    width: parent.width
                    height: 58
                    radius: 10
                    color: "#171d22"
                    border.width: 1
                    border.color: "#39434d"

                    Text {
                        anchors.fill: parent
                        anchors.margins: 14
                        verticalAlignment: Text.AlignVCenter
                        horizontalAlignment: Text.AlignHCenter
                        text: root.destination.length > 0 ? root.destination : "Enter extension"
                        color: root.destination.length > 0 ? "#f5f7fa" : "#65717c"
                        font.pixelSize: 22
                    }
                }

                Grid {
                    anchors.horizontalCenter: parent.horizontalCenter
                    columns: 3
                    spacing: 10

                    Repeater {
                        model: ["1", "2", "3", "4", "5", "6", "7", "8", "9", "Clear", "0", "⌫"]

                        delegate: Rectangle {
                            required property string modelData
                            width: 104
                            height: 58
                            radius: 12
                            color: modelData === "Clear" || modelData === "⌫" ? "#252d35" : "#1c242b"
                            border.width: 1
                            border.color: "#303c46"

                            Text {
                                anchors.centerIn: parent
                                text: modelData
                                color: "#edf2f6"
                                font.pixelSize: modelData.length === 1 ? 22 : 14
                                font.bold: true
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (modelData === "Clear")
                                        root.destination = ""
                                    else if (modelData === "⌫")
                                        root.destination = root.destination.slice(0, -1)
                                    else
                                        root.appendDigit(modelData)
                                }
                            }
                        }
                    }
                }

                ActionButton {
                    width: parent.width
                    label: "Call"
                    enabled: root.destination.length > 0
                    buttonColor: "#2b9b60"
                    onClicked: callViewModel.makeCall(root.destination)
                }
            }

            Column {
                anchors.fill: parent
                spacing: 18
                visible: !root.showDiagnostics && !callViewModel.canRegister && !callViewModel.canCall

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: callViewModel.peer.length > 0 ? callViewModel.peer : "SIP call"
                    color: "#f5f7fa"
                    font.pixelSize: 28
                    font.bold: true
                    elide: Text.ElideRight
                }

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: callViewModel.incomingCall && callViewModel.stateText === "Ringing"
                          ? "Incoming call"
                          : callViewModel.stateText
                    color: callViewModel.stateText === "Connected" ? "#52d68a" : "#aeb9c2"
                    font.pixelSize: 20
                }

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: callViewModel.callDurationText
                    color: "#e7edf2"
                    font.pixelSize: 36
                    font.bold: true
                    visible: callViewModel.stateText === "Connected"
                }

                Item {
                    width: 1
                    height: 30
                }

                ActionButton {
                    width: parent.width
                    visible: callViewModel.canAnswer
                    label: "Answer"
                    buttonColor: "#2b9b60"
                    onClicked: callViewModel.answerCall()
                }

                ActionButton {
                    width: parent.width
                    visible: callViewModel.canHangup
                    label: "Hang up"
                    buttonColor: "#c84e55"
                    onClicked: callViewModel.hangupCall()
                }

                Text {
                    width: parent.width
                    visible: callViewModel.errorText.length > 0
                    text: callViewModel.errorText
                    color: "#ff8589"
                    horizontalAlignment: Text.AlignHCenter
                    font.pixelSize: 13
                    wrapMode: Text.Wrap
                }
            }
        }
    }
}

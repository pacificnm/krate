import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KrateRemote

// Which player, logging in, what it is, and Krate started again.
ToolBar {
    id: bar

    readonly property bool connected: Remote.state === "connected"
    readonly property bool connecting: Remote.state === "connecting"

    function connectNow() {
        Remote.connectTo(hostField.text, userField.text, parseInt(portField.text) || 22, passwordField.text)
        passwordField.text = ""
    }

    padding: 6

    ColumnLayout {
        width: parent.width
        spacing: 4

        RowLayout {
            spacing: 8
            Label { text: "Player" }
            TextField {
                id: hostField
                Layout.preferredWidth: 180
                text: Remote.host
                placeholderText: "krate.local"
                enabled: !bar.connected && !bar.connecting
                onAccepted: bar.connectNow()
            }
            Label { text: "User" }
            TextField {
                id: userField
                Layout.preferredWidth: 110
                text: Remote.user
                enabled: !bar.connected && !bar.connecting
                onAccepted: bar.connectNow()
            }
            Label { text: "Port" }
            TextField {
                id: portField
                Layout.preferredWidth: 56
                text: Remote.port
                inputMethodHints: Qt.ImhDigitsOnly
                validator: IntValidator { bottom: 1; top: 65535 }
                enabled: !bar.connected && !bar.connecting
            }
            TextField {
                id: passwordField
                Layout.preferredWidth: 160
                visible: Remote.needsPassword && !bar.connected
                echoMode: TextInput.Password
                placeholderText: "Password"
                onVisibleChanged: if (visible) forceActiveFocus()
                onAccepted: bar.connectNow()
            }
            Button {
                text: bar.connected ? "DISCONNECT" : (bar.connecting ? "CONNECTING…" : "CONNECT")
                enabled: !bar.connecting
                highlighted: !bar.connected
                onClicked: bar.connected ? Remote.disconnectFrom() : bar.connectNow()
            }
            BusyIndicator {
                Layout.preferredWidth: 28
                Layout.preferredHeight: 28
                running: Remote.busy || bar.connecting
            }
            Item { Layout.fillWidth: true }
            Button {
                visible: bar.connected && Remote.usingPassword
                text: "SET UP KEY LOGIN"
                ToolTip.visible: hovered
                ToolTip.text: "Makes a key for this app (~/.ssh/krate-remote) and puts it on the player: no password from now on."
                onClicked: Remote.setUpKey()
            }
            Button {
                visible: bar.connected
                text: "REFRESH"
                onClicked: Remote.refresh()
            }
            Button {
                visible: bar.connected
                text: "RESTART KRATE"
                enabled: Remote.krateState === "kiosk"
                highlighted: Remote.restartNeeded
                ToolTip.visible: hovered
                ToolTip.text: Remote.krateState === "kiosk"
                              ? "Krate quits and the player's kiosk starts it again (a few seconds): folders move then."
                              : "Krate isn't running in the player's kiosk: start it there."
                onClicked: restartConfirm.open()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 14
            Label {
                visible: bar.connected
                font.bold: true
                text: (Remote.hostName || Remote.host)
                      + (Remote.version ? "  ·  Krate " + Remote.version : "  ·  Krate isn't installed")
                      + "  ·  " + ({ kiosk: "running (kiosk)", running: "running (desktop)", stopped: "not running" }[Remote.krateState] || "")
            }
            Label {
                Layout.fillWidth: true
                text: Remote.message
                elide: Text.ElideRight
                color: palette.placeholderText
                ToolTip.visible: messageArea.containsMouse && truncated
                ToolTip.text: Remote.message
                MouseArea {
                    id: messageArea
                    anchors.fill: parent
                    hoverEnabled: true
                }
            }
        }
    }

    Dialog {
        id: restartConfirm
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        title: "Restart Krate on " + (Remote.hostName || Remote.host) + "?"
        width: 440
        standardButtons: Dialog.Yes | Dialog.Cancel
        Label {
            width: restartConfirm.availableWidth
            wrapMode: Text.WordWrap
            text: "Krate quits properly (saving as it does) and starts again in a few seconds. Anything playing stops."
        }
        onAccepted: Remote.restartKrate()
    }
}

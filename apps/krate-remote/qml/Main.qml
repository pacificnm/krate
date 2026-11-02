pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KrateRemote

// Krate Remote (docs/REMOTE.md): a player's settings, files and crash
// reports, from another computer, over SSH.
ApplicationWindow {
    id: window
    width: 1180
    height: 760
    minimumWidth: 760
    minimumHeight: 480
    visible: true
    title: Remote.state === "connected" ? "Krate Remote — " + (Remote.hostName || Remote.host) : "Krate Remote"

    readonly property bool connected: Remote.state === "connected"

    header: ConnectBar {}

    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        visible: window.connected

        TabBar {
            id: tabs
            Layout.fillWidth: true
            TabButton { text: "Settings" + (Remote.restartNeeded ? "  •" : "") }
            TabButton { text: "Files" }
            TabButton { text: "Crash reports and log" + (Remote.crashReports.length ? "  (" + Remote.crashReports.length + ")" : "") }
        }
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabs.currentIndex
            SettingsPage {}
            FilesPage {}
            ReportsPage {}
        }
    }

    // Not connected: what this is and what it needs.
    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 48, 620)
        spacing: 12
        visible: !window.connected
        Label {
            text: "Krate Remote"
            font.pixelSize: 26
            font.bold: true
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: "Manages a Krate player from this computer: its settings, its files (songs, samples, recordings, "
                  + "plugins) both ways, its crash reports, and starting Krate again. It logs in over SSH, "
                  + "as you would from a terminal: the player needs openssh-server, and this computer ssh."
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: palette.placeholderText
            text: "Enter the player's name on the network (krate.local) or its address, and the user Krate runs as, "
                  + "then CONNECT. If it asks for a password, once logged in SET UP KEY LOGIN makes it the last time."
        }
    }
}

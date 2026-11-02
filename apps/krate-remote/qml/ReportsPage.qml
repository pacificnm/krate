pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KrateRemote

// The player's crash reports (docs/CRASHES.md) and Krate's log, read there.
SplitView {
    id: page

    property string shown: ""
    property string text: ""

    function show(path) {
        shown = path
        text = "Reading…"
        Remote.readText(path)
    }

    Connections {
        target: Remote
        function onTextRead(path, text) {
            if (path === page.shown)
                page.text = text
        }
    }

    ColumnLayout {
        SplitView.preferredWidth: 280
        SplitView.minimumWidth: 180
        spacing: 0
        ItemDelegate {
            Layout.fillWidth: true
            text: "Krate's log (this session)"
            highlighted: page.shown === Remote.logPath
            onClicked: page.show(Remote.logPath)
        }
        ItemDelegate {
            Layout.fillWidth: true
            text: "Krate's log (the session before)"
            highlighted: page.shown === Remote.logPath + ".1"
            onClicked: page.show(Remote.logPath + ".1")
        }
        Label {
            text: Remote.crashReports.length ? "CRASH REPORTS" : "No crash reports"
            padding: 8
            font.bold: true
            color: palette.placeholderText
        }
        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: Remote.crashReports
            delegate: ItemDelegate {
                required property var modelData
                width: ListView.view.width
                text: modelData.when
                highlighted: page.shown === modelData.path
                onClicked: page.show(modelData.path)
            }
        }
    }

    ScrollView {
        SplitView.fillWidth: true
        TextArea {
            readOnly: true
            selectByMouse: true
            wrapMode: TextEdit.NoWrap
            font.family: "monospace"
            font.pixelSize: 12
            text: page.text || "Pick a report or the log."
        }
    }
}

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Krate
import KrateRack

// The crash reports (docs/CRASHES.md): the list, newest first, and the one
// picked: what happened, when, where in the code, and the log before it.
Popup {
    id: popup

    property string chosen: ""
    readonly property var list: Crashes.reports

    modal: true
    padding: 16
    visible: Crashes.viewing
    onClosed: Crashes.viewing = false
    onOpened: chosen = Crashes.newReport !== "" ? Crashes.newReport : (list.length > 0 ? list[0].path : "")
    onListChanged: if (!list.some(r => r.path === chosen)) chosen = list.length > 0 ? list[0].path : ""
    background: Rectangle { radius: 8; color: Theme.lowerDeck; border.color: Theme.deckLine; border.width: 2 }

    Column {
        width: popup.availableWidth
        height: popup.availableHeight
        spacing: 10

        Row {
            width: parent.width
            spacing: 10
            Text {
                width: parent.width - 3 * 130
                anchors.verticalCenter: parent.verticalCenter
                text: "CRASH REPORTS. Written when Krate crashes (or a session never ends: killed, frozen, power cut), kept in "
                      + Crashes.folder + " (the last 30)."
                wrapMode: Text.WordWrap
                color: Theme.ink
                font.family: Theme.labelFont
                font.pixelSize: 13
                font.bold: true
            }
            PanelButton {
                width: 120
                height: 32
                enabled: popup.chosen !== ""
                opacity: enabled ? 1 : 0.5
                faceText: "DELETE"
                text: "Delete this report"
                onClicked: Crashes.remove(popup.chosen)
            }
            PanelButton {
                width: 120
                height: 32
                enabled: popup.list.length > 0
                opacity: enabled ? 1 : 0.5
                faceText: "DELETE ALL"
                text: "Delete every report"
                onClicked: Crashes.removeAll()
            }
            PanelButton {
                width: 120
                height: 32
                faceText: "CLOSE"
                text: "Back to Krate"
                onClicked: popup.close()
            }
        }

        Row {
            width: parent.width
            height: parent.height - y
            spacing: 10

            ListView { // the reports
                id: reportList
                width: 300
                height: parent.height
                clip: true
                spacing: 4
                model: popup.list
                ScrollBar.vertical: ScrollBar {}
                delegate: Rectangle {
                    id: row
                    required property var modelData
                    readonly property bool picked: popup.chosen === modelData.path
                    width: reportList.width - 12
                    height: 58
                    radius: 4
                    color: picked ? Theme.silverLight : "#D9DADC"
                    border.color: picked ? Theme.accentUi : Theme.deckLine
                    border.width: picked ? 2 : 1
                    Column {
                        x: 8
                        y: 6
                        width: parent.width - 16
                        spacing: 2
                        Text {
                            text: row.modelData.when
                            color: Theme.ink
                            font.family: Theme.labelFont
                            font.pixelSize: 13
                            font.bold: true
                        }
                        Text {
                            width: parent.width
                            text: row.modelData.summary
                            color: "#55575B"
                            font.family: Theme.labelFont
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                        }
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: popup.chosen = row.modelData.path
                    }
                }
                Text {
                    anchors.centerIn: parent
                    visible: popup.list.length === 0
                    text: "No crash reports."
                    color: Theme.ink
                    font.family: Theme.labelFont
                    font.pixelSize: 14
                }
            }

            ScrollView { // the report
                width: parent.width - reportList.width - 10
                height: parent.height
                TextArea {
                    readOnly: true
                    selectByMouse: true
                    wrapMode: TextEdit.NoWrap
                    text: popup.chosen !== "" ? Crashes.text(popup.chosen) : ""
                    color: "#E8E8E8"
                    font.family: "monospace"
                    font.pixelSize: 12
                    background: Rectangle { color: "#1F2125"; radius: 4 }
                }
            }
        }
    }
}

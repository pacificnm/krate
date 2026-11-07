pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import Krate

// Picks a channel's amp: a capture of a real amp (docs/CAPTURES.md), the
// .nam files in the captures folder, or none (the channel is silent). A click
// plays it at once. IMPORT (or a drop) copies .nam files in; Krate Remote can put them
// there too (Files > Places > Amp captures).
Popup {
    id: picker
    required property Captures captures

    property int channel: 0
    readonly property string current: picker.captures.revision >= 0 ? picker.captures.capture(channel) : ""

    width: 560
    height: 520
    padding: 14
    modal: true
    background: Rectangle {
        radius: 6
        color: "#232325"
        border.color: "#5A5B5E"
    }

    FileDialog {
        id: fileDialog
        title: "Import amp captures"
        fileMode: FileDialog.OpenFiles
        nameFilters: ["Amp captures (*.nam)"]
        onAccepted: picker.captures.importUrls(selectedFiles)
    }

    DropArea {
        anchors.fill: parent
        keys: ["text/uri-list"]
        onDropped: event => {
            picker.captures.importUrls(event.urls)
            event.accept()
        }
    }

    Column {
        anchors.fill: parent
        spacing: 10

        Row {
            width: parent.width
            spacing: 10
            Label {
                width: parent.width - importButton.width - closeButton.width - 20
                anchors.verticalCenter: parent.verticalCenter
                text: "CHANNEL " + (picker.channel + 1) + " · AMP"
                font.pixelSize: 14
                color: "#E9DFC4"
            }
            PanelButton {
                id: importButton
                width: 90
                height: 30
                dark: true
                faceText: "IMPORT"
                text: "Import .nam amp captures"
                onClicked: fileDialog.open()
            }
            PanelButton {
                id: closeButton
                width: 60
                height: 30
                dark: true
                faceText: "CLOSE"
                onClicked: picker.close()
            }
        }
        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            text: "A capture is a real amp, modelled (a .nam file): GAIN drives it, BASS to PRESENCE shape it after, "
                  + "your cabinet IRs follow; one with its cabinet in it (a full rig) switches the channel's CABINET off. "
                  + (picker.captures.message !== "" ? "\n" + picker.captures.message : "")
            font.pixelSize: 11
            color: "#A9A392"
        }

        ListView {
            id: list
            width: parent.width
            height: parent.height - y
            clip: true
            model: [""].concat(picker.captures.revision >= 0 ? picker.captures.names : [])
            ScrollBar.vertical: ScrollBar {}
            delegate: Rectangle {
                id: row
                required property string modelData
                width: ListView.view.width
                height: 34
                radius: 3
                color: picker.current === modelData ? "#4A3B22" : (area.containsMouse ? "#2E2E31" : "transparent")
                Text {
                    x: 10
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - 20
                    elide: Text.ElideRight
                    text: row.modelData === "" ? "No amp (the channel is silent)"
                        : row.modelData + (picker.captures.hasCabinet(row.modelData) ? "   ·  with its cabinet (CABINET goes off)" : "")
                    color: picker.current === row.modelData ? "#F2C230" : "#E9DFC4"
                    font.family: Theme.labelFont
                    font.pixelSize: 13
                    font.bold: picker.current === row.modelData
                }
                MouseArea {
                    id: area
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: picker.captures.choose(picker.channel, row.modelData)
                }
            }
            Label {
                anchors.centerIn: parent
                visible: picker.captures.names.length === 0
                width: parent.width - 40
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: "No captures yet. IMPORT .nam files, drop them here, or put them in " + picker.captures.folder
                      + " (Krate Remote: Files > Places > Amp captures)."
                font.pixelSize: 12
                color: "#8F897A"
            }
        }
    }
}

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import Krate

// Picks a channel's cabinet IR (A or B) from the IR library. A click plays
// it at once, so you can play and compare; the library fills from WAV
// files, folders or zip packs (IMPORT, or drop them here).
Popup {
    id: picker

    property int channel: 0
    property bool slotB: false
    property string filter: ""

    readonly property string current: Cabinets.revision >= 0 ? Cabinets.irId(channel, slotB) : ""
    readonly property var entries: Cabinets.revision >= 0 ? Cabinets.irs(filter) : []

    width: 640
    height: 560
    padding: 14
    modal: true
    background: Rectangle {
        radius: 6
        color: "#232325"
        border.color: "#5A5B5E"
    }

    FileDialog {
        id: fileDialog
        title: "Import impulse responses"
        fileMode: FileDialog.OpenFiles
        nameFilters: ["Impulse responses (*.wav *.zip)", "WAV files (*.wav)", "Zip packs (*.zip)"]
        onAccepted: Cabinets.importUrls(selectedFiles)
    }
    FolderDialog {
        id: folderDialog
        title: "Import a folder of impulse responses"
        onAccepted: Cabinets.importUrls([selectedFolder])
    }

    DropArea {
        anchors.fill: parent
        keys: ["text/uri-list"]
        onDropped: event => {
            Cabinets.importUrls(event.urls)
            event.accept()
        }
    }

    Column {
        anchors.fill: parent
        spacing: 10

        Row {
            width: parent.width
            spacing: 10
            Text {
                width: parent.width - 100
                anchors.verticalCenter: parent.verticalCenter
                text: "CHANNEL " + (picker.channel + 1) + "  ·  CABINET " + (picker.slotB ? "B" : "A")
                color: "#E9DFC4"
                font.family: Theme.labelFont
                font.pixelSize: 15
                font.bold: true
            }
            PanelButton {
                width: 90
                height: 30
                faceText: "DONE"
                text: "Close"
                onClicked: picker.close()
            }
        }

        TextField {
            id: search
            width: parent.width
            placeholderText: "Search: pack, folder, name (e.g. 57 bright)"
            onTextChanged: picker.filter = text
            Accessible.name: "Search impulse responses"
        }

        ListView {
            id: list
            width: parent.width
            height: parent.height - 190
            clip: true
            model: [{ id: "", name: picker.slotB ? "None" : "Built-in speaker", pack: "", folder: "", channels: 0, favourite: false }]
                   .concat(picker.entries)
            ScrollBar.vertical: ScrollBar {}

            delegate: Rectangle {
                id: row
                required property var modelData
                required property int index
                readonly property bool chosen: modelData.id === picker.current
                width: list.width - 12
                height: 44
                radius: 4
                color: chosen ? "#3B4A5A" : (mouse.containsMouse ? "#2E2F32" : "transparent")

                MouseArea {
                    id: mouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: Cabinets.choose(picker.channel, picker.slotB, row.modelData.id)
                    onDoubleClicked: picker.close()
                }
                Text { // favourite
                    x: 8
                    anchors.verticalCenter: parent.verticalCenter
                    visible: row.modelData.id !== ""
                    text: row.modelData.favourite ? "★" : "☆"
                    color: row.modelData.favourite ? "#F2C230" : "#6A6C70"
                    font.pixelSize: 20
                    MouseArea {
                        anchors.fill: parent
                        anchors.margins: -6
                        onClicked: Cabinets.setFavourite(row.modelData.id, !row.modelData.favourite)
                    }
                }
                Column {
                    x: 40
                    width: parent.width - 120
                    anchors.verticalCenter: parent.verticalCenter
                    Text {
                        width: parent.width
                        text: row.modelData.name
                        color: "#F2F2F2"
                        font.family: Theme.labelFont
                        font.pixelSize: 14
                        font.bold: row.chosen
                        elide: Text.ElideRight
                    }
                    Text {
                        width: parent.width
                        visible: text !== ""
                        text: [row.modelData.pack, row.modelData.folder].filter(s => s).join("  ›  ")
                        color: "#9A9488"
                        font.family: Theme.labelFont
                        font.pixelSize: 11
                        elide: Text.ElideMiddle
                    }
                }
                Text {
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    visible: row.modelData.channels > 0
                    text: (row.modelData.channels > 1 ? "STEREO  " : "") + Math.round((row.modelData.seconds ?? 0) * 1000) + " ms"
                    color: "#9A9488"
                    font.family: Theme.labelFont
                    font.pixelSize: 11
                }
            }
        }

        Text {
            width: parent.width
            visible: picker.entries.length === 0
            text: picker.filter === "" ? "The library is empty: import your IRs (WAV files, folders or zip packs) below, or drop them here."
                                       : "Nothing matches."
            color: "#B9B2A0"
            font.family: Theme.labelFont
            font.pixelSize: 13
            wrapMode: Text.WordWrap
        }

        Row {
            spacing: 10
            PanelButton {
                width: 150
                height: 32
                faceText: "IMPORT FILES…"
                text: "Import WAV files or zip packs into the IR library"
                onClicked: fileDialog.open()
            }
            PanelButton {
                width: 150
                height: 32
                faceText: "IMPORT FOLDER…"
                text: "Import a folder of IRs (WAVs and zips, with its subfolders)"
                onClicked: folderDialog.open()
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: Cabinets.libraryCount + " IRs in the library"
                color: "#9A9488"
                font.family: Theme.labelFont
                font.pixelSize: 12
            }
        }
        Text {
            width: parent.width
            text: Cabinets.importMessage
            color: "#E9DFC4"
            font.family: Theme.labelFont
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }
    }
}

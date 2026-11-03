pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import Krate

// Picks a channel's amp: Krunch's own tubes (as its VOICE, or built as an amp
// head: docs/HEADS.md, the heads folder's .json files), or a capture of a real
// amp (docs/CAPTURES.md): the .nam files in the captures folder. A click plays
// it at once. IMPORT (or a drop) copies .nam and head files in; Krate Remote
// can put them there too (Files > Places > Amp captures, Amp heads).
Popup {
    id: picker
    required property Captures captures
    required property Heads heads

    property int channel: 0
    readonly property string current: picker.captures.revision >= 0 ? picker.captures.capture(channel) : ""
    readonly property string currentHead: picker.heads.revision >= 0 ? picker.heads.head(channel) : ""
    // The list: Krunch's tubes as the VOICE, the heads, the captures.
    readonly property var entries: {
        const list = [{ kind: "tubes", name: "" }]
        for (const h of (picker.heads.revision >= 0 ? picker.heads.names : []))
            list.push({ kind: "head", name: h })
        for (const c of (picker.captures.revision >= 0 ? picker.captures.names : []))
            list.push({ kind: "capture", name: c })
        return list
    }
    function chosen(entry) {
        return entry.kind === "tubes" ? current === "" && currentHead === ""
             : entry.kind === "head" ? current === "" && currentHead === entry.name
             : current === entry.name
    }
    function choose(entry) {
        // One or the other: a head is Krunch's tubes built otherwise; a capture replaces them.
        picker.captures.choose(channel, entry.kind === "capture" ? entry.name : "")
        picker.heads.choose(channel, entry.kind === "head" ? entry.name : "")
    }
    function importUrls(urls) {
        const nam = [], json = []
        for (const u of urls)
            (String(u).toLowerCase().endsWith(".json") ? json : nam).push(u)
        if (nam.length > 0)
            picker.captures.importUrls(nam)
        if (json.length > 0)
            picker.heads.importUrls(json)
    }

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
        title: "Import amp captures and heads"
        fileMode: FileDialog.OpenFiles
        nameFilters: ["Amp captures and heads (*.nam *.json)", "Amp captures (*.nam)", "Amp heads (*.json)"]
        onAccepted: picker.importUrls(selectedFiles)
    }

    DropArea {
        anchors.fill: parent
        keys: ["text/uri-list"]
        onDropped: event => {
            picker.importUrls(event.urls)
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
                text: "Import amp captures (.nam) and amp heads (.json)"
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
            text: "A head is Krunch's own tubes built like a real amp (fitted to a capture): every knob works as on an amp. "
                  + "A capture is a real amp, modelled (a .nam file): it plays in place of Krunch's preamp and power amp; "
                  + "GAIN drives it, BASS to PRESENCE shape it after. Your cabinet IRs follow either."
                  + (picker.captures.message !== "" ? "\n" + picker.captures.message : "")
                  + (picker.heads.message !== "" ? "\n" + picker.heads.message : "")
            font.pixelSize: 11
            color: "#A9A392"
        }

        ListView {
            id: list
            width: parent.width
            height: parent.height - y
            clip: true
            model: picker.entries
            ScrollBar.vertical: ScrollBar {}
            section.property: "kind"
            section.delegate: Text {
                required property string section
                width: ListView.view.width
                height: section === "tubes" ? 0 : 30
                visible: section !== "tubes"
                verticalAlignment: Text.AlignBottom
                leftPadding: 4
                bottomPadding: 4
                text: section === "head" ? "AMP HEADS (KRUNCH'S TUBES, BUILT AS)" : "CAPTURES (IN PLACE OF THE TUBES)"
                color: "#8F897A"
                font.family: Theme.labelFont
                font.pixelSize: 11
                font.bold: true
            }
            delegate: Rectangle {
                id: row
                required property var modelData
                readonly property bool isChosen: picker.chosen(modelData)
                width: ListView.view.width
                height: modelData.kind === "head" && picker.heads.about(modelData.name) !== "" ? 46 : 34
                radius: 3
                color: isChosen ? "#4A3B22" : (area.containsMouse ? "#2E2E31" : "transparent")
                Column {
                    x: 10
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - 20
                    Text {
                        width: parent.width
                        elide: Text.ElideRight
                        text: row.modelData.kind === "tubes" ? "Krunch's own amp (its tubes, as the VOICE knob)"
                            : row.modelData.kind === "head" ? picker.heads.title(row.modelData.name)
                            : row.modelData.name
                        color: row.isChosen ? "#F2C230" : "#E9DFC4"
                        font.family: Theme.labelFont
                        font.pixelSize: 13
                        font.bold: row.isChosen
                    }
                    Text {
                        visible: row.modelData.kind === "head" && text !== ""
                        width: parent.width
                        elide: Text.ElideRight
                        text: row.modelData.kind === "head" ? picker.heads.about(row.modelData.name) : ""
                        color: "#8F897A"
                        font.family: Theme.labelFont
                        font.pixelSize: 10
                    }
                }
                MouseArea {
                    id: area
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: picker.choose(row.modelData)
                }
            }
            Label {
                anchors.centerIn: parent
                visible: picker.entries.length === 1
                width: parent.width - 40
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: "No heads or captures yet. IMPORT .json heads or .nam captures, drop them here, or put them in "
                      + picker.heads.folder + " and " + picker.captures.folder + " (Krate Remote: Files > Places)."
                font.pixelSize: 12
                color: "#8F897A"
            }
        }
    }
}

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import Krate
import KrateRack

// The sound library (docs/SOUNDS.md): your folders of sounds, indexed where
// they are, by category; a click plays a sound (the arrows step through and
// play each), the star keeps it as a favourite; SEND TO puts it on one of
// Klang's pads or Kessel's drums (copied in). A sound's category was guessed
// from its names: the category buttons set it.
Item {
    id: screen

    property int category: -1      // -1: all
    property bool favourites: false
    property string search: ""
    property int selected: -1      // a sound's id
    property int targetDevice: -1  // the instrument SEND TO shows
    readonly property int rev: Sounds.revision
    readonly property var rows: rev >= 0 ? Sounds.list(category, favourites, search) : []
    readonly property var current: rev >= 0 && selected >= 0 ? Sounds.sound(selected) : ({})
    readonly property var instruments: rev >= 0 && Rack.present.length > 0 ? Sounds.instruments() : []
    readonly property var target: {
        for (const i of instruments)
            if (i.device === targetDevice)
                return i
        return instruments.length > 0 ? instruments[0] : null
    }

    function lengthText(seconds) {
        if (!(seconds > 0))
            return "—"
        return seconds < 10 ? seconds.toFixed(2) + " s" : Math.floor(seconds / 60) + ":" + String(Math.round(seconds % 60)).padStart(2, "0")
    }
    function choose(index) {
        if (index < 0 || index >= rows.length)
            return
        list.currentIndex = index
        selected = rows[index].id
        Sounds.preview(selected)
    }

    Rectangle { // the deck
        anchors.fill: parent
        radius: 6
        color: Theme.lowerDeck
        border.color: Theme.deckLine
    }

    FolderDialog {
        id: folderDialog
        title: "A folder of sounds for the library"
        onAccepted: Sounds.addFolder(selectedFolder)
    }

    // --- Left: folders and categories -------------------------------------------------
    Column {
        id: left
        x: 16
        y: 14
        width: 250
        spacing: 6

        Text {
            text: "FOLDERS"
            color: Theme.ink
            font.family: Theme.labelFont
            font.pixelSize: 12
            font.bold: true
        }
        Row {
            spacing: 6
            PanelButton {
                width: 140
                height: 30
                faceText: "ADD FOLDER"
                text: "Add a folder of sounds (indexed where it is, nothing copied)"
                onClicked: folderDialog.open()
            }
            PanelButton {
                width: 100
                height: 30
                faceText: Sounds.scanning ? "LOOKING…" : "RESCAN"
                text: "Look through the folders again for new or removed sounds"
                enabled: !Sounds.scanning
                onClicked: Sounds.rescan()
            }
        }
        Repeater {
            model: screen.rev >= 0 ? Sounds.folders : []
            Rectangle {
                id: folderRow
                required property string modelData
                width: left.width
                height: 26
                radius: 3
                color: Theme.silverLight
                border.color: Theme.deckLine
                Text {
                    x: 6
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - 36
                    elide: Text.ElideMiddle
                    text: Sounds.ownFolders[folderRow.modelData] !== undefined ? Sounds.ownFolders[folderRow.modelData]
                                                                    : folderRow.modelData.replace(/^\/home\/[^/]+/, "~")
                    color: Theme.ink
                    font.family: Theme.labelFont
                    font.pixelSize: 12
                }
                Text {
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    visible: Sounds.ownFolders[folderRow.modelData] === undefined // Krate's own: always there
                    text: "✕"
                    color: Theme.ink
                    MouseArea {
                        anchors.fill: parent
                        anchors.margins: -6
                        onClicked: Sounds.removeFolder(folderRow.modelData)
                    }
                }
            }
        }

        Item { width: 1; height: 8 }
        Text {
            text: "CATEGORIES"
            color: Theme.ink
            font.family: Theme.labelFont
            font.pixelSize: 12
            font.bold: true
        }
        Repeater {
            // All, the favourites, then each category with its count.
            model: [{ name: "All", category: -1, favourites: false, count: Sounds.total },
                    { name: "★ Favourites", category: -1, favourites: true, count: Sounds.favouriteCount }]
                   .concat(Sounds.categoryNames.map((n, i) => ({ name: n, category: i, favourites: false,
                                                                 count: Sounds.categoryCounts[i] ?? 0 })))
            Rectangle {
                id: catRow
                required property var modelData
                readonly property bool chosen: screen.category === modelData.category && screen.favourites === modelData.favourites
                width: left.width
                height: 24
                radius: 3
                color: chosen ? Theme.accentUi : (catMouse.containsMouse ? Theme.silverLight : "transparent")
                Text {
                    x: 8
                    anchors.verticalCenter: parent.verticalCenter
                    text: catRow.modelData.name
                    color: catRow.chosen ? Theme.inkInverse : Theme.ink
                    opacity: catRow.modelData.count > 0 || catRow.chosen ? 1 : 0.45
                    font.family: Theme.labelFont
                    font.pixelSize: 14
                    font.bold: catRow.chosen
                }
                Text {
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    text: catRow.modelData.count
                    color: catRow.chosen ? Theme.inkInverse : Theme.ink
                    opacity: 0.7
                    font.family: "monospace"
                    font.pixelSize: 12
                }
                MouseArea {
                    id: catMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        screen.category = catRow.modelData.category
                        screen.favourites = catRow.modelData.favourites
                    }
                }
            }
        }
    }

    // --- Middle: search and the sounds -------------------------------------------------
    Item {
        id: middle
        x: left.x + left.width + 16
        y: 14
        width: screen.width - x - right.width - 32
        height: screen.height - 28

        TextField {
            id: searchField
            width: parent.width
            height: 34
            placeholderText: "Search names and folders"
            font.family: Theme.labelFont
            font.pixelSize: 15
            onTextChanged: screen.search = text
            Keys.onDownPressed: { list.forceActiveFocus(); screen.choose(Math.max(0, list.currentIndex)) }
        }
        Text {
            id: status
            y: searchField.height + 6
            width: parent.width
            elide: Text.ElideRight
            text: (Sounds.message !== "" ? Sounds.message + "   " : "") + screen.rows.length + " shown"
            color: Theme.ink
            opacity: 0.75
            font.family: Theme.labelFont
            font.pixelSize: 12
        }
        Rectangle {
            y: status.y + status.height + 6
            width: parent.width
            height: parent.height - y
            radius: 4
            color: Theme.lcdBg
            border.color: Theme.deckLine
            clip: true

            ListView {
                id: list
                anchors.fill: parent
                anchors.margins: 4
                model: screen.rows
                currentIndex: -1
                focus: true
                keyNavigationEnabled: false
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                Keys.onUpPressed: screen.choose(currentIndex - 1)
                Keys.onDownPressed: screen.choose(currentIndex + 1)
                Keys.onSpacePressed: Sounds.previewing >= 0 ? Sounds.stopPreview() : Sounds.preview(screen.selected)
                delegate: Rectangle {
                    id: row
                    required property int index
                    required property var modelData
                    readonly property bool chosen: screen.selected === modelData.id
                    width: list.width - 10
                    height: 30
                    radius: 3
                    color: chosen ? Theme.accentUi : (index % 2 ? "#D2D5CE" : "transparent")
                    Led {
                        x: 6
                        anchors.verticalCenter: parent.verticalCenter
                        width: 10
                        height: 10
                        on: Sounds.previewing === row.modelData.id
                    }
                    Text { // favourite
                        x: 22
                        anchors.verticalCenter: parent.verticalCenter
                        text: row.modelData.favorite ? "★" : "☆"
                        color: row.chosen ? Theme.inkInverse : Theme.ink
                        font.pixelSize: 16
                        MouseArea {
                            anchors.fill: parent
                            anchors.margins: -4
                            onClicked: Sounds.setFavourite(row.modelData.id, !row.modelData.favorite)
                        }
                    }
                    Text {
                        x: 44
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width * 0.42
                        elide: Text.ElideRight
                        text: row.modelData.name
                        color: row.chosen ? Theme.inkInverse : Theme.lcdInk
                        font.family: Theme.labelFont
                        font.pixelSize: 14
                        font.bold: row.chosen
                    }
                    Text {
                        x: 44 + parent.width * 0.42 + 10
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width * 0.28
                        elide: Text.ElideMiddle
                        text: row.modelData.dir
                        color: row.chosen ? Theme.inkInverse : Theme.lcdInk
                        opacity: 0.65
                        font.family: Theme.labelFont
                        font.pixelSize: 12
                    }
                    Text {
                        anchors.right: lengthLabel.left
                        anchors.rightMargin: 12
                        anchors.verticalCenter: parent.verticalCenter
                        text: Sounds.categoryNames[row.modelData.category] ?? ""
                        color: row.chosen ? Theme.inkInverse : Theme.lcdInk
                        font.family: Theme.labelFont
                        font.pixelSize: 12
                        font.bold: true
                    }
                    Text {
                        id: lengthLabel
                        anchors.right: parent.right
                        anchors.rightMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        width: 56
                        horizontalAlignment: Text.AlignRight
                        text: screen.lengthText(row.modelData.seconds)
                        color: row.chosen ? Theme.inkInverse : Theme.lcdInk
                        font.family: "monospace"
                        font.pixelSize: 12
                    }
                    MouseArea {
                        anchors.fill: parent
                        anchors.leftMargin: 40
                        onClicked: {
                            list.forceActiveFocus()
                            screen.choose(row.index)
                        }
                    }
                }
            }
            Text { // nothing yet
                anchors.centerIn: parent
                width: parent.width - 80
                visible: Sounds.total === 0 && !Sounds.scanning
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                text: "No sounds yet. ADD FOLDER points the library at a folder of WAV or MP3 files (sample packs, your drum sounds); they stay where they are. Or put them in Krate's sounds folder (Krate Remote: Places ▸ Sounds): they show up by themselves."
                color: Theme.lcdInk
                font.family: Theme.labelFont
                font.pixelSize: 16
            }
        }
    }

    // --- Right: the sound chosen, its category, where it goes -------------------------
    Column {
        id: right
        x: screen.width - width - 16
        y: 14
        width: 400
        spacing: 8

        Text {
            width: parent.width
            elide: Text.ElideRight
            text: screen.current.name ?? "Pick a sound"
            color: Theme.ink
            font.family: Theme.labelFont
            font.pixelSize: 20
            font.bold: true
        }
        Text {
            width: parent.width
            elide: Text.ElideMiddle
            visible: screen.selected >= 0
            text: (screen.current.dir ? screen.current.dir + " · " : "") + screen.lengthText(screen.current.seconds)
            color: Theme.ink
            opacity: 0.7
            font.family: Theme.labelFont
            font.pixelSize: 12
        }
        Rectangle {
            width: parent.width
            height: 110
            radius: 4
            color: Theme.band
            border.color: Theme.deckLine
            Canvas {
                id: wave
                anchors.fill: parent
                anchors.margins: 4
                readonly property var peaks: screen.selected >= 0 ? Sounds.waveform(screen.selected, 380) : []
                onPeaksChanged: requestPaint()
                onPaint: {
                    const ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    const n = peaks.length
                    if (n === 0)
                        return
                    ctx.fillStyle = Theme.labelOrange
                    const mid = height / 2
                    for (let x = 0; x < width; ++x) {
                        const p = peaks[Math.min(n - 1, Math.floor(x * n / width))]
                        const h = Math.max(0.5, p * mid)
                        ctx.fillRect(x, mid - h, 1, 2 * h)
                    }
                }
            }
        }
        PanelButton {
            width: 120
            height: 32
            faceText: Sounds.previewing >= 0 ? "STOP" : "PLAY"
            text: "Play the sound (Space)"
            enabled: screen.selected >= 0
            opacity: enabled ? 1 : 0.5
            onClicked: Sounds.previewing >= 0 ? Sounds.stopPreview() : Sounds.preview(screen.selected)
        }

        Text {
            text: "CATEGORY"
            color: Theme.ink
            font.family: Theme.labelFont
            font.pixelSize: 12
            font.bold: true
        }
        Grid {
            columns: 5
            spacing: 4
            Repeater {
                model: Sounds.categoryNames
                Rectangle {
                    id: catButton
                    required property int index
                    required property string modelData
                    readonly property bool chosen: screen.current.category === index
                    width: 76
                    height: 24
                    radius: 3
                    color: chosen ? Theme.labelOrange : Theme.silverLight
                    border.color: Theme.deckLine
                    opacity: screen.selected >= 0 ? 1 : 0.5
                    Text {
                        anchors.centerIn: parent
                        text: catButton.modelData
                        color: Theme.ink
                        font.family: Theme.labelFont
                        font.pixelSize: 11
                        font.bold: catButton.chosen
                    }
                    MouseArea {
                        anchors.fill: parent
                        enabled: screen.selected >= 0
                        onClicked: Sounds.setCategory(screen.selected, catButton.index)
                    }
                }
            }
        }

        Text {
            text: "SEND TO"
            color: Theme.ink
            font.family: Theme.labelFont
            font.pixelSize: 12
            font.bold: true
        }
        Row {
            spacing: 6
            Repeater {
                model: screen.instruments
                PanelButton {
                    required property var modelData
                    width: 96
                    height: 30
                    faceText: modelData.name.toUpperCase()
                    text: "Send sounds to " + modelData.name
                    tint: screen.target && screen.target.device === modelData.device ? Theme.labelOrange : "transparent"
                    onClicked: screen.targetDevice = modelData.device
                }
            }
        }
        Text {
            width: parent.width
            visible: screen.instruments.length === 0
            wrapMode: Text.WordWrap
            text: "Add Klang or Kessel to the rack (+) to send sounds to them."
            color: Theme.ink
            opacity: 0.7
            font.family: Theme.labelFont
            font.pixelSize: 12
        }
        Grid { // its pads or drums: a click puts the sound there, a right click empties it
            columns: 4
            spacing: 4
            visible: screen.target !== null
            Repeater {
                model: screen.target ? screen.target.targets : []
                Rectangle {
                    id: slotBox
                    required property int index
                    required property string modelData
                    readonly property string holds: screen.target ? (screen.target.contents[index] ?? "") : ""
                    width: 97
                    height: 46
                    radius: 4
                    color: holds !== "" ? "#E8D9C0" : Theme.silverLight
                    border.color: slotMouse.containsMouse && screen.selected >= 0 ? Theme.accentUi : Theme.deckLine
                    border.width: slotMouse.containsMouse && screen.selected >= 0 ? 2 : 1
                    Text {
                        x: 6
                        y: 4
                        width: parent.width - 12
                        elide: Text.ElideRight
                        text: slotBox.modelData
                        color: Theme.ink
                        font.family: Theme.labelFont
                        font.pixelSize: 12
                        font.bold: true
                    }
                    Text {
                        x: 6
                        y: 24
                        width: parent.width - 12
                        elide: Text.ElideRight
                        text: slotBox.holds !== "" ? slotBox.holds.replace(/\.(wav|mp3)$/i, "") : "—"
                        color: Theme.ink
                        opacity: 0.7
                        font.family: Theme.labelFont
                        font.pixelSize: 11
                    }
                    MouseArea {
                        id: slotMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        onClicked: mouse => {
                            if (mouse.button === Qt.RightButton)
                                Sounds.clearTarget(screen.target.device, slotBox.index)
                            else if (screen.selected >= 0)
                                Sounds.assign(screen.selected, screen.target.device, slotBox.index)
                        }
                    }
                    Accessible.role: Accessible.Button
                    Accessible.name: "Put the sound on " + slotBox.modelData
                }
            }
        }
        Text {
            width: parent.width
            visible: screen.target !== null
            wrapMode: Text.WordWrap
            text: "A click puts the sound there (copied into the instrument); a right click empties it (a Kessel drum: its own sound again)."
            color: Theme.ink
            opacity: 0.7
            font.family: Theme.labelFont
            font.pixelSize: 11
        }
    }
}

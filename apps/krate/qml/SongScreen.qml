pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import Krate
import KrateRack

// The Song screen (docs/SONG_PLAYER.md): open a .gp tab file and/or a .mid
// file (or drop them here), give each track an instrument, and play along
// with your track's tab, which scrolls with the song.
Item {
    id: screen

    FileDialog {
        id: openDialog
        title: "Open a song"
        fileMode: FileDialog.OpenFiles
        nameFilters: ["Songs (*.gp *.mid *.midi)", "Tab files (*.gp)", "MIDI files (*.mid *.midi)"]
        onAccepted: Song.open(selectedFiles)
    }

    FileDialog {
        id: backingDialog
        title: "A backing track for this song"
        fileMode: FileDialog.OpenFile
        nameFilters: ["MP3 files (*.mp3)"]
        onAccepted: Song.loadBacking(selectedFile)
    }

    DropArea {
        id: drop
        anchors.fill: parent
        keys: ["text/uri-list"]
        onDropped: event => {
            // An MP3 dropped on an open song is its backing track; anything else opens.
            const mp3 = event.urls.length === 1 && event.urls[0].toString().toLowerCase().endsWith(".mp3")
            if (mp3 && Song.loaded)
                Song.loadBacking(event.urls[0])
            else
                Song.open(event.urls)
            event.accept()
        }
    }

    // "m:ss.mmm" for an offset in seconds (negative: before the recording starts).
    function clockText(seconds) {
        const sign = seconds < 0 ? "-" : ""
        const t = Math.abs(seconds)
        const m = Math.floor(t / 60)
        const s = t - m * 60
        return sign + m + ":" + (s < 10 ? "0" : "") + s.toFixed(3)
    }
    // Seconds from "m:ss.mmm" or plain seconds; NaN if it isn't either.
    function parseClock(text) {
        const t = text.trim()
        const negative = t.startsWith("-")
        const body = negative ? t.slice(1) : t
        const parts = body.split(":")
        let seconds = NaN
        if (parts.length === 1)
            seconds = Number(parts[0])
        else if (parts.length === 2 && parts[0] !== "" && parts[1] !== "")
            seconds = Number(parts[0]) * 60 + Number(parts[1])
        return negative ? -seconds : seconds
    }

    Rectangle { // the deck
        anchors.fill: parent
        radius: 6
        color: Theme.lowerDeck
        border.color: drop.containsDrag ? Theme.accentUi : Theme.deckLine
        border.width: drop.containsDrag ? 3 : 1
    }

    // The library on the left (always there while no song is open).
    property bool libraryShown: !Song.loaded
    // The track list folded away, so the tab gets the whole screen.
    property bool tracksCollapsed: false

    LibraryPanel {
        id: library
        x: 12
        y: 12
        width: 300
        height: parent.height - 24
        visible: screen.libraryShown
    }

    Item { // the song: header, tracks, tab
        id: main
        x: library.visible ? library.x + library.width : 0
        width: screen.width - x
        height: screen.height

        // --- Top: the song and the transport ------------------------------------------
        Item {
            id: header
            x: 16
            y: 12
            width: parent.width - 32
            height: 56

            PanelButton {
                id: libraryButton
                width: 84
                height: 36
                anchors.verticalCenter: parent.verticalCenter
                faceText: "LIBRARY"
                text: "Show the song library"
                dark: screen.libraryShown
                onClicked: screen.libraryShown = !screen.libraryShown
            }
            PanelButton {
                id: openButton
                x: libraryButton.width + 8
                width: 72
                height: 36
                anchors.verticalCenter: parent.verticalCenter
                faceText: "OPEN"
                text: "Open a song from a file"
                onClicked: openDialog.open()
            }
            PanelButton {
                id: editButton
                x: openButton.x + openButton.width + 8
                width: 60
                height: 36
                anchors.verticalCenter: parent.verticalCenter
                visible: Song.songId > 0
                faceText: "EDIT"
                text: "Edit the song's details"
                onClicked: editor.open()
            }

            Column {
                x: (editButton.visible ? editButton.x + editButton.width : openButton.x + openButton.width) + 16
                width: parent.width - x - transport.width - 16
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2
                Text {
                    width: parent.width
                    text: Song.loaded ? Song.title + (Song.artist ? " — " + Song.artist : "") : "No song"
                    color: Theme.ink
                    font.family: Theme.labelFont
                    font.pixelSize: 22
                    font.bold: true
                    elide: Text.ElideRight
                }
                Text {
                    width: parent.width
                    text: Song.error !== "" ? Song.error
                        : Song.files + (Song.loaded ? (Song.soundsSaved ? "  ·  sounds saved" : "  ·  sounds not saved") : "")
                    color: Song.error !== "" ? "#B02020" : "#55575B"
                    font.family: Theme.labelFont
                    font.pixelSize: 12
                    elide: Text.ElideLeft // the note at the end matters more than the file names
                }
            }

            Row {
                id: transport
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                spacing: 10
                visible: Song.loaded

                PanelButton {
                    width: 48
                    height: 36
                    faceText: "|◂"
                    text: "Back to the start"
                    onClicked: Song.backToStart()
                }
                PanelButton {
                    width: 84
                    height: 36
                    faceText: Rack.playing ? "STOP" : "PLAY"
                    text: Rack.playing ? "Stop" : "Play"
                    dark: Rack.playing
                    onClicked: Rack.playing = !Rack.playing
                }
                Rectangle { // position
                    width: positionText.implicitWidth + 24
                    height: 36
                    radius: 4
                    color: Theme.lcdBg
                    border.color: Theme.deckLine
                    Text {
                        id: positionText
                        anchors.centerIn: parent
                        text: "BAR " + Song.bar + "." + Song.beat + " / " + Song.barCount + "   " + Song.time
                        color: Theme.lcdInk
                        font.family: "monospace"
                        font.pixelSize: 14
                    }
                    Accessible.role: Accessible.StaticText
                    Accessible.name: "Bar " + Song.bar + " beat " + Song.beat + " of " + Song.barCount
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: Math.round(Rack.bpm) + " BPM"
                    color: Theme.ink
                    font.family: Theme.labelFont
                    font.pixelSize: 14
                }
                PanelButton {
                    width: 64
                    height: 36
                    faceText: "SAVE"
                    text: "Save the instruments' sounds and the mixer with this song"
                    tint: Song.soundsSaved ? "transparent" : Theme.labelOrange
                    onClicked: Song.saveSounds()
                }
                PanelButton {
                    width: 64
                    height: 36
                    faceText: "CLOSE"
                    text: "Close the song"
                    onClicked: Song.close()
                }
            }
        }

        // --- No song yet ----------------------------------------------------------------
        Column {
            anchors.centerIn: parent
            spacing: 10
            visible: !Song.loaded
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Drop a .gp tab file and/or a .mid file here"
                color: Theme.ink
                font.family: Theme.labelFont
                font.pixelSize: 24
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "or press OPEN. With both files of a song, the tab shows your part and either file can play each track."
                color: "#55575B"
                font.family: Theme.labelFont
                font.pixelSize: 14
            }
        }

        // --- Backing track ------------------------------------------------------------------
        // An MP3 playing along with the song, in sync: BAR 1 AT is where the
        // song's bar 1 is in the recording. Its level is the mixer's Backing strip.
        Rectangle {
            id: backingBar
            x: 16
            y: header.y + header.height + 8
            width: parent.width - 32
            height: 40
            radius: 4
            visible: Song.loaded
            color: Theme.silverLight
            border.color: Theme.deckLine

            Row {
                x: 10
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "BACKING"
                    color: Theme.ink
                    font.family: Theme.labelFont
                    font.pixelSize: 12
                    font.bold: true
                }
                PanelButton {
                    visible: Song.backingName === "" && !Song.backingLoading
                    width: 110
                    height: 28
                    faceText: "LOAD MP3…"
                    text: "Choose an MP3 to play along with this song"
                    onClicked: backingDialog.open()
                }
                Text {
                    visible: Song.backingName === "" || Song.backingLoading
                    anchors.verticalCenter: parent.verticalCenter
                    text: Song.backingLoading ? "Reading the recording…" : "an MP3 to play along with, in sync (or drop one here)"
                    color: "#55575B"
                    font.family: Theme.labelFont
                    font.pixelSize: 12
                }
                Text {
                    visible: Song.backingName !== "" && !Song.backingLoading
                    width: 230
                    anchors.verticalCenter: parent.verticalCenter
                    text: Song.backingName + "  ·  " + screen.clockText(Song.backingSeconds).replace(/\.\d+$/, "")
                    color: Theme.ink
                    font.family: Theme.labelFont
                    font.pixelSize: 12
                    elide: Text.ElideMiddle
                }

                Row { // sync
                    visible: Song.backingName !== "" && !Song.backingLoading
                    spacing: 4
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "BAR 1 AT"
                        color: Theme.ink
                        font.family: Theme.labelFont
                        font.pixelSize: 11
                        font.bold: true
                        rightPadding: 4
                    }
                    TextField {
                        id: offsetField
                        width: 96
                        height: 28
                        anchors.verticalCenter: parent.verticalCenter
                        font.family: "monospace"
                        font.pixelSize: 13
                        horizontalAlignment: TextInput.AlignRight
                        text: screen.clockText(Song.backingOffset)
                        onEditingFinished: {
                            const seconds = screen.parseClock(text)
                            if (!isNaN(seconds))
                                Song.backingOffset = seconds
                            text = Qt.binding(() => screen.clockText(Song.backingOffset))
                        }
                        Accessible.name: "Where bar 1 is in the recording"
                    }
                    Repeater { // nudges: a beat, a tenth, a hundredth, each way
                        model: [
                            { label: "−BEAT", by: -1, beat: true }, { label: "−0.1", by: -0.1 }, { label: "−0.01", by: -0.01 },
                            { label: "+0.01", by: 0.01 }, { label: "+0.1", by: 0.1 }, { label: "+BEAT", by: 1, beat: true }
                        ]
                        PanelButton {
                            required property var modelData
                            width: modelData.beat ? 52 : 44
                            height: 28
                            faceText: modelData.label
                            text: "Move the recording " + (modelData.beat ? "a beat" : Math.abs(modelData.by) + " s")
                                  + (modelData.by < 0 ? " earlier" : " later") + " against the song"
                            onClicked: Song.nudgeBacking(modelData.beat ? modelData.by * Song.beatSeconds : modelData.by)
                        }
                    }
                }
                PanelButton {
                    visible: Song.backingName !== "" && !Song.backingLoading
                    width: 84
                    height: 28
                    faceText: "REPLACE…"
                    text: "Choose another MP3 for this song"
                    onClicked: backingDialog.open()
                }
                PanelButton {
                    visible: Song.backingName !== "" && !Song.backingLoading
                    width: 32
                    height: 28
                    faceText: "×"
                    text: "Remove the backing track from this song"
                    onClicked: Song.clearBacking()
                }
            }
            Text {
                anchors.right: parent.right
                anchors.rightMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                visible: Song.backingName !== "" && !Song.backingLoading && parent.width > 1350
                text: "level: MIXER › Backing"
                color: "#55575B"
                font.family: Theme.labelFont
                font.pixelSize: 11
            }
        }

        // --- Tracks ---------------------------------------------------------------------------
        Row { // column headings
            id: headings
            x: 16
            y: backingBar.y + backingBar.height + 10
            visible: Song.loaded
            spacing: 0
            Text { // folded: just how many
                visible: screen.tracksCollapsed
                text: Song.tracks.length + " TRACKS"
                color: "#55575B"
                font.family: Theme.labelFont
                font.pixelSize: 11
                font.bold: true
            }
            Repeater {
                model: screen.tracksCollapsed ? [] : [["MINE", 56], ["TRACK", list.nameWidth], ["PART", 70], ["NOTES", 64],
                                                     ["INSTRUMENT", 170], ["LEVEL", 150], ["", 92], ["FROM", 80]]
                Text {
                    required property var modelData
                    width: modelData[1]
                    text: modelData[0]
                    color: "#55575B"
                    font.family: Theme.labelFont
                    font.pixelSize: 11
                    font.bold: true
                }
            }
        }

        Rectangle { // fold / unfold the tracks, to the right of FROM
            id: collapseToggle
            x: parent.width - 16 - width
            y: headings.y + (headings.height - height) / 2
            width: 26
            height: 20
            radius: 3
            visible: Song.loaded
            color: toggleArea.containsMouse ? Theme.silverLight : "transparent"
            border.color: Theme.deckLine
            Text {
                anchors.centerIn: parent
                text: screen.tracksCollapsed ? "\u25BE" : "\u25B4" // ▾ show / ▴ hide
                color: Theme.ink
                font.pixelSize: 12
            }
            MouseArea {
                id: toggleArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: screen.tracksCollapsed = !screen.tracksCollapsed
            }
            Accessible.role: Accessible.Button
            Accessible.name: screen.tracksCollapsed ? "Show the tracks" : "Hide the tracks"
        }

        ListView {
            id: list
            readonly property int nameWidth: Math.max(160, width - 56 - 70 - 64 - 170 - 150 - 92 - 80 - 30)
            x: 16
            y: headings.y + headings.height + 6
            width: parent.width - 32
            // The tracks take what they need, up to about a third; the tab gets the rest (all of it, folded).
            height: screen.tracksCollapsed ? 0 : Math.min(contentHeight, Math.max(96, parent.height * 0.34))
            visible: Song.loaded && !screen.tracksCollapsed
            clip: true
            spacing: 4
            model: Song.tracks
            boundsBehavior: Flickable.StopAtBounds

            delegate: Rectangle {
                id: row
                required property int index
                required property var modelData
                width: list.width
                height: 44
                radius: 4
                color: modelData.mine ? "#F4E7D6" : Theme.silverLight
                border.color: modelData.mine ? Theme.labelOrange : Theme.deckLine
                border.width: modelData.mine ? 2 : 1

                MouseArea { // click the row (not its controls) to see this track's tab or notes
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: Song.viewTrack = row.index
                }
                Rectangle { // the track in view
                    visible: row.modelData.viewed
                    x: 0
                    width: 5
                    height: parent.height
                    radius: 2
                    color: Theme.accentUi
                }

                Row {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 0

                    Item { // my track
                        width: 56
                        height: 44
                        Led {
                            anchors.centerIn: parent
                            on: row.modelData.mine
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: Song.myTrack = row.modelData.mine ? -1 : row.index
                        }
                        Accessible.role: Accessible.RadioButton
                        Accessible.name: "My track: " + row.modelData.name
                        Accessible.checked: row.modelData.mine
                    }
                    Column {
                        width: list.nameWidth
                        anchors.verticalCenter: parent.verticalCenter
                        Text {
                            width: parent.width - 8
                            text: row.modelData.name
                            color: Theme.ink
                            font.family: Theme.labelFont
                            font.pixelSize: 14
                            font.bold: row.modelData.mine
                            elide: Text.ElideRight
                        }
                        Text {
                            visible: row.modelData.tuning !== ""
                            text: row.modelData.strings + " strings: " + row.modelData.tuning
                            color: "#6A6C70"
                            font.family: Theme.labelFont
                            font.pixelSize: 10
                        }
                    }
                    Text {
                        width: 70
                        anchors.verticalCenter: parent.verticalCenter
                        text: row.modelData.role
                        color: Theme.ink
                        font.family: Theme.labelFont
                        font.pixelSize: 12
                    }
                    Text {
                        width: 64
                        anchors.verticalCenter: parent.verticalCenter
                        text: row.modelData.notes
                        color: Theme.ink
                        font.family: Theme.labelFont
                        font.pixelSize: 12
                    }
                    ComboBox {
                        width: 160
                        height: 32
                        anchors.verticalCenter: parent.verticalCenter
                        model: Song.instrumentChoices
                        currentIndex: row.modelData.choice
                        onActivated: index => Song.setInstrument(row.index, index)
                        Accessible.name: row.modelData.name + " instrument"
                    }
                    Item { width: 10; height: 1 }
                    Slider {
                        width: 140
                        anchors.verticalCenter: parent.verticalCenter
                        from: 0
                        to: 1
                        value: row.modelData.level
                        onMoved: Song.setLevel(row.index, value)
                        Accessible.name: row.modelData.name + " level"
                    }
                    Item { width: 10; height: 1 }
                    Row {
                        width: 92
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 6
                        PanelButton {
                            width: 38
                            height: 26
                            faceText: "M"
                            text: row.modelData.name + " mute"
                            dark: row.modelData.mute
                            onClicked: Song.setMute(row.index, !row.modelData.mute)
                        }
                        PanelButton {
                            width: 38
                            height: 26
                            faceText: "S"
                            text: row.modelData.name + " solo"
                            dark: row.modelData.solo
                            onClicked: Song.setSolo(row.index, !row.modelData.solo)
                        }
                    }
                    PanelButton { // which file the track plays from
                        width: 64
                        height: 26
                        anchors.verticalCenter: parent.verticalCenter
                        visible: row.modelData.canMidi
                        faceText: row.modelData.fromMidi ? "MIDI" : "TAB"
                        text: row.modelData.name + " plays from the " + (row.modelData.fromMidi ? "MIDI file" : "tab file")
                        onClicked: Song.setFromMidi(row.index, !row.modelData.fromMidi)
                    }
                }
            }
        }

        // --- My track's tab -----------------------------------------------------------------
        Item {
            id: tabArea
            x: 16
            y: list.y + list.height + (screen.tracksCollapsed ? 0 : 10)
            width: parent.width - 32
            height: parent.height - y - 16
            visible: Song.loaded

            Rectangle {
                anchors.fill: parent
                radius: 4
                color: "#F7F6F2"
                border.color: Theme.deckLine
            }

            // What's in view: the track, and tab or notes (drums: notes only).
            readonly property var viewed: Song.viewTrack >= 0 && Song.viewTrack < Song.tracks.length ? Song.tracks[Song.viewTrack] : null
            property bool notesWanted: false
            readonly property bool notes: viewed !== null && (notesWanted || !viewed.hasTab)

            Row {
                id: viewBar
                x: 10
                y: 6
                height: 24
                spacing: 8
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: tabArea.viewed ? tabArea.viewed.name : ""
                    color: Theme.ink
                    font.family: Theme.labelFont
                    font.pixelSize: 13
                    font.bold: true
                    elide: Text.ElideRight
                    width: Math.min(implicitWidth, tabArea.width - 200)
                }
            }
            Row {
                anchors.right: parent.right
                anchors.rightMargin: 10
                y: 4
                spacing: 4
                PanelButton {
                    width: 56
                    height: 24
                    faceText: "TAB"
                    text: "Show tab"
                    enabled: tabArea.viewed !== null && tabArea.viewed.hasTab
                    opacity: enabled ? 1.0 : 0.4
                    dark: !tabArea.notes
                    onClicked: tabArea.notesWanted = false
                }
                PanelButton {
                    width: 64
                    height: 24
                    faceText: "NOTES"
                    text: "Show notation"
                    dark: tabArea.notes
                    onClicked: tabArea.notesWanted = true
                }
            }

            TabView {
                id: tab
                x: 8
                y: viewBar.y + viewBar.height + 4
                width: parent.width - 16
                height: parent.height - y - 6
                song: Song
                scroll: flick.contentY
                notation: tabArea.notes
            }

            Flickable { // scrolling and clicks; the TabView under it draws
                id: flick
                anchors.fill: tab
                contentWidth: width
                contentHeight: tab.contentHeight
                boundsBehavior: Flickable.StopAtBounds
                clip: true
                ScrollBar.vertical: ScrollBar {}

                MouseArea {
                    width: flick.contentWidth
                    height: flick.contentHeight
                    onClicked: mouse => tab.clickAt(mouse.x, mouse.y)
                    cursorShape: Qt.PointingHandCursor
                }

                NumberAnimation on contentY {
                    id: follow
                    running: false
                    duration: 250
                    easing.type: Easing.InOutQuad
                }
            }

            Rectangle { // the cursor
                x: tab.x + tab.cursorX - 1
                y: tab.y + tab.cursorY + 14 - flick.contentY
                width: 3
                height: Math.max(0, tab.cursorHeight - 14)
                radius: 1
                color: Theme.accentUi
                opacity: 0.85
                visible: tab.cursorVisible && y + height > tab.y && y < tab.y + tab.height
            }

            // Paging: while playing, the cursor's line goes to the top so the next ones are in view.
            Connections {
                target: tab
                function onCursorSystemChanged() {
                    if (tab.cursorSystem < 0 || flick.moving)
                        return
                    const top = tab.systemTop(tab.cursorSystem)
                    const max = Math.max(0, flick.contentHeight - flick.height)
                    const target = Math.min(top, max)
                    const visible = top >= flick.contentY && top + tab.cursorHeight <= flick.contentY + flick.height
                    if (Rack.playing || !visible) {
                        follow.to = target
                        follow.restart()
                    }
                }
            }

            Text {
                anchors.centerIn: parent
                visible: tab.message !== ""
                text: tab.message
                color: "#6A6C70"
                font.family: Theme.labelFont
                font.pixelSize: 16
            }
        }
    }

    // Details of the open song: artist, album, title, track number; or remove it.
    Popup {
        id: editor
        x: (screen.width - width) / 2
        y: 70
        width: 420
        modal: true
        focus: true
        padding: 16
        property bool confirmRemove: false
        onOpened: {
            artistField.text = Song.artist
            albumField.text = Song.album
            titleField.text = Song.title
            trackField.value = Song.trackNumber
            confirmRemove = false
        }
        background: Rectangle {
            radius: 6
            color: Theme.silverLight
            border.color: Theme.deckLine
        }

        Column {
            width: parent.width
            spacing: 8
            Text {
                text: "SONG DETAILS"
                color: Theme.ink
                font.family: Theme.labelFont
                font.pixelSize: 13
                font.bold: true
            }
            Text { text: "Artist"; color: "#55575B"; font.family: Theme.labelFont; font.pixelSize: 11 }
            TextField { id: artistField; width: parent.width; Accessible.name: "Artist" }
            Text { text: "Album"; color: "#55575B"; font.family: Theme.labelFont; font.pixelSize: 11 }
            TextField { id: albumField; width: parent.width; Accessible.name: "Album" }
            Text { text: "Title"; color: "#55575B"; font.family: Theme.labelFont; font.pixelSize: 11 }
            TextField { id: titleField; width: parent.width; Accessible.name: "Title" }
            Text { text: "Track number (0 = none)"; color: "#55575B"; font.family: Theme.labelFont; font.pixelSize: 11 }
            SpinBox { id: trackField; from: 0; to: 99; editable: true; Accessible.name: "Track number" }
            Item { width: 1; height: 6 }
            Row {
                spacing: 10
                PanelButton {
                    width: 84
                    height: 32
                    faceText: "APPLY"
                    text: "Apply the details"
                    onClicked: {
                        Song.setDetails(artistField.text, albumField.text, titleField.text, trackField.value)
                        editor.close()
                    }
                }
                PanelButton {
                    width: 84
                    height: 32
                    faceText: "CANCEL"
                    text: "Cancel"
                    onClicked: editor.close()
                }
                Item { width: 40; height: 1 }
                PanelButton { // two steps: REMOVE, then SURE?
                    width: 120
                    height: 32
                    faceText: editor.confirmRemove ? "SURE? REMOVE" : "REMOVE"
                    text: "Remove the song and its files from the library"
                    tint: editor.confirmRemove ? Theme.ledOn : "transparent"
                    onClicked: {
                        if (!editor.confirmRemove) {
                            editor.confirmRemove = true
                            return
                        }
                        Song.removeFromLibrary(Song.songId)
                        editor.close()
                    }
                }
            }
        }
    }
}

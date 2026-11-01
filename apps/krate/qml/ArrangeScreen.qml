pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Krate
import KrateRack

// The arranger (docs/DAW.md, D3): the sequencer's 8 tracks and the audio
// tracks in the rack as lanes across bars. SONG plays the arrangement instead
// of the patterns. The ruler sets where play starts; the LOOP and PUNCH strips
// under it are dragged to set those ranges. On a MIDI lane, a click places the
// chosen pattern (drag for a longer clip); R arms the lane for recording what
// you play on the keyboard (SONG and REC on). On an audio lane, the takes on it.
//
// Editing clips (D4): a click selects one, dragging it moves it (a MIDI clip
// to another lane too), dragging an edge trims it, an audio clip's top
// corners set its fades; SPLIT cuts where you click, DUPLICATE (Ctrl+D) puts
// a copy right after, Delete or a right click (or ERASE) removes it; a double
// click opens a pattern clip's pattern in the Seq screen. MIDI clips edit in
// whole bars, audio clips in beats (SNAP off: freely). UNDO / REDO (Ctrl+Z,
// Ctrl+Shift+Z) take back or redo any of it.
//
// Automation (D5): + AUTOMATION adds a lane for an instrument's knob, under
// the tracks; a click on it adds a point, a point drags, a right click
// removes it. With AUTO on (and SONG), knobs you turn while playing are
// recorded into their lanes.
//
// EXPORT (D6): the song (or the loop range) rendered to a WAV file, and with
// STEMS each track to its own, in ~/Music/Krate.
Item {
    id: screen

    property int brush: 0          // the pattern a click places
    property bool erasing: false
    property bool splitting: false
    property bool snap: true       // audio clips to beats
    // The selected clip: "midi" or "audio", its lane (a sequencer track or an audio track) and index.
    property string selKind: ""
    property int selTrack: -1
    property int selIndex: -1
    property real barWidth: 48
    readonly property int headerWidth: 168
    readonly property int rulerHeight: 26
    readonly property int stripHeight: 18
    // The lanes fill the screen's height (an audio lane a little taller), within reason.
    readonly property int midiLaneHeight: Math.max(30, Math.min(56, Math.floor(
        (height - 150 - lanesTop - 24) / (8 + 1.4 * audioLanes.length + autoLanes.length))))
    readonly property int audioLaneHeight: Math.round(midiLaneHeight * 1.4)
    readonly property var trackList: Sequencer.revision >= 0 ? Sequencer.tracks : []
    // The audio tracks in the rack: [{track, device}].
    readonly property var audioLanes: {
        const lanes = []
        for (let i = 0; i < 8; ++i) {
            const device = Rack.deviceIds.indexOf("audio" + (i + 1))
            if (device >= 0 && Rack.present[device] === true)
                lanes.push({ track: i, device: device })
        }
        return lanes
    }
    readonly property int bars: Math.min(Arrange.maxBars, Math.max(64, Arrange.endBar + 16,
                                         Math.ceil(Arrange.position / 4) + 16, Arrange.loopTo + 4))
    readonly property real lanesTop: rulerHeight + 2 * stripHeight
    readonly property var autoLanes: Arrange.revision >= 0 ? Arrange.automationLanes() : []
    readonly property real autoTop: lanesTop + 8 * midiLaneHeight + audioLanes.length * audioLaneHeight
    readonly property real lanesHeight: 8 * midiLaneHeight + audioLanes.length * audioLaneHeight
                                        + autoLanes.length * midiLaneHeight

    function barAt(x) { return Math.max(0, Math.min(Arrange.maxBars - 1, Math.floor(x / screen.barWidth))) }
    function select(kind, track, index) {
        selKind = kind
        selTrack = track
        selIndex = index
    }
    function isSelected(kind, track, index) { return selKind === kind && selTrack === track && selIndex === index }
    // Beats on the timeline from a pixel offset, snapped to beats when SNAP is on.
    function beatsOf(dx) {
        const beats = dx / screen.barWidth * 4
        return snap ? Math.round(beats) : beats
    }
    function deleteSelected() {
        if (selKind === "midi")
            Arrange.removeClip(selTrack, selIndex)
        else if (selKind === "audio")
            Tracks.removeClip(selTrack, selIndex)
        select("", -1, -1)
    }
    function duplicateSelected() {
        if (selKind === "midi") {
            if (Arrange.duplicateClip(selTrack, selIndex))
                select("midi", selTrack, selIndex + 1)
        } else if (selKind === "audio") {
            const index = Tracks.duplicateClip(selTrack, selIndex, Rack.bpm)
            if (index >= 0)
                select("audio", selTrack, index)
        }
    }

    // Undo, redo and the selection's keys, while this screen shows.
    Shortcut { sequences: [StandardKey.Undo]; enabled: screen.visible; onActivated: { Arrange.undo(); screen.select("", -1, -1) } }
    Shortcut { sequences: [StandardKey.Redo, "Ctrl+Shift+Z"]; enabled: screen.visible; onActivated: { Arrange.redo(); screen.select("", -1, -1) } }
    Shortcut { sequence: "Ctrl+D"; enabled: screen.visible; onActivated: screen.duplicateSelected() }
    Shortcut { sequences: [StandardKey.Delete, "Backspace"]; enabled: screen.visible; onActivated: screen.deleteSelected() }
    function patternColor(pattern) { return Qt.hsla((pattern * 0.13) % 1.0, 0.45, 0.62, 1.0) }

    Rectangle { // the deck
        anchors.fill: parent
        radius: 6
        color: Theme.lowerDeck
        border.color: Theme.deckLine
    }

    // --- Top: SONG, LOOP, PUNCH, the pattern to place, zoom, where it is ----------------
    Row {
        id: header
        x: 16
        y: 12
        height: 56
        spacing: 10

        PanelButton {
            width: 92
            height: 40
            anchors.verticalCenter: parent.verticalCenter
            faceText: "    SONG"
            text: "Song mode: play the arrangement instead of the patterns"
            onClicked: Arrange.songMode = !Arrange.songMode
            Led { x: 12; anchors.verticalCenter: parent.verticalCenter; on: Arrange.songMode }
        }
        PanelButton {
            width: 92
            height: 40
            anchors.verticalCenter: parent.verticalCenter
            faceText: "    LOOP"
            text: "Loop: play the loop range round and round (SONG mode)"
            onClicked: Arrange.loopOn = !Arrange.loopOn
            Led { x: 12; anchors.verticalCenter: parent.verticalCenter; on: Arrange.loopOn }
        }
        PanelButton {
            width: 100
            height: 40
            anchors.verticalCenter: parent.verticalCenter
            faceText: "    PUNCH"
            text: "Punch: armed audio tracks record only inside the punch range (SONG mode)"
            onClicked: Arrange.punchOn = !Arrange.punchOn
            Led { x: 12; anchors.verticalCenter: parent.verticalCenter; on: Arrange.punchOn }
        }
        PanelButton {
            width: 92
            height: 40
            anchors.verticalCenter: parent.verticalCenter
            faceText: "    AUTO"
            text: "Automation: knobs you turn while playing are recorded (SONG mode)"
            tint: Arrange.writing ? Theme.ledOn : "transparent"
            onClicked: Arrange.autoWrite = !Arrange.autoWrite
            Led { x: 12; anchors.verticalCenter: parent.verticalCenter; on: Arrange.autoWrite }
        }

        Column {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2
            Text {
                text: "PLACE PATTERN"
                color: Theme.ink
                font.family: Theme.labelFont
                font.pixelSize: 11
                font.bold: true
            }
            Row {
                spacing: 3
                Repeater {
                    model: 16
                    Rectangle {
                        id: brushButton
                        required property int index
                        readonly property bool chosen: !screen.erasing && screen.brush === index
                        width: 30
                        height: 30
                        radius: 3
                        color: chosen ? screen.patternColor(index) : Qt.lighter(screen.patternColor(index), 1.25)
                        border.color: chosen ? Theme.accentUi : Theme.deckLine
                        border.width: chosen ? 3 : 1
                        Text {
                            anchors.centerIn: parent
                            text: brushButton.index + 1
                            color: Theme.ink
                            font.family: Theme.labelFont
                            font.pixelSize: 13
                            font.bold: brushButton.chosen
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                screen.brush = brushButton.index
                                screen.erasing = false
                            }
                        }
                        Accessible.role: Accessible.Button
                        Accessible.name: "Place pattern " + (brushButton.index + 1)
                    }
                }
                PanelButton {
                    width: 70
                    height: 30
                    faceText: "ERASE"
                    text: "Erase: a click removes a clip"
                    tint: screen.erasing ? Theme.labelOrange : "transparent"
                    onClicked: {
                        screen.erasing = !screen.erasing
                        screen.splitting = false
                    }
                }
            }
        }

        Column {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2
            Text {
                text: "ZOOM"
                color: Theme.ink
                font.family: Theme.labelFont
                font.pixelSize: 11
                font.bold: true
            }
            Row {
                spacing: 3
                PanelButton {
                    width: 30
                    height: 30
                    faceText: "−"
                    text: "Zoom out"
                    onClicked: screen.barWidth = Math.max(16, screen.barWidth / 1.5)
                }
                PanelButton {
                    width: 30
                    height: 30
                    faceText: "+"
                    text: "Zoom in"
                    onClicked: screen.barWidth = Math.min(200, screen.barWidth * 1.5)
                }
            }
        }

        Column {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2
            Text {
                text: Arrange.playing ? "PLAYING" : "PLAY STARTS AT"
                color: Theme.ink
                font.family: Theme.labelFont
                font.pixelSize: 11
                font.bold: true
            }
            Text {
                readonly property int beat: Math.floor(Arrange.position)
                text: "Bar " + (Math.floor(beat / 4) + 1) + " · " + (beat % 4 + 1)
                color: Theme.ink
                font.family: "monospace"
                font.pixelSize: 20
            }
        }
    }

    // --- Editing: split, duplicate, snap, undo, redo --------------------------------------
    Row {
        id: tools
        x: 16
        y: header.y + header.height + 2
        spacing: 8
        PanelButton {
            width: 76
            height: 30
            faceText: "SPLIT"
            text: "Split: a click on a clip cuts it there"
            tint: screen.splitting ? Theme.labelOrange : "transparent"
            onClicked: {
                screen.splitting = !screen.splitting
                screen.erasing = false
            }
        }
        PanelButton {
            width: 104
            height: 30
            faceText: "DUPLICATE"
            text: "Duplicate the selected clip right after itself (Ctrl+D)"
            enabled: screen.selKind !== ""
            opacity: enabled ? 1 : 0.5
            onClicked: screen.duplicateSelected()
        }
        PanelButton {
            width: 76
            height: 30
            faceText: "SNAP"
            text: "Snap audio clips to beats"
            tint: screen.snap ? Theme.labelOrange : "transparent"
            onClicked: screen.snap = !screen.snap
        }
        PanelButton {
            width: 90
            height: 30
            faceText: "EXPORT"
            text: "Export the song to a WAV file (and each track, with stems)"
            onClicked: exportPopup.open()
        }
        PanelButton {
            width: 130
            height: 30
            faceText: "+ AUTOMATION"
            text: "Add an automation lane for an instrument's knob"
            onClicked: automationPopup.open()
        }
        Item { width: 12; height: 1 }
        PanelButton {
            width: 76
            height: 30
            faceText: "UNDO"
            text: "Undo the last edit (Ctrl+Z)"
            enabled: Arrange.canUndo
            opacity: enabled ? 1 : 0.5
            onClicked: {
                Arrange.undo()
                screen.select("", -1, -1)
            }
        }
        PanelButton {
            width: 76
            height: 30
            faceText: "REDO"
            text: "Redo what was undone (Ctrl+Shift+Z)"
            enabled: Arrange.canRedo
            opacity: enabled ? 1 : 0.5
            onClicked: {
                Arrange.redo()
                screen.select("", -1, -1)
            }
        }
    }

    Text { // what this screen does in pattern mode, or while a DAW leads
        x: tools.x + tools.width + 16
        anchors.verticalCenter: tools.verticalCenter
        width: parent.width - x - 16
        elide: Text.ElideRight
        text: Arrange.following ? "A DAW leads over JACK: its position and its loop play (start, LOOP and PUNCH here wait)."
              : Arrange.songMode && Arrange.recording && Arrange.playing ? "Recording: " + Arrange.takeNotes + " notes so far (armed lanes)."
              : Arrange.songMode ? "SONG: the arrangement plays. Ruler: where play starts. Drag LOOP and PUNCH to set them. R + REC records the keyboard."
              : "Patterns play as the Seq screen says. Switch SONG on to play this arrangement."
        color: Theme.ink
        opacity: 0.75
        font.family: Theme.labelFont
        font.pixelSize: 13
    }

    // --- The lanes' names ---------------------------------------------------------------
    Item {
        id: names
        x: 16
        y: tools.y + tools.height + 8
        width: screen.headerWidth
        height: screen.lanesTop + screen.lanesHeight

        Text {
            x: 6
            y: 4
            text: "BAR"
            color: Theme.ink
            font.family: Theme.labelFont
            font.pixelSize: 11
            font.bold: true
        }
        Text {
            x: 6
            y: screen.rulerHeight + 2
            text: "LOOP"
            color: Theme.ink
            font.family: Theme.labelFont
            font.pixelSize: 10
            font.bold: true
        }
        Text {
            x: 6
            y: screen.rulerHeight + screen.stripHeight + 2
            text: "PUNCH"
            color: Theme.ink
            font.family: Theme.labelFont
            font.pixelSize: 10
            font.bold: true
        }

        Repeater {
            model: 8
            Rectangle {
                id: midiName
                required property int index
                readonly property var info: screen.trackList[index] ?? ({})
                y: screen.lanesTop + index * screen.midiLaneHeight
                width: screen.headerWidth - 4
                height: screen.midiLaneHeight - 2
                radius: 3
                color: Theme.silverLight
                border.color: Theme.deckLine
                readonly property bool armed: Arrange.revision >= 0 && Arrange.armed(index)
                Text {
                    x: 8
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - 84
                    elide: Text.ElideRight
                    text: (midiName.index + 1) + "  " + (midiName.info.label || "—")
                    color: Theme.ink
                    opacity: midiName.info.mute ? 0.45 : 1.0
                    font.family: Theme.labelFont
                    font.pixelSize: 13
                    font.bold: true
                }
                PanelButton {
                    anchors.right: parent.right
                    anchors.rightMargin: 4
                    anchors.verticalCenter: parent.verticalCenter
                    width: 32
                    height: 24
                    faceText: "M"
                    text: "Mute track " + (midiName.index + 1)
                    tint: midiName.info.mute ? Theme.labelOrange : "transparent"
                    onClicked: Sequencer.setMute(midiName.index, !midiName.info.mute)
                }
                PanelButton {
                    anchors.right: parent.right
                    anchors.rightMargin: 40
                    anchors.verticalCenter: parent.verticalCenter
                    width: 32
                    height: 24
                    faceText: "R"
                    text: "Arm track " + (midiName.index + 1) + " to record what you play into a MIDI clip"
                    tint: midiName.armed ? Theme.ledOn : "transparent"
                    onClicked: {
                        Arrange.setArmed(midiName.index, !midiName.armed)
                        // The keyboard plays the lane's instrument, so you hear what's recorded.
                        const device = Sequencer.targetDevices[midiName.info.choice ?? 0] ?? -1
                        if (midiName.armed && device >= 0) {
                            Rack.focus = device
                            Rack.arrangeShown = true
                        }
                    }
                }
            }
        }

        Repeater {
            model: screen.audioLanes
            Rectangle {
                id: audioName
                required property int index
                required property var modelData
                readonly property bool armed: Tracks.frame >= 0 && Tracks.param(modelData.track, 0) > 0.5
                y: screen.lanesTop + 8 * screen.midiLaneHeight + index * screen.audioLaneHeight
                width: screen.headerWidth - 4
                height: screen.audioLaneHeight - 2
                radius: 3
                color: "#D9D3C7"
                border.color: Theme.deckLine
                Text {
                    x: 8
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Audio " + (audioName.modelData.track + 1)
                    color: Theme.ink
                    font.family: Theme.labelFont
                    font.pixelSize: 13
                    font.bold: true
                }
                Led {
                    anchors.right: armButton.left
                    anchors.rightMargin: 6
                    anchors.verticalCenter: parent.verticalCenter
                    visible: Tracks.frame >= 0 && Tracks.isRecording(audioName.modelData.track)
                    on: true
                }
                PanelButton {
                    id: armButton
                    anchors.right: parent.right
                    anchors.rightMargin: 4
                    anchors.verticalCenter: parent.verticalCenter
                    width: 48
                    height: 26
                    faceText: "ARM"
                    text: "Arm Audio " + (audioName.modelData.track + 1) + " for recording"
                    tint: audioName.armed ? Theme.ledOn : "transparent"
                    onClicked: Tracks.setParam(audioName.modelData.track, 0, audioName.armed ? 0 : 1)
                }
            }
        }

        Repeater {
            model: screen.autoLanes
            Rectangle {
                id: autoName
                required property int index
                required property var modelData
                y: screen.autoTop + index * screen.midiLaneHeight
                width: screen.headerWidth - 4
                height: screen.midiLaneHeight - 2
                radius: 3
                color: "#CFD8CF"
                border.color: Theme.deckLine
                Text {
                    x: 8
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - 44
                    elide: Text.ElideRight
                    text: autoName.modelData.deviceName + " · " + autoName.modelData.label
                    color: Theme.ink
                    font.family: Theme.labelFont
                    font.pixelSize: 12
                    font.bold: true
                }
                PanelButton {
                    anchors.right: parent.right
                    anchors.rightMargin: 4
                    anchors.verticalCenter: parent.verticalCenter
                    width: 30
                    height: 24
                    faceText: "✕"
                    text: "Remove the automation lane " + autoName.modelData.deviceName + " " + autoName.modelData.label
                    onClicked: Arrange.removeAutomationLane(autoName.index)
                }
            }
        }
    }

    // --- The timeline ------------------------------------------------------------------
    Flickable {
        id: timeline
        x: names.x + screen.headerWidth
        y: names.y
        width: parent.width - x - 16
        height: names.height + 16 // room for the scroll bar under the lanes
        contentWidth: screen.bars * screen.barWidth
        contentHeight: height
        flickableDirection: Flickable.HorizontalFlick
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AlwaysOn }

        // While playing, the page turns to keep the playhead in view.
        Connections {
            target: Arrange
            function onPositionChanged() {
                if (!Arrange.playing)
                    return
                const x = Arrange.position / 4 * screen.barWidth
                if (x < timeline.contentX || x > timeline.contentX + timeline.width - 40)
                    timeline.contentX = Math.max(0, Math.min(timeline.contentWidth - timeline.width, x - 40))
            }
        }

        Item {
            id: content
            width: timeline.contentWidth
            height: timeline.height

            // The ruler: bar numbers; a click sets where play starts.
            Rectangle {
                width: parent.width
                height: screen.rulerHeight
                color: Theme.silverDark
                Repeater {
                    model: screen.bars
                    Item {
                        id: tickMark
                        required property int index
                        readonly property int every: screen.barWidth >= 40 ? 1 : screen.barWidth >= 20 ? 2 : 4
                        x: index * screen.barWidth
                        Rectangle { width: 1; height: screen.rulerHeight; color: Theme.deckLine }
                        Text {
                            x: 4
                            y: 4
                            visible: tickMark.index % tickMark.every === 0
                            text: tickMark.index + 1
                            color: Theme.ink
                            font.family: "monospace"
                            font.pixelSize: 12
                        }
                    }
                }
                Text { // the start marker
                    x: Arrange.startBar * screen.barWidth - 6
                    y: 8
                    visible: Arrange.songMode
                    text: "▼"
                    color: Theme.accentUi
                    font.pixelSize: 14
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: mouse => Arrange.startBar = screen.barAt(mouse.x)
                }
            }

            // LOOP and PUNCH: drag across bars to set the range.
            Repeater {
                model: [
                    { kind: "loop", y: screen.rulerHeight, color: Theme.accentUi },
                    { kind: "punch", y: screen.rulerHeight + screen.stripHeight, color: Theme.ledOn }
                ]
                Rectangle {
                    id: strip
                    required property var modelData
                    readonly property bool isLoop: modelData.kind === "loop"
                    readonly property bool on: isLoop ? Arrange.loopOn : Arrange.punchOn
                    readonly property int from: isLoop ? Arrange.loopFrom : Arrange.punchFrom
                    readonly property int to: isLoop ? Arrange.loopTo : Arrange.punchTo
                    property int dragFrom: -1
                    property int dragTo: -1
                    y: modelData.y
                    width: content.width
                    height: screen.stripHeight
                    color: Theme.lowerDeck
                    border.color: Theme.deckLine
                    Rectangle {
                        x: (strip.dragFrom >= 0 ? Math.min(strip.dragFrom, strip.dragTo) : strip.from) * screen.barWidth
                        width: (strip.dragFrom >= 0 ? Math.abs(strip.dragTo - strip.dragFrom) + 1 : strip.to - strip.from) * screen.barWidth
                        y: 3
                        height: parent.height - 6
                        radius: 3
                        color: strip.modelData.color
                        opacity: strip.on || strip.dragFrom >= 0 ? 0.85 : 0.3
                    }
                    MouseArea {
                        anchors.fill: parent
                        preventStealing: true
                        onPressed: mouse => {
                            strip.dragFrom = screen.barAt(mouse.x)
                            strip.dragTo = strip.dragFrom
                        }
                        onPositionChanged: mouse => strip.dragTo = screen.barAt(mouse.x)
                        onReleased: {
                            const a = Math.min(strip.dragFrom, strip.dragTo), b = Math.max(strip.dragFrom, strip.dragTo) + 1
                            if (strip.isLoop) {
                                Arrange.setLoop(a, b)
                                Arrange.loopOn = true
                            } else {
                                Arrange.setPunch(a, b)
                                Arrange.punchOn = true
                            }
                            strip.dragFrom = -1
                        }
                    }
                }
            }

            // The MIDI lanes.
            Repeater {
                model: 8
                Rectangle {
                    id: midiLane
                    required property int index
                    readonly property var clips: Arrange.revision >= 0 ? Arrange.lane(index) : []
                    property int pressBar: -1
                    property int dragBar: -1
                    property bool dragging: false // a clip of it: drawn over the other lanes
                    z: dragging ? 5 : 0
                    y: screen.lanesTop + index * screen.midiLaneHeight
                    width: content.width
                    height: screen.midiLaneHeight - 2
                    color: index % 2 ? "#C4C5C8" : "#C9CACD"

                    Repeater { // bar lines
                        model: screen.bars
                        Rectangle {
                            required property int index
                            x: index * screen.barWidth
                            width: 1
                            height: midiLane.height
                            color: index % 4 === 0 ? Theme.deckLine : "#B4B5B8"
                        }
                    }
                    Rectangle { // a clip being dragged out
                        visible: midiLane.pressBar >= 0 && midiLane.dragBar > midiLane.pressBar
                        x: midiLane.pressBar * screen.barWidth
                        width: (midiLane.dragBar - midiLane.pressBar + 1) * screen.barWidth
                        height: midiLane.height
                        radius: 3
                        color: screen.patternColor(screen.brush)
                        opacity: 0.5
                    }
                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        preventStealing: true
                        onPressed: mouse => {
                            const bar = screen.barAt(mouse.x)
                            screen.select("", -1, -1)
                            if (Arrange.clipAt(midiLane.index, bar) >= 0)
                                return // (the clips take their own clicks)
                            if (mouse.button === Qt.LeftButton && !screen.erasing && !screen.splitting) {
                                midiLane.pressBar = bar
                                midiLane.dragBar = bar
                            }
                        }
                        onPositionChanged: mouse => {
                            if (midiLane.pressBar >= 0)
                                midiLane.dragBar = Math.max(midiLane.pressBar, screen.barAt(mouse.x))
                        }
                        onReleased: {
                            if (midiLane.pressBar < 0)
                                return
                            const bars = midiLane.dragBar > midiLane.pressBar ? midiLane.dragBar - midiLane.pressBar + 1 : 0
                            Arrange.placePattern(midiLane.index, midiLane.pressBar, screen.brush, bars)
                            midiLane.pressBar = -1
                        }
                    }

                    Repeater { // the clips
                        model: midiLane.clips
                        Rectangle {
                            id: clipBox
                            required property int index
                            required property var modelData
                            readonly property bool isPattern: modelData.pattern >= 0
                            readonly property bool selected: screen.isSelected("midi", midiLane.index, index)
                            // A drag: "move", "start" or "end", and how far (whole bars, lanes).
                            property string dragKind: ""
                            property int dBars: 0
                            property int dLanes: 0
                            readonly property int shownStart: modelData.startBar + (dragKind === "end" ? 0 : dBars)
                            readonly property int shownEnd: modelData.startBar + modelData.bars + (dragKind === "start" ? 0 : dBars)
                            x: shownStart * screen.barWidth + 1
                            y: dLanes * screen.midiLaneHeight
                            width: Math.max(screen.barWidth, (shownEnd - shownStart) * screen.barWidth) - 2
                            height: midiLane.height
                            radius: 3
                            color: isPattern ? screen.patternColor(modelData.pattern) : "#9FB4C8"
                            border.color: selected ? Theme.accentUi : Qt.darker(color, 1.4)
                            border.width: selected ? 3 : 1
                            opacity: dragKind !== "" ? 0.8 : 1.0
                            clip: true
                            Canvas { // its notes
                                anchors.fill: parent
                                anchors.margins: 3
                                readonly property var notes: Arrange.revision >= 0 ? Arrange.clipNotes(midiLane.index, clipBox.index) : []
                                readonly property real beatWidth: screen.barWidth / 4
                                onNotesChanged: requestPaint()
                                onBeatWidthChanged: requestPaint()
                                onWidthChanged: requestPaint()
                                onPaint: {
                                    const ctx = getContext("2d")
                                    ctx.clearRect(0, 0, width, height)
                                    if (notes.length === 0)
                                        return
                                    let low = 127, high = 0
                                    for (const n of notes) {
                                        low = Math.min(low, n.pitch)
                                        high = Math.max(high, n.pitch)
                                    }
                                    const span = Math.max(high - low, 6)
                                    ctx.fillStyle = "rgba(20, 20, 20, 0.55)"
                                    for (const n of notes) {
                                        const y = (1 - (n.pitch - low) / span) * (height - 3)
                                        ctx.fillRect(n.x * beatWidth, y, Math.max(1.5, n.w * beatWidth - 1), 3)
                                    }
                                }
                            }
                            Text {
                                x: 4
                                y: 1
                                visible: clipBox.width > 24
                                text: clipBox.isPattern ? "P" + (clipBox.modelData.pattern + 1) : "MIDI"
                                color: Theme.ink
                                font.family: Theme.labelFont
                                font.pixelSize: 10
                                font.bold: true
                            }
                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                preventStealing: true
                                hoverEnabled: true
                                property real pressX: 0
                                property real pressY: 0
                                readonly property int edge: 8
                                cursorShape: screen.splitting ? Qt.SplitHCursor
                                             : mouseX < edge || mouseX > width - edge ? Qt.SizeHorCursor : Qt.OpenHandCursor
                                onPressed: mouse => {
                                    if (mouse.button === Qt.RightButton || screen.erasing) {
                                        Arrange.removeClip(midiLane.index, clipBox.index)
                                        screen.select("", -1, -1)
                                        return
                                    }
                                    if (screen.splitting) {
                                        Arrange.splitClip(midiLane.index, clipBox.index,
                                                          clipBox.modelData.startBar + Math.round(mouse.x / screen.barWidth))
                                        return
                                    }
                                    screen.select("midi", midiLane.index, clipBox.index)
                                    pressX = mouse.x
                                    pressY = mouse.y
                                    clipBox.dragKind = mouse.x < edge ? "start" : mouse.x > width - edge ? "end" : "move"
                                    midiLane.dragging = true
                                }
                                onPositionChanged: mouse => {
                                    if (clipBox.dragKind === "")
                                        return
                                    clipBox.dBars = Math.round((mouse.x - pressX) / screen.barWidth)
                                    if (clipBox.dragKind === "move")
                                        clipBox.dLanes = Math.max(-midiLane.index, Math.min(7 - midiLane.index,
                                                                  Math.round((mouse.y - pressY) / screen.midiLaneHeight)))
                                }
                                onReleased: {
                                    const kind = clipBox.dragKind, bars = clipBox.dBars, lanes = clipBox.dLanes
                                    const start = clipBox.modelData.startBar, end = start + clipBox.modelData.bars
                                    clipBox.dragKind = ""
                                    clipBox.dBars = 0
                                    clipBox.dLanes = 0
                                    midiLane.dragging = false
                                    if (kind === "move" && (bars !== 0 || lanes !== 0)) {
                                        if (Arrange.moveClip(midiLane.index, clipBox.index, midiLane.index + lanes, start + bars))
                                            screen.select("", -1, -1) // (it has a new place in its lane)
                                    } else if (kind === "start" && bars !== 0) {
                                        Arrange.trimClipStart(midiLane.index, clipBox.index, start + bars)
                                    } else if (kind === "end" && bars !== 0) {
                                        Arrange.trimClipEnd(midiLane.index, clipBox.index, end + bars)
                                    }
                                }
                                onDoubleClicked: {
                                    if (!clipBox.isPattern)
                                        return
                                    Sequencer.pattern = clipBox.modelData.pattern // its notes, in the Seq screen
                                    Rack.seqShown = true
                                }
                            }
                        }
                    }
                }
            }

            // The audio lanes.
            Repeater {
                model: screen.audioLanes
                Rectangle {
                    id: audioLane
                    required property int index
                    required property var modelData
                    readonly property var clips: Tracks.revision >= 0 ? Tracks.clips(modelData.track) : []
                    y: screen.lanesTop + 8 * screen.midiLaneHeight + index * screen.audioLaneHeight
                    width: content.width
                    height: screen.audioLaneHeight - 2
                    color: index % 2 ? "#CFC9BE" : "#D4CEC3"

                    Repeater { // bar lines
                        model: screen.bars
                        Rectangle {
                            required property int index
                            x: index * screen.barWidth
                            width: 1
                            height: audioLane.height
                            color: index % 4 === 0 ? Theme.deckLine : "#BDB7AC"
                        }
                    }
                    MouseArea { // a click beside the clips: nothing selected
                        anchors.fill: parent
                        onPressed: screen.select("", -1, -1)
                    }
                    Repeater { // the takes on the lane, a later one on top
                        model: audioLane.clips
                        Rectangle {
                            id: takeBox
                            required property int index
                            required property var modelData
                            readonly property int track: audioLane.modelData.track
                            readonly property real beatsLong: modelData.seconds * Rack.bpm / 60
                            readonly property bool selected: screen.isSelected("audio", track, index)
                            // A drag: "move", "start", "end", "fadeIn" or "fadeOut", and how far (beats).
                            property string dragKind: ""
                            property real dBeats: 0
                            readonly property real shownStart: modelData.startBeat + (dragKind === "move" || dragKind === "start" ? dBeats : 0)
                            readonly property real shownEnd: modelData.startBeat + beatsLong
                                                              + (dragKind === "move" || dragKind === "end" ? dBeats : 0)
                            readonly property real fadeInBeats: Math.max(0, modelData.fadeIn * Rack.bpm / 60 + (dragKind === "fadeIn" ? dBeats : 0))
                            readonly property real fadeOutBeats: Math.max(0, modelData.fadeOut * Rack.bpm / 60 - (dragKind === "fadeOut" ? dBeats : 0))
                            readonly property real beatWidth: screen.barWidth / 4
                            x: shownStart * beatWidth
                            width: Math.max(4, (shownEnd - shownStart) * beatWidth)
                            height: audioLane.height
                            radius: 3
                            color: "#5E6F7E"
                            border.color: selected ? Theme.accentUi : "#33404C"
                            border.width: selected ? 3 : 1
                            opacity: dragKind !== "" ? 0.85 : 1.0
                            clip: true
                            Canvas { // the part of the take it plays, and its fades
                                anchors.fill: parent
                                anchors.margins: 2
                                readonly property var peaks: Tracks.revision >= 0 && takeBox.modelData.loaded
                                                             ? Tracks.takePeaks(takeBox.track, takeBox.modelData.take) : []
                                readonly property real from: takeBox.modelData.from
                                readonly property real to: takeBox.modelData.to
                                readonly property real fadeIn: takeBox.fadeInBeats * takeBox.beatWidth
                                readonly property real fadeOut: takeBox.fadeOutBeats * takeBox.beatWidth
                                onPeaksChanged: requestPaint()
                                onWidthChanged: requestPaint()
                                onFromChanged: requestPaint()
                                onToChanged: requestPaint()
                                onFadeInChanged: requestPaint()
                                onFadeOutChanged: requestPaint()
                                onPaint: {
                                    const ctx = getContext("2d")
                                    ctx.clearRect(0, 0, width, height)
                                    const n = peaks.length
                                    if (n > 0) {
                                        ctx.fillStyle = Theme.labelOrange
                                        const mid = height / 2
                                        for (let x = 0; x < width; ++x) {
                                            const at = from + (to - from) * x / width
                                            const p = peaks[Math.min(n - 1, Math.floor(at * n))]
                                            const h = Math.max(0.5, p * mid)
                                            ctx.fillRect(x, mid - h, 1, 2 * h)
                                        }
                                    }
                                    ctx.fillStyle = "rgba(20, 24, 28, 0.55)" // the fades: what they take away
                                    if (fadeIn > 0) {
                                        ctx.beginPath()
                                        ctx.moveTo(0, 0)
                                        ctx.lineTo(fadeIn, 0)
                                        ctx.lineTo(0, height)
                                        ctx.closePath()
                                        ctx.fill()
                                    }
                                    if (fadeOut > 0) {
                                        ctx.beginPath()
                                        ctx.moveTo(width, 0)
                                        ctx.lineTo(width - fadeOut, 0)
                                        ctx.lineTo(width, height)
                                        ctx.closePath()
                                        ctx.fill()
                                    }
                                }
                            }
                            Rectangle { // the fade handles (top corners), on the selected clip
                                visible: takeBox.selected
                                x: Math.min(takeBox.fadeInBeats * takeBox.beatWidth, takeBox.width - 10)
                                width: 10
                                height: 10
                                color: Theme.inkInverse
                                border.color: "#33404C"
                            }
                            Rectangle {
                                visible: takeBox.selected
                                x: Math.max(0, takeBox.width - 10 - takeBox.fadeOutBeats * takeBox.beatWidth)
                                width: 10
                                height: 10
                                color: Theme.inkInverse
                                border.color: "#33404C"
                            }
                            Text {
                                x: 14
                                y: 1
                                visible: takeBox.width > 80
                                text: takeBox.modelData.name.replace(/^take-/, "")
                                color: Theme.inkInverse
                                font.family: "monospace"
                                font.pixelSize: 10
                            }
                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                preventStealing: true
                                hoverEnabled: true
                                property real pressX: 0
                                readonly property int edge: 8
                                readonly property int corner: 12 // the top corners: fades
                                function kindAt(x, y) {
                                    if (y < corner && x < corner + takeBox.fadeInBeats * takeBox.beatWidth && x < width / 2)
                                        return "fadeIn"
                                    if (y < corner && x > width - corner - takeBox.fadeOutBeats * takeBox.beatWidth && x >= width / 2)
                                        return "fadeOut"
                                    return x < edge ? "start" : x > width - edge ? "end" : "move"
                                }
                                cursorShape: screen.splitting ? Qt.SplitHCursor
                                             : kindAt(mouseX, mouseY) === "move" ? Qt.OpenHandCursor : Qt.SizeHorCursor
                                onPressed: mouse => {
                                    if (mouse.button === Qt.RightButton || screen.erasing) {
                                        Tracks.removeClip(takeBox.track, takeBox.index)
                                        screen.select("", -1, -1)
                                        return
                                    }
                                    if (screen.splitting) {
                                        Tracks.splitClip(takeBox.track, takeBox.index,
                                                         takeBox.modelData.startBeat + screen.beatsOf(mouse.x), Rack.bpm)
                                        return
                                    }
                                    screen.select("audio", takeBox.track, takeBox.index)
                                    Tracks.selectTake(takeBox.track, takeBox.modelData.take)
                                    pressX = mouse.x
                                    takeBox.dragKind = kindAt(mouse.x, mouse.y)
                                }
                                onPositionChanged: mouse => {
                                    if (takeBox.dragKind === "")
                                        return
                                    const fade = takeBox.dragKind === "fadeIn" || takeBox.dragKind === "fadeOut"
                                    takeBox.dBeats = fade ? (mouse.x - pressX) / takeBox.beatWidth : screen.beatsOf(mouse.x - pressX)
                                }
                                onReleased: {
                                    const kind = takeBox.dragKind, d = takeBox.dBeats
                                    const fadeIn = takeBox.fadeInBeats * 60 / Rack.bpm, fadeOut = takeBox.fadeOutBeats * 60 / Rack.bpm
                                    const start = takeBox.modelData.startBeat
                                    takeBox.dragKind = ""
                                    takeBox.dBeats = 0
                                    if (d === 0)
                                        return
                                    if (kind === "move") {
                                        screen.select("audio", takeBox.track, Tracks.moveClip(takeBox.track, takeBox.index, start + d))
                                    } else if (kind === "start") {
                                        Tracks.trimClipStart(takeBox.track, takeBox.index, start + d, Rack.bpm)
                                    } else if (kind === "end") {
                                        Tracks.trimClipEnd(takeBox.track, takeBox.index, start + takeBox.beatsLong + d, Rack.bpm)
                                    } else {
                                        Tracks.setClipFades(takeBox.track, takeBox.index, fadeIn, fadeOut)
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // The automation lanes: the line, its points.
            Repeater {
                model: screen.autoLanes
                Rectangle {
                    id: autoLane
                    required property int index
                    required property var modelData
                    readonly property var points: modelData.points
                    readonly property real beatWidth: screen.barWidth / 4
                    property int dragging: -1 // the point being dragged
                    property real dragBeat: 0
                    property real dragValue: 0
                    y: screen.autoTop + index * screen.midiLaneHeight
                    width: content.width
                    height: screen.midiLaneHeight - 2
                    color: index % 2 ? "#C6CEC6" : "#CBD3CB"

                    function xOf(beat) { return beat * beatWidth }
                    function yOf(value) { return 3 + (1 - value) * (height - 6) }
                    function valueAt(y) { return Math.max(0, Math.min(1, 1 - (y - 3) / (height - 6))) }
                    // Quarter beats when SNAP is on.
                    function beatAt(x) {
                        const beat = Math.max(0, x / beatWidth)
                        return screen.snap ? Math.round(beat * 4) / 4 : beat
                    }
                    function pointNear(x, y) {
                        for (let i = 0; i < points.length; ++i)
                            if (Math.abs(xOf(points[i].beat) - x) <= 6 && Math.abs(yOf(points[i].value) - y) <= 8)
                                return i
                        return -1
                    }

                    Canvas {
                        id: curve
                        anchors.fill: parent
                        onPaint: {
                            const ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            const pts = autoLane.points.map((p, i) => i === autoLane.dragging
                                ? { beat: autoLane.dragBeat, value: autoLane.dragValue } : p)
                            pts.sort((a, b) => a.beat - b.beat)
                            if (pts.length === 0)
                                return
                            ctx.strokeStyle = "#2F6B3A"
                            ctx.lineWidth = 2
                            ctx.beginPath()
                            ctx.moveTo(0, autoLane.yOf(pts[0].value))
                            for (const p of pts)
                                ctx.lineTo(autoLane.xOf(p.beat), autoLane.yOf(p.value))
                            ctx.lineTo(width, autoLane.yOf(pts[pts.length - 1].value))
                            ctx.stroke()
                            ctx.fillStyle = "#2F6B3A"
                            for (const p of pts)
                                ctx.fillRect(autoLane.xOf(p.beat) - 3, autoLane.yOf(p.value) - 3, 6, 6)
                        }
                    }
                    onPointsChanged: curve.requestPaint()
                    onBeatWidthChanged: curve.requestPaint()
                    onHeightChanged: curve.requestPaint()
                    onWidthChanged: curve.requestPaint()

                    Text { // a dragged point's value, as the knob shows it
                        visible: autoLane.dragging >= 0
                        x: autoLane.xOf(autoLane.dragBeat) + 8
                        y: 2
                        text: autoLane.dragging >= 0 ? Arrange.valueText(autoLane.index, autoLane.dragValue) : ""
                        color: Theme.ink
                        font.family: "monospace"
                        font.pixelSize: 11
                        font.bold: true
                    }
                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        preventStealing: true
                        onPressed: mouse => {
                            screen.select("", -1, -1)
                            const near = autoLane.pointNear(mouse.x, mouse.y)
                            if (mouse.button === Qt.RightButton || screen.erasing) {
                                if (near >= 0)
                                    Arrange.removePoint(autoLane.index, near)
                                return
                            }
                            if (near < 0) {
                                Arrange.addPoint(autoLane.index, autoLane.beatAt(mouse.x), autoLane.valueAt(mouse.y))
                                return
                            }
                            autoLane.dragging = near
                            autoLane.dragBeat = autoLane.points[near].beat
                            autoLane.dragValue = autoLane.points[near].value
                            curve.requestPaint()
                        }
                        onPositionChanged: mouse => {
                            if (autoLane.dragging < 0)
                                return
                            autoLane.dragBeat = autoLane.beatAt(mouse.x)
                            autoLane.dragValue = autoLane.valueAt(mouse.y)
                            curve.requestPaint()
                        }
                        onReleased: {
                            if (autoLane.dragging < 0)
                                return
                            const point = autoLane.dragging
                            autoLane.dragging = -1
                            Arrange.movePoint(autoLane.index, point, autoLane.dragBeat, autoLane.dragValue)
                            curve.requestPaint()
                        }
                    }
                }
            }

            // The loop and punch ranges over the lanes, where play starts, and the playhead.
            Rectangle {
                visible: Arrange.songMode && Arrange.loopOn
                x: Arrange.loopFrom * screen.barWidth
                y: screen.lanesTop
                width: (Arrange.loopTo - Arrange.loopFrom) * screen.barWidth
                height: screen.lanesHeight
                color: Theme.accentUi
                opacity: 0.08
            }
            Rectangle {
                visible: Arrange.songMode && Arrange.punchOn
                x: Arrange.punchFrom * screen.barWidth
                y: screen.lanesTop + 8 * screen.midiLaneHeight
                width: (Arrange.punchTo - Arrange.punchFrom) * screen.barWidth
                height: screen.audioLanes.length * screen.audioLaneHeight
                color: Theme.ledOn
                opacity: 0.12
            }
            Rectangle {
                visible: Arrange.songMode
                x: Arrange.startBar * screen.barWidth
                y: screen.rulerHeight
                width: 1
                height: parent.height - y
                color: Theme.accentUi
                opacity: 0.6
            }
            Rectangle {
                visible: Arrange.playing
                x: Arrange.position / 4 * screen.barWidth
                width: 2
                height: parent.height
                color: Theme.ledOn
            }
        }
    }

    Text { // no audio tracks yet
        x: timeline.x + 8
        y: names.y + screen.lanesTop + 8 * screen.midiLaneHeight + 8
        visible: screen.audioLanes.length === 0
        text: "Audio tracks you add from + show here."
        color: Theme.ink
        opacity: 0.6
        font.family: Theme.labelFont
        font.pixelSize: 13
    }

    // + AUTOMATION: an instrument, then one of its knobs.
    Popup {
        id: automationPopup
        x: (screen.width - width) / 2
        y: tools.y + tools.height + 6
        width: 560
        height: Math.min(screen.height - y - 16, 520)
        padding: 16
        modal: true
        background: Rectangle { radius: 8; color: Theme.lowerDeck; border.color: Theme.deckLine; border.width: 2 }
        property int device: -1
        readonly property var devices: opened ? Arrange.automatableDevices() : []
        onOpened: device = devices.length > 0 ? devices[0].device : -1

        Row {
            anchors.fill: parent
            spacing: 12
            Column { // the instruments
                width: 170
                spacing: 4
                Text {
                    text: "INSTRUMENT"
                    color: Theme.ink
                    font.family: Theme.labelFont
                    font.pixelSize: 12
                    font.bold: true
                }
                Repeater {
                    model: automationPopup.devices
                    Rectangle {
                        id: deviceRow
                        required property var modelData
                        readonly property bool chosen: automationPopup.device === modelData.device
                        width: 170
                        height: 28
                        radius: 3
                        color: chosen ? Theme.accentUi : Theme.silverLight
                        border.color: Theme.deckLine
                        Text {
                            x: 8
                            anchors.verticalCenter: parent.verticalCenter
                            text: deviceRow.modelData.name
                            color: deviceRow.chosen ? Theme.inkInverse : Theme.ink
                            font.family: Theme.labelFont
                            font.pixelSize: 14
                            font.bold: deviceRow.chosen
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: automationPopup.device = deviceRow.modelData.device
                        }
                    }
                }
            }
            Column { // its knobs: a click adds the lane
                width: parent.width - 182
                height: parent.height
                spacing: 4
                Text {
                    text: "KNOB: A CLICK ADDS ITS LANE"
                    color: Theme.ink
                    font.family: Theme.labelFont
                    font.pixelSize: 12
                    font.bold: true
                }
                ListView {
                    id: knobList
                    width: parent.width
                    height: parent.height - 24
                    clip: true
                    spacing: 2
                    model: automationPopup.device >= 0 ? Arrange.paramLabels(automationPopup.device) : []
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                    delegate: Rectangle {
                        id: knobRow
                        required property int index
                        required property string modelData
                        width: knobList.width - 12
                        height: 26
                        radius: 3
                        color: knobMouse.containsMouse ? Theme.silverLight : "transparent"
                        Text {
                            x: 8
                            anchors.verticalCenter: parent.verticalCenter
                            text: knobRow.modelData
                            color: Theme.ink
                            font.family: Theme.labelFont
                            font.pixelSize: 14
                        }
                        MouseArea {
                            id: knobMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: {
                                Arrange.addAutomationLane(automationPopup.device, knobRow.index)
                                automationPopup.close()
                            }
                        }
                    }
                }
            }
        }
    }

    // EXPORT: the song or the loop range to WAV, with stems if asked.
    Popup {
        id: exportPopup
        x: (screen.width - width) / 2
        y: tools.y + tools.height + 6
        width: 560
        padding: 20
        modal: true
        closePolicy: Export.busy ? Popup.NoAutoClose : Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle { radius: 8; color: Theme.lowerDeck; border.color: Theme.deckLine; border.width: 2 }
        property bool loopRange: false
        property bool stems: false
        property bool metronome: false

        function lengthText(seconds) {
            const s = Math.round(seconds)
            return Math.floor(s / 60) + ":" + String(s % 60).padStart(2, "0")
        }

        Column {
            width: parent.width
            spacing: 12
            Text {
                text: "EXPORT TO WAV"
                color: Theme.ink
                font.family: Theme.labelFont
                font.pixelSize: 16
                font.bold: true
            }
            Row {
                spacing: 8
                PanelButton {
                    width: 200
                    height: 34
                    faceText: "WHOLE SONG " + exportPopup.lengthText(exportPopup.opened ? Export.songSeconds() : 0)
                    text: "Export the whole song, from its start to its last clip"
                    tint: !exportPopup.loopRange ? Theme.labelOrange : "transparent"
                    onClicked: exportPopup.loopRange = false
                }
                PanelButton {
                    width: 200
                    height: 34
                    faceText: "LOOP RANGE " + (Arrange.loopFrom + 1) + "–" + Arrange.loopTo
                    text: "Export the loop range only"
                    tint: exportPopup.loopRange ? Theme.labelOrange : "transparent"
                    onClicked: exportPopup.loopRange = true
                }
            }
            Row {
                spacing: 8
                PanelButton {
                    width: 200
                    height: 34
                    faceText: "STEMS"
                    text: "Stems: each track to its own file too, as it sounds in the mix"
                    tint: exportPopup.stems ? Theme.labelOrange : "transparent"
                    onClicked: exportPopup.stems = !exportPopup.stems
                }
                PanelButton {
                    width: 200
                    height: 34
                    faceText: "METRONOME"
                    text: "Include the metronome"
                    tint: exportPopup.metronome ? Theme.labelOrange : "transparent"
                    onClicked: exportPopup.metronome = !exportPopup.metronome
                }
            }
            Text {
                width: parent.width
                wrapMode: Text.WordWrap
                text: "24-bit WAV, rendered faster than real time through the mixer, effects and automation as you hear them, into " + Export.folder
                      + ". Live sound pauses while it renders."
                color: Theme.ink
                opacity: 0.75
                font.family: Theme.labelFont
                font.pixelSize: 13
            }
            Rectangle { // progress
                visible: Export.busy
                width: parent.width
                height: 14
                radius: 3
                color: Theme.ledOff
                Rectangle {
                    width: parent.width * Export.progress
                    height: parent.height
                    radius: 3
                    color: Theme.labelOrange
                }
            }
            Text {
                visible: Export.message !== ""
                width: parent.width
                wrapMode: Text.WordWrap
                text: Export.message
                color: Theme.ink
                font.family: Theme.labelFont
                font.pixelSize: 14
                font.bold: true
            }
            Row {
                spacing: 8
                PanelButton {
                    width: 120
                    height: 36
                    faceText: Export.busy ? "STOP" : "EXPORT"
                    text: Export.busy ? "Stop exporting" : "Export now"
                    tint: Export.busy ? Theme.ledOn : "transparent"
                    onClicked: Export.busy ? Export.cancel() : Export.start(exportPopup.loopRange, exportPopup.stems, exportPopup.metronome)
                }
                PanelButton {
                    width: 140
                    height: 36
                    faceText: "OPEN FOLDER"
                    text: "Open the export folder"
                    enabled: !Export.busy
                    opacity: enabled ? 1 : 0.5
                    onClicked: Export.openFolder()
                }
                PanelButton {
                    width: 90
                    height: 36
                    faceText: "CLOSE"
                    text: "Close"
                    enabled: !Export.busy
                    opacity: enabled ? 1 : 0.5
                    onClicked: exportPopup.close()
                }
            }
        }
    }
}

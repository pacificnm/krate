pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Krate
import KrateRack

// The arranger (docs/DAW.md, D3): the sequencer's 8 tracks and the audio
// tracks in the rack as lanes across bars. SONG plays the arrangement instead
// of the patterns. The ruler sets where play starts; the LOOP and PUNCH strips
// under it are dragged to set those ranges. On a MIDI lane, a click places the
// chosen pattern (drag for a longer clip), a click on a clip opens its pattern
// in the Seq screen, a right click (or ERASE) removes it. On an audio lane,
// the takes on it; a right click (or ERASE) takes one off.
Item {
    id: screen

    property int brush: 0          // the pattern a click places
    property bool erasing: false
    property real barWidth: 48
    readonly property int headerWidth: 168
    readonly property int rulerHeight: 26
    readonly property int stripHeight: 18
    // The lanes fill the screen's height (an audio lane a little taller), within reason.
    readonly property int midiLaneHeight: Math.max(30, Math.min(56, Math.floor(
        (height - 112 - lanesTop - 24) / (8 + 1.4 * audioLanes.length))))
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
    readonly property real lanesHeight: 8 * midiLaneHeight + audioLanes.length * audioLaneHeight

    function barAt(x) { return Math.max(0, Math.min(Arrange.maxBars - 1, Math.floor(x / screen.barWidth))) }
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
                    onClicked: screen.erasing = !screen.erasing
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

    Text { // what this screen does in pattern mode, or while a DAW leads
        x: 16
        y: header.y + header.height + 2
        width: parent.width - 32
        elide: Text.ElideRight
        text: Arrange.following ? "A DAW leads over JACK: its position and its loop play (start, LOOP and PUNCH here wait)."
              : Arrange.songMode ? "SONG: the arrangement plays. Ruler: where play starts. Drag the LOOP and PUNCH strips to set them."
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
        y: header.y + header.height + 24
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
                Text {
                    x: 8
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - 48
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
                            const clip = Arrange.clipAt(midiLane.index, bar)
                            if (clip >= 0) {
                                if (mouse.button === Qt.RightButton || screen.erasing)
                                    Arrange.removeClip(midiLane.index, clip)
                                else if (midiLane.clips[clip].pattern >= 0)
                                    Sequencer.pattern = midiLane.clips[clip].pattern // to edit in the Seq screen
                                return
                            }
                            if (mouse.button === Qt.LeftButton && !screen.erasing) {
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
                            x: modelData.startBar * screen.barWidth + 1
                            width: modelData.bars * screen.barWidth - 2
                            height: midiLane.height
                            radius: 3
                            color: isPattern ? screen.patternColor(modelData.pattern) : "#9FB4C8"
                            border.color: Qt.darker(color, 1.4)
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
                    Repeater { // the takes on the lane, a later one on top
                        model: audioLane.clips
                        Rectangle {
                            id: takeBox
                            required property int index
                            required property var modelData
                            x: modelData.startBeat / 4 * screen.barWidth
                            width: Math.max(4, modelData.seconds * Rack.bpm / 60 / 4 * screen.barWidth)
                            height: audioLane.height
                            radius: 3
                            color: "#5E6F7E"
                            border.color: "#33404C"
                            clip: true
                            Canvas {
                                anchors.fill: parent
                                anchors.margins: 2
                                readonly property var peaks: Tracks.revision >= 0 && takeBox.modelData.loaded
                                                             ? Tracks.takePeaks(audioLane.modelData.track, takeBox.modelData.take) : []
                                onPeaksChanged: requestPaint()
                                onWidthChanged: requestPaint()
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
                            Text {
                                x: 4
                                y: 1
                                visible: takeBox.width > 60
                                text: takeBox.modelData.name.replace(/^take-/, "")
                                color: Theme.inkInverse
                                font.family: "monospace"
                                font.pixelSize: 10
                            }
                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                onClicked: mouse => {
                                    if (mouse.button === Qt.RightButton || screen.erasing)
                                        Tracks.removeClip(audioLane.modelData.track, takeBox.index)
                                    else
                                        Tracks.selectTake(audioLane.modelData.track, takeBox.modelData.take)
                                }
                            }
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
}

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Krate
import KrateRack

// The rack sequencer's screen (docs/SEQUENCER.md): RUN, the 16 patterns,
// length and grid; the 8 tracks (each playing an instrument); a piano roll
// for the selected track (drum lanes for Kessel) with a velocity lane.
Item {
    id: screen

    readonly property var trackList: Sequencer.revision >= 0 ? Sequencer.tracks : []
    readonly property var selected: trackList.length > Sequencer.track ? trackList[Sequencer.track] : ({})
    readonly property bool drums: selected.drums === true
    readonly property int stepsPerBeat: Sequencer.ticksPerBeat / Sequencer.gridTicks
    readonly property string lengthText: {
        const beats = Sequencer.length / Sequencer.ticksPerBeat
        const bars = Math.floor(beats / 4)
        const rest = beats - bars * 4
        if (rest === 0)
            return bars + (bars === 1 ? " bar" : " bars")
        const steps = Math.round(rest * screen.stepsPerBeat)
        return (bars > 0 ? bars + (bars === 1 ? " bar " : " bars ") : "") + "+" + steps + " steps"
    }

    function noteName(pitch) {
        const names = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
        return names[pitch % 12] + (Math.floor(pitch / 12) - 1)
    }

    Rectangle { // the deck
        anchors.fill: parent
        radius: 6
        color: Theme.lowerDeck
        border.color: Theme.deckLine
    }

    // --- Top: RUN, patterns, length, grid ---------------------------------------------
    Row {
        id: header
        x: 16
        y: 12
        height: 56
        spacing: 10

        PanelButton {
            width: 84
            height: 40
            anchors.verticalCenter: parent.verticalCenter
            faceText: "    RUN"
            text: "Play or stop the sequencer"
            onClicked: Sequencer.toggleRun()
            Led { // lit while it plays (or will, when the rack's PLAY starts)
                x: 12
                anchors.verticalCenter: parent.verticalCenter
                on: Sequencer.running
                opacity: Sequencer.playing || !Sequencer.running ? 1.0 : 0.5
            }
            LearnOverlay { anchors.fill: parent; paramKey: "rack:seqrun"; round: false }
        }

        Column {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2
            Text {
                text: "PATTERN"
                color: Theme.ink
                font.family: Theme.labelFont
                font.pixelSize: 11
                font.bold: true
            }
            Row {
                spacing: 3
                PanelButton {
                    width: 26
                    height: 30
                    faceText: "◂"
                    text: "Previous pattern"
                    onClicked: Sequencer.stepPattern(-1)
                    LearnOverlay { anchors.fill: parent; paramKey: "rack:seqprevious"; round: false }
                }
                Repeater {
                    model: 16
                    Rectangle {
                        id: patternButton
                        required property int index
                        readonly property bool chosen: Sequencer.pattern === index
                        readonly property bool sounding: Sequencer.playing && Sequencer.playingPattern === index
                        readonly property bool used: Sequencer.patternsUsed[index] === true
                        width: 32
                        height: 30
                        radius: 3
                        color: chosen ? Theme.silverLight : (used ? "#B9BBBF" : Theme.lowerDeck)
                        border.color: chosen ? Theme.accentUi : Theme.deckLine
                        border.width: chosen ? 3 : 1
                        Text {
                            anchors.centerIn: parent
                            text: patternButton.index + 1
                            color: Theme.ink
                            font.family: Theme.labelFont
                            font.pixelSize: 13
                            font.bold: patternButton.chosen
                        }
                        Rectangle { // in the range (RANGE mode)
                            visible: Sequencer.playMode === 1 && patternButton.index >= Sequencer.rangeFrom
                                     && patternButton.index <= Sequencer.rangeTo
                            x: 3
                            y: parent.height - 6
                            width: parent.width - 6
                            height: 3
                            color: Theme.accentUi
                        }
                        Rectangle { // playing now
                            visible: patternButton.sounding
                            x: 3
                            y: 3
                            width: 6
                            height: 6
                            radius: 3
                            color: Theme.ledOn
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: Sequencer.pattern = patternButton.index
                        }
                        LearnOverlay { anchors.fill: parent; paramKey: "rack:seqpattern" + (patternButton.index + 1); round: false }
                        Accessible.role: Accessible.Button
                        Accessible.name: "Pattern " + (index + 1) + (used ? "" : " (empty)") + (chosen ? " (chosen)" : "")
                    }
                }
                PanelButton {
                    width: 26
                    height: 30
                    faceText: "▸"
                    text: "Next pattern"
                    onClicked: Sequencer.stepPattern(1)
                    LearnOverlay { anchors.fill: parent; paramKey: "rack:seqnext"; round: false }
                }
            }
        }

        PanelButton {
            width: 62
            height: 34
            anchors.verticalCenter: parent.verticalCenter
            faceText: "COPY"
            text: "Copy this pattern"
            onClicked: Sequencer.copyPattern()
        }
        PanelButton {
            width: 62
            height: 34
            anchors.verticalCenter: parent.verticalCenter
            enabled: Sequencer.hasClipboard
            opacity: enabled ? 1 : 0.5
            faceText: "PASTE"
            text: "Paste the copied pattern over this one"
            onClicked: Sequencer.pastePattern()
        }
        PanelButton {
            width: 62
            height: 34
            anchors.verticalCenter: parent.verticalCenter
            faceText: "CLEAR"
            text: "Clear this pattern (every track)"
            onClicked: Sequencer.clearPattern()
        }

        Column {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2
            Text {
                text: "LENGTH"
                color: Theme.ink
                font.family: Theme.labelFont
                font.pixelSize: 11
                font.bold: true
            }
            Row {
                spacing: 4
                PanelButton {
                    width: 30
                    height: 28
                    faceText: "−"
                    text: "One bar shorter (a step, below a bar)"
                    onClicked: Sequencer.setLength(Sequencer.length > 4 * Sequencer.ticksPerBeat
                                                   ? Sequencer.length - 4 * Sequencer.ticksPerBeat
                                                   : Sequencer.length - Sequencer.gridTicks)
                }
                Rectangle {
                    width: 120
                    height: 28
                    radius: 3
                    color: Theme.lcdBg
                    Text {
                        anchors.centerIn: parent
                        text: screen.lengthText
                        color: Theme.lcdInk
                        font.family: "monospace"
                        font.pixelSize: 14
                    }
                    Accessible.role: Accessible.StaticText
                    Accessible.name: "Length " + screen.lengthText
                }
                PanelButton {
                    width: 30
                    height: 28
                    faceText: "+"
                    text: "One bar longer (up to 4)"
                    onClicked: Sequencer.setLength(Sequencer.length + 4 * Sequencer.ticksPerBeat
                                                   - Sequencer.length % (4 * Sequencer.ticksPerBeat))
                }
            }
        }

        Column {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2
            Text {
                text: "GRID"
                color: Theme.ink
                font.family: Theme.labelFont
                font.pixelSize: 11
                font.bold: true
            }
            ComboBox {
                width: 96
                height: 28
                model: Sequencer.gridNames
                currentIndex: Sequencer.grid
                onActivated: index => Sequencer.grid = index
                Accessible.name: "Grid"
            }
        }
    }

    // --- Left: the 8 tracks -------------------------------------------------------------
    Column {
        id: trackColumn
        x: 16
        y: header.y + header.height + 12
        width: 300
        spacing: 6

        // The sequence: its name (saved in the library, or not yet), SAVE, the library.
        Rectangle {
            width: trackColumn.width
            height: 76
            radius: 4
            color: Theme.lcdBg
            border.color: Theme.deckLine
            Text {
                x: 10
                y: 6
                width: parent.width - 20
                text: (Sequencer.sequenceId > 0 ? Sequencer.sequenceName : "Not saved yet")
                      + (Sequencer.sequenceId > 0 && Sequencer.modified ? "  •  changed" : "")
                color: Theme.lcdInk
                font.family: Theme.labelFont
                font.pixelSize: 15
                font.bold: true
                elide: Text.ElideRight
            }
            Row {
                x: 8
                y: 36
                spacing: 5
                PanelButton { // NEW: asks once more when there are unsaved changes
                    id: newButton
                    property bool confirming: false
                    width: confirming ? 150 : 56
                    height: 30
                    faceText: confirming ? "DROP CHANGES?" : "NEW"
                    text: confirming ? "Press again to clear it all without saving" : "Start a new, empty sequence"
                    onClicked: {
                        if (Sequencer.modified && !confirming) {
                            confirming = true
                            newConfirmTimer.restart()
                            return
                        }
                        confirming = false
                        Sequencer.newSequence()
                    }
                    Timer {
                        id: newConfirmTimer
                        interval: 4000
                        onTriggered: newButton.confirming = false
                    }
                }
                PanelButton {
                    visible: !newButton.confirming
                    width: 62
                    height: 30
                    faceText: "SAVE"
                    text: Sequencer.sequenceId > 0 ? "Save over " + Sequencer.sequenceName : "Save it in the library"
                    onClicked: {
                        if (Sequencer.sequenceId > 0)
                            Sequencer.saveSequence()
                        else
                            library.openSaveAs()
                    }
                }
                PanelButton {
                    visible: !newButton.confirming
                    width: 88
                    height: 30
                    faceText: "LIBRARY…"
                    text: "Saved sequences: open, save as, rename, delete"
                    onClicked: library.open()
                }
                PanelButton {
                    width: 64
                    height: 30
                    visible: Sequencer.sequenceId > 0 && !newButton.confirming
                    faceText: "CLOSE"
                    text: "Close it: back to what was here before, sounds and mixer too"
                    onClicked: Sequencer.closeSequence()
                }
            }
        }

        Repeater {
            model: screen.trackList

            Rectangle {
                id: trackRow
                required property int index
                required property var modelData
                readonly property bool chosen: Sequencer.track === index
                width: trackColumn.width
                height: 56
                radius: 4
                color: chosen ? Theme.silverLight : "#D9DADC"
                border.color: chosen ? Theme.accentUi : Theme.deckLine
                border.width: chosen ? 2 : 1

                MouseArea {
                    anchors.fill: parent
                    onClicked: Sequencer.track = trackRow.index
                }
                Text {
                    x: 10
                    y: 8
                    text: trackRow.index + 1
                    color: Theme.ink
                    font.family: Theme.labelFont
                    font.pixelSize: 20
                    font.bold: true
                }
                ComboBox {
                    x: 40
                    y: 6
                    width: 170
                    height: 30
                    model: Sequencer.targetChoices
                    currentIndex: trackRow.modelData.choice
                    onActivated: index => {
                        Sequencer.setTarget(trackRow.index, index)
                        Sequencer.track = trackRow.index
                    }
                    Accessible.name: "Track " + (trackRow.index + 1) + " instrument"
                }
                Text {
                    x: 42
                    y: 38
                    text: trackRow.modelData.notes + (trackRow.modelData.notes === 1 ? " note" : " notes")
                    color: "#55575B"
                    font.family: Theme.labelFont
                    font.pixelSize: 11
                }
                PanelButton {
                    x: parent.width - 52
                    y: 12
                    width: 40
                    height: 30
                    faceText: "M"
                    text: "Track " + (trackRow.index + 1) + " mute"
                    dark: trackRow.modelData.mute
                    onClicked: Sequencer.setMute(trackRow.index, !trackRow.modelData.mute)
                }
            }
        }

        Row {
            spacing: 6
            PanelButton {
                width: 96
                height: 32
                faceText: "COPY"
                text: "Copy the selected track's notes in this pattern"
                onClicked: Sequencer.copyTrack()
            }
            PanelButton {
                width: 96
                height: 32
                enabled: Sequencer.trackClipboard !== ""
                opacity: enabled ? 1 : 0.5
                faceText: "PASTE"
                text: "Paste the copied notes over the selected track"
                onClicked: Sequencer.pasteTrack()
            }
            PanelButton {
                width: 96
                height: 32
                faceText: "CLEAR"
                text: "Clear the selected track in this pattern"
                onClicked: Sequencer.clearTrack()
            }
        }
        Text {
            width: trackColumn.width
            text: Sequencer.trackClipboard !== "" ? "TRACK: copied from " + Sequencer.trackClipboard
                                                  : "TRACK: copy, paste or clear the selected one"
            color: "#55575B"
            font.family: Theme.labelFont
            font.pixelSize: 11
            elide: Text.ElideRight
        }
    }

    // --- Right, top: the chain ----------------------------------------------------------
    Row {
        id: chainStrip
        x: trackColumn.x + trackColumn.width + 16
        y: trackColumn.y
        width: screen.width - x - 16
        height: 40
        spacing: 6

        Row { // what plays: one pattern, a range of them, or the chain
            id: modeButtons
            spacing: 4
            anchors.verticalCenter: parent.verticalCenter
            Repeater {
                model: [{label: "PATTERN", key: "", tip: "Loop the pattern you choose"},
                        {label: "RANGE", key: "rack:seqrange", tip: "Play patterns FROM to TO in order, around and around"},
                        {label: "CHAIN", key: "rack:seqchain", tip: "Play the chain"}]
                PanelButton {
                    id: modeButton
                    required property int index
                    required property var modelData
                    width: 106
                    height: 34
                    faceText: "     " + modelData.label
                    text: modelData.tip
                    onClicked: Sequencer.playMode = index
                    Led {
                        x: 10
                        anchors.verticalCenter: parent.verticalCenter
                        on: Sequencer.playMode === modeButton.index
                    }
                    Loader { // learnable (RANGE, CHAIN)
                        anchors.fill: parent
                        active: modeButton.modelData.key !== ""
                        sourceComponent: LearnOverlay {
                            paramKey: modeButton.modelData.key
                            round: false
                        }
                    }
                    Accessible.role: Accessible.RadioButton
                    Accessible.checked: Sequencer.playMode === index
                }
            }
        }
        Row { // RANGE: from, to
            id: rangeControls
            visible: Sequencer.playMode === 1
            spacing: 6
            anchors.verticalCenter: parent.verticalCenter
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "FROM"
                color: Theme.ink
                font.family: Theme.labelFont
                font.pixelSize: 11
                font.bold: true
            }
            SpinBox {
                width: 110
                height: 32
                from: 1
                to: 16
                value: Sequencer.rangeFrom + 1
                onValueModified: Sequencer.rangeFrom = value - 1
                Accessible.name: "Range from pattern"
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "TO"
                color: Theme.ink
                font.family: Theme.labelFont
                font.pixelSize: 11
                font.bold: true
            }
            SpinBox {
                width: 110
                height: 32
                from: 1
                to: 16
                value: Sequencer.rangeTo + 1
                onValueModified: Sequencer.rangeTo = value - 1
                Accessible.name: "Range to pattern"
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: (Sequencer.rangeTo - Sequencer.rangeFrom + 1) + " patterns, in order, then around again"
                color: "#55575B"
                font.family: Theme.labelFont
                font.pixelSize: 12
            }
        }
        Flickable { // the steps: click one to edit its pattern; wheel: repeats; right-click: remove
            id: chainSteps
            visible: Sequencer.playMode !== 1
            width: chainStrip.width - modeButtons.width - addButton.width - clearChainButton.width - 3 * chainStrip.spacing
            height: 34
            anchors.verticalCenter: parent.verticalCenter
            contentWidth: chainRow.implicitWidth
            flickableDirection: Flickable.HorizontalFlick
            boundsBehavior: Flickable.StopAtBounds
            clip: true
            Row {
                id: chainRow
                spacing: 4
                Text {
                    visible: Sequencer.chain.length === 0
                    height: 34
                    verticalAlignment: Text.AlignVCenter
                    text: "The chain is empty: + ADD puts the pattern you're on at its end."
                    color: "#55575B"
                    font.family: Theme.labelFont
                    font.pixelSize: 12
                }
                Repeater {
                    model: Sequencer.chain
                    Rectangle {
                        id: chainChip
                        required property int index
                        required property var modelData
                        readonly property bool sounding: Sequencer.playing && Sequencer.chainStep === index
                        width: chipText.implicitWidth + 16
                        height: 34
                        radius: 3
                        color: Sequencer.pattern === modelData.pattern ? Theme.silverLight : "#D9DADC"
                        border.color: sounding ? Theme.ledOn : Theme.deckLine
                        border.width: sounding ? 3 : 1
                        Text {
                            id: chipText
                            anchors.centerIn: parent
                            text: (chainChip.modelData.pattern + 1)
                                  + (chainChip.modelData.repeats > 1 ? " ×" + chainChip.modelData.repeats : "")
                                  + (chainChip.sounding && chainChip.modelData.repeats > 1 ? " (" + (Sequencer.chainPass + 1) + ")" : "")
                            color: Theme.ink
                            font.family: Theme.labelFont
                            font.pixelSize: 14
                            font.bold: chainChip.sounding
                        }
                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            onClicked: mouse => {
                                if (mouse.button === Qt.RightButton)
                                    Sequencer.removeChainStep(chainChip.index)
                                else
                                    Sequencer.pattern = chainChip.modelData.pattern
                            }
                            onWheel: wheel => Sequencer.setChainRepeats(chainChip.index, chainChip.modelData.repeats
                                                                        + (wheel.angleDelta.y > 0 ? 1 : -1))
                        }
                        Accessible.role: Accessible.Button
                        Accessible.name: "Chain step " + (index + 1) + ": pattern " + (modelData.pattern + 1)
                                         + ", " + modelData.repeats + (modelData.repeats === 1 ? " time" : " times")
                    }
                }
            }
        }
        PanelButton {
            id: addButton
            visible: Sequencer.playMode !== 1
            width: 74
            height: 34
            anchors.verticalCenter: parent.verticalCenter
            faceText: "+ ADD"
            text: "Add the pattern you're on to the end of the chain"
            onClicked: Sequencer.addToChain(Sequencer.pattern)
        }
        PanelButton {
            id: clearChainButton
            visible: Sequencer.playMode !== 1
            width: 74
            height: 34
            anchors.verticalCenter: parent.verticalCenter
            faceText: "CLEAR"
            text: "Empty the chain"
            onClicked: Sequencer.clearChain()
        }
    }

    // --- Right: the piano roll ----------------------------------------------------------
    Rectangle {
        id: rollFrame
        x: trackColumn.x + trackColumn.width + 16
        y: chainStrip.y + chainStrip.height + 8
        width: screen.width - x - 16
        height: screen.height - y - 40
        radius: 4
        color: "#1E1F22"
        border.color: Theme.deckLine
        clip: true

        readonly property int keyWidth: screen.drums ? 110 : 52
        readonly property int velocityHeight: 90
        readonly property real rowHeight: screen.drums ? Math.max(18, (height - velocityHeight - 8) / 16) : 14
        readonly property real gridWidth: width - keyWidth - 2
        readonly property real pxPerTick: gridWidth / Math.max(1, Sequencer.length)
        readonly property var lanes: Sequencer.drumLanes
        readonly property int rows: screen.drums ? lanes.length : 128

        // Pitch at a row (top row = highest pitch); the row of a pitch (-1: not shown).
        function pitchAt(row) {
            return screen.drums ? lanes[row].note : 127 - row
        }
        function rowOf(pitch) {
            if (!screen.drums)
                return 127 - pitch
            for (let i = 0; i < lanes.length; ++i)
                if (lanes[i].note === pitch)
                    return i
            return -1
        }
        function snap(tick) {
            return Math.max(0, Math.floor(tick / Sequencer.gridTicks) * Sequencer.gridTicks)
        }

        Flickable {
            id: roll
            x: 1
            y: 1
            width: parent.width - 2
            height: parent.height - rollFrame.velocityHeight - 2
            contentWidth: width
            contentHeight: rollFrame.rows * rollFrame.rowHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            flickableDirection: Flickable.VerticalFlick
            ScrollBar.vertical: ScrollBar {}
            // Start around the notes, or around C3..C5.
            function centre() {
                if (screen.drums) {
                    contentY = 0
                    return
                }
                const notes = Sequencer.notes
                let pitch = 60
                if (notes.length > 0) {
                    let sum = 0
                    for (const n of notes)
                        sum += n.pitch
                    pitch = Math.round(sum / notes.length)
                }
                contentY = Math.max(0, Math.min(contentHeight - height,
                                                (127 - pitch) * rollFrame.rowHeight - height / 2))
            }
            Component.onCompleted: centre()
            readonly property int shownTrack: Sequencer.track
            onShownTrackChanged: centre()

            // Rows: keys on the left, lanes across.
            Repeater {
                model: rollFrame.rows
                Item {
                    id: lane
                    required property int index
                    readonly property int pitch: rollFrame.pitchAt(index)
                    readonly property bool black: !screen.drums && [1, 3, 6, 8, 10].indexOf(pitch % 12) >= 0
                    y: index * rollFrame.rowHeight
                    width: roll.width
                    height: rollFrame.rowHeight
                    Rectangle {
                        x: rollFrame.keyWidth
                        width: rollFrame.gridWidth
                        height: parent.height
                        color: lane.black ? "#26272B" : "#303135"
                    }
                    Rectangle { // between rows; an octave line under each C
                        x: rollFrame.keyWidth
                        y: parent.height - 1
                        width: rollFrame.gridWidth
                        height: 1
                        color: !screen.drums && lane.pitch % 12 === 0 ? "#6A6C70" : "#3A3B3F"
                    }
                    Rectangle { // the key
                        width: rollFrame.keyWidth - 2
                        height: parent.height - 1
                        color: lane.black ? "#2A2B2E" : "#E9E9EA"
                        Text {
                            x: 6
                            anchors.verticalCenter: parent.verticalCenter
                            visible: screen.drums || lane.pitch % 12 === 0
                            text: screen.drums ? rollFrame.lanes[lane.index].name : screen.noteName(lane.pitch)
                            color: lane.black ? Theme.inkInverse : Theme.ink
                            font.family: Theme.labelFont
                            font.pixelSize: screen.drums ? 12 : 10
                        }
                    }
                }
            }

            // Steps, beats and bars.
            Repeater {
                model: Math.ceil(Sequencer.length / Sequencer.gridTicks)
                Rectangle {
                    required property int index
                    readonly property int tick: index * Sequencer.gridTicks
                    readonly property bool bar: tick % (4 * Sequencer.ticksPerBeat) === 0
                    readonly property bool beat: tick % Sequencer.ticksPerBeat === 0
                    x: rollFrame.keyWidth + tick * rollFrame.pxPerTick
                    width: bar ? 2 : 1
                    height: roll.contentHeight
                    color: bar ? "#9A9CA0" : (beat ? "#5C5E62" : "#3E3F43")
                }
            }

            // Click an empty place: a note of one step.
            MouseArea {
                x: rollFrame.keyWidth
                width: rollFrame.gridWidth
                height: roll.contentHeight
                onClicked: mouse => {
                    const row = Math.floor(mouse.y / rollFrame.rowHeight)
                    if (row < 0 || row >= rollFrame.rows)
                        return
                    Sequencer.addNote(rollFrame.snap(mouse.x / rollFrame.pxPerTick), rollFrame.pitchAt(row),
                                      Sequencer.gridTicks, 100)
                }
            }

            // The notes.
            Repeater {
                model: Sequencer.notes
                Rectangle {
                    id: note
                    required property int index
                    required property var modelData
                    // While dragging, where it would go (committed on release: every
                    // edit rebuilds this list, so nothing is changed mid-drag).
                    property bool dragging: false
                    property int dragStart: modelData.start
                    property int dragRow: rollFrame.rowOf(modelData.pitch)
                    property int dragLength: modelData.length
                    readonly property int row: dragging ? dragRow : rollFrame.rowOf(modelData.pitch)
                    readonly property int start: dragging ? dragStart : modelData.start
                    readonly property int length: dragging ? dragLength : modelData.length
                    visible: row >= 0 && start < Sequencer.length
                    x: rollFrame.keyWidth + start * rollFrame.pxPerTick
                    y: row * rollFrame.rowHeight + 1
                    width: Math.max(4, Math.min(length, Sequencer.length - start) * rollFrame.pxPerTick - 1)
                    height: rollFrame.rowHeight - 2
                    radius: 2
                    color: Qt.rgba(1.0, 0.55 + 0.35 * (1 - modelData.velocity / 127), 0.1, 0.45 + 0.55 * modelData.velocity / 127)
                    border.color: dragging ? "#FFFFFF" : "#FFD8A8"
                    border.width: 1
                    z: dragging ? 2 : 1

                    MouseArea { // drag to move; right-click or double-click to delete; wheel: velocity
                        anchors.fill: parent
                        anchors.rightMargin: screen.drums ? 0 : 6
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        cursorShape: Qt.SizeAllCursor
                        property point grab
                        onPressed: mouse => {
                            if (mouse.button !== Qt.LeftButton)
                                return
                            grab = Qt.point(mouse.x, mouse.y)
                            note.dragStart = note.modelData.start
                            note.dragRow = rollFrame.rowOf(note.modelData.pitch)
                            note.dragLength = note.modelData.length
                        }
                        onClicked: mouse => {
                            if (mouse.button === Qt.RightButton)
                                Sequencer.deleteNote(note.index)
                        }
                        onDoubleClicked: Sequencer.deleteNote(note.index)
                        onPositionChanged: mouse => {
                            if (!(mouse.buttons & Qt.LeftButton))
                                return
                            note.dragging = true
                            const p = mapToItem(roll.contentItem, mouse.x - grab.x, mouse.y - grab.y + rollFrame.rowHeight / 2)
                            note.dragStart = Math.min(Sequencer.length - Sequencer.gridTicks,
                                                      rollFrame.snap((p.x - rollFrame.keyWidth) / rollFrame.pxPerTick + Sequencer.gridTicks / 2))
                            note.dragRow = Math.max(0, Math.min(rollFrame.rows - 1, Math.floor(p.y / rollFrame.rowHeight)))
                        }
                        onReleased: {
                            if (!note.dragging)
                                return
                            note.dragging = false
                            Sequencer.moveNote(note.index, note.dragStart, rollFrame.pitchAt(note.dragRow))
                        }
                        onWheel: wheel => Sequencer.setVelocity(note.index, note.modelData.velocity + (wheel.angleDelta.y > 0 ? 8 : -8))
                    }
                    MouseArea { // the right edge: drag to resize
                        anchors.right: parent.right
                        width: 6
                        height: parent.height
                        visible: !screen.drums
                        cursorShape: Qt.SizeHorCursor
                        onPressed: {
                            note.dragStart = note.modelData.start
                            note.dragRow = rollFrame.rowOf(note.modelData.pitch)
                            note.dragLength = note.modelData.length
                        }
                        onPositionChanged: mouse => {
                            if (!(mouse.buttons & Qt.LeftButton))
                                return
                            note.dragging = true
                            const p = mapToItem(roll.contentItem, mouse.x, 0)
                            const end = rollFrame.snap((p.x - rollFrame.keyWidth) / rollFrame.pxPerTick + Sequencer.gridTicks / 2)
                            note.dragLength = Math.max(Sequencer.gridTicks, end - note.modelData.start)
                        }
                        onReleased: {
                            if (!note.dragging)
                                return
                            note.dragging = false
                            Sequencer.resizeNote(note.index, note.dragLength)
                        }
                    }
                    Accessible.role: Accessible.Button
                    Accessible.name: (screen.drums && note.row >= 0 ? rollFrame.lanes[note.row].name : screen.noteName(modelData.pitch))
                                     + ", step " + (Math.floor(modelData.start / Sequencer.gridTicks) + 1)
                                     + ", velocity " + modelData.velocity
                }
            }

            // The playhead.
            Rectangle {
                visible: Sequencer.playing && Sequencer.playingPattern === Sequencer.pattern
                x: rollFrame.keyWidth + Sequencer.tick * rollFrame.pxPerTick
                width: 2
                height: roll.contentHeight
                color: Theme.ledOn
            }
        }

        // The velocity lane: a bar per note; drag up or down to set it.
        Rectangle {
            id: velocityLane
            x: 1
            y: parent.height - rollFrame.velocityHeight
            width: parent.width - 2
            height: rollFrame.velocityHeight - 1
            color: "#18191B"
            Rectangle {
                width: parent.width
                height: 1
                color: Theme.deckLine
            }
            Text {
                x: 6
                y: 6
                text: "VELOCITY"
                color: "#A0A2A6"
                font.family: Theme.labelFont
                font.pixelSize: 10
                font.bold: true
            }
            Repeater {
                model: Sequencer.notes
                Rectangle {
                    id: bar
                    required property int index
                    required property var modelData
                    readonly property real full: velocityLane.height - 14
                    property bool dragging: false
                    property int dragVelocity: modelData.velocity
                    readonly property int velocity: dragging ? dragVelocity : modelData.velocity
                    visible: modelData.start < Sequencer.length
                    x: rollFrame.keyWidth + modelData.start * rollFrame.pxPerTick
                    y: velocityLane.height - height - 2
                    width: Math.max(3, Math.min(10, Sequencer.gridTicks * rollFrame.pxPerTick - 2))
                    height: full * velocity / 127
                    color: dragging ? "#FFC27A" : "#FF8C1A"
                    MouseArea {
                        x: -4
                        width: parent.width + 8
                        y: -(bar.full - bar.height)
                        height: bar.full
                        cursorShape: Qt.SizeVerCursor
                        function set(mouseY) {
                            const p = mapToItem(velocityLane, 0, mouseY)
                            bar.dragVelocity = Math.max(1, Math.min(127, Math.round(127 * (velocityLane.height - 2 - p.y) / bar.full)))
                            bar.dragging = true
                        }
                        onPressed: mouse => set(mouse.y)
                        onPositionChanged: mouse => set(mouse.y)
                        onReleased: {
                            bar.dragging = false
                            Sequencer.setVelocity(bar.index, bar.dragVelocity)
                        }
                    }
                    Accessible.role: Accessible.Slider
                    Accessible.name: "Velocity " + velocity
                }
            }
        }
    }

    Text {
        x: rollFrame.x
        y: rollFrame.y + rollFrame.height + 10
        width: rollFrame.width
        text: (screen.selected.label ? "Track " + (Sequencer.track + 1) + " plays " + screen.selected.label + ".  "
                                     : "Track " + (Sequencer.track + 1) + " plays nothing yet: choose an instrument.  ")
              + (Sequencer.playMode === 2 ? "CHAIN plays the chain (wheel over a step: repeats; right-click: remove).  "
                 : Sequencer.playMode === 1 ? "RANGE plays patterns " + (Sequencer.rangeFrom + 1) + "–" + (Sequencer.rangeTo + 1) + " in order.  " : "")
              + "Click to add a note · drag it to move · drag its right edge to resize · right-click or double-click to delete · wheel or the lane below: velocity"
              + (screen.drums ? " (100 and up plays accented)" : "")
        color: Theme.ink
        font.family: Theme.labelFont
        font.pixelSize: 12
        elide: Text.ElideRight
    }

    // --- The library's saved sequences ------------------------------------------------
    Popup {
        id: library
        parent: Overlay.overlay
        x: Math.round((parent.width - width) / 2)
        y: Math.round((parent.height - height) / 2)
        width: 620
        height: Math.min(parent.height - 80, 640)
        modal: true
        padding: 20
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        property int confirmOpen: -1    // a sequence to open over unsaved changes
        property int confirmDelete: -1
        property int renaming: -1
        function openSaveAs() {
            saveAsName.text = Sequencer.sequenceId > 0 ? Sequencer.sequenceName + " 2" : ""
            open()
            saveAsName.forceActiveFocus()
        }
        onOpened: {
            confirmOpen = -1
            confirmDelete = -1
            renaming = -1
        }

        background: Rectangle {
            radius: 8
            color: Theme.silverLight
            border.color: Theme.deckLine
            border.width: 2
        }

        Item {
            anchors.fill: parent

            Text {
                id: libraryTitle
                text: "Saved sequences"
                color: Theme.ink
                font.family: Theme.labelFont
                font.pixelSize: 20
                font.bold: true
            }
            Text {
                anchors.top: libraryTitle.bottom
                anchors.topMargin: 4
                width: parent.width
                text: "Each keeps its patterns, tracks, chain and range, and the instruments' sounds and the mixer as they were when saved."
                wrapMode: Text.WordWrap
                color: "#55575B"
                font.family: Theme.labelFont
                font.pixelSize: 12
            }

            ListView {
                id: sequenceList
                y: 84
                width: parent.width
                height: parent.height - y - saveAsRow.height - 16
                clip: true
                spacing: 4
                model: Sequencer.savedSequences
                ScrollBar.vertical: ScrollBar {}

                Text {
                    visible: sequenceList.count === 0
                    y: 8
                    text: "None yet: give this one a name below and SAVE AS."
                    color: "#55575B"
                    font.family: Theme.labelFont
                    font.pixelSize: 13
                }

                delegate: Rectangle {
                    id: row
                    required property var modelData
                    readonly property bool isOpen: Sequencer.sequenceId === modelData.id
                    width: sequenceList.width
                    height: 46
                    radius: 4
                    color: isOpen ? "#F4E7D6" : "#D9DADC"
                    border.color: isOpen ? Theme.labelOrange : Theme.deckLine

                    Column {
                        x: 10
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - buttons.width - 30
                        visible: library.renaming !== row.modelData.id
                        Text {
                            width: parent.width
                            text: row.modelData.name + (row.isOpen ? "  (open)" : "")
                            color: Theme.ink
                            font.family: Theme.labelFont
                            font.pixelSize: 14
                            font.bold: row.isOpen
                            elide: Text.ElideRight
                        }
                        Text {
                            text: "saved " + row.modelData.saved
                            color: "#6A6C70"
                            font.family: Theme.labelFont
                            font.pixelSize: 10
                        }
                    }
                    TextField {
                        id: renameField
                        x: 10
                        width: parent.width - buttons.width - 30
                        anchors.verticalCenter: parent.verticalCenter
                        visible: library.renaming === row.modelData.id
                        onVisibleChanged: if (visible) { text = row.modelData.name; forceActiveFocus(); selectAll() }
                        onAccepted: {
                            Sequencer.renameSequence(row.modelData.id, text)
                            library.renaming = -1
                        }
                        Accessible.name: "New name"
                    }

                    Row {
                        id: buttons
                        anchors.right: parent.right
                        anchors.rightMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 6
                        PanelButton {
                            width: library.confirmOpen === row.modelData.id ? 170 : 66
                            height: 30
                            faceText: library.confirmOpen === row.modelData.id ? "OPEN, DROP CHANGES" : "OPEN"
                            text: "Open " + row.modelData.name
                            onClicked: {
                                const unsaved = Sequencer.modified
                                if (unsaved && library.confirmOpen !== row.modelData.id) {
                                    library.confirmOpen = row.modelData.id
                                    return
                                }
                                if (Sequencer.openSequence(row.modelData.id))
                                    library.close()
                            }
                        }
                        PanelButton {
                            width: 80
                            height: 30
                            faceText: library.renaming === row.modelData.id ? "OK" : "RENAME"
                            text: "Rename " + row.modelData.name
                            onClicked: {
                                if (library.renaming === row.modelData.id) {
                                    Sequencer.renameSequence(row.modelData.id, renameField.text)
                                    library.renaming = -1
                                } else {
                                    library.renaming = row.modelData.id
                                }
                            }
                        }
                        PanelButton {
                            width: library.confirmDelete === row.modelData.id ? 116 : 70
                            height: 30
                            faceText: library.confirmDelete === row.modelData.id ? "REALLY DELETE" : "DELETE"
                            text: "Delete " + row.modelData.name
                            onClicked: {
                                if (library.confirmDelete !== row.modelData.id) {
                                    library.confirmDelete = row.modelData.id
                                    return
                                }
                                Sequencer.deleteSequence(row.modelData.id)
                                library.confirmDelete = -1
                            }
                        }
                    }
                }
            }

            Row {
                id: saveAsRow
                anchors.bottom: parent.bottom
                spacing: 8
                TextField {
                    id: saveAsName
                    width: 300
                    placeholderText: "A name for what's in the sequencer now"
                    onAccepted: if (Sequencer.saveSequenceAs(text)) library.close()
                    Accessible.name: "Name to save as"
                }
                PanelButton {
                    width: 96
                    height: 34
                    enabled: saveAsName.text.trim() !== ""
                    opacity: enabled ? 1 : 0.5
                    faceText: "SAVE AS"
                    text: "Save what's in the sequencer as a new sequence"
                    onClicked: if (Sequencer.saveSequenceAs(saveAsName.text)) library.close()
                }
                PanelButton {
                    width: 80
                    height: 34
                    faceText: "CLOSE"
                    text: "Close this list"
                    onClicked: library.close()
                }
            }
        }
    }
}

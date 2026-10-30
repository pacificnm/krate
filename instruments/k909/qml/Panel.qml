pragma ComponentBehavior: Bound

import QtQuick
import Krate

// K909's panel at design size 1800 x 1000 (docs/UI.md): the knob sections
// (SOUND: each voice's own knobs; MIX: each voice's pan, drive and sends), the
// sequencer's step buttons and track strip, START/STOP/CLEAR, patterns A-H
// and the LCD block, the effects, UNMAP/AUDIO/LEARN with the status line and meter.
Item {
    id: panel

    width: 1800
    height: 1000

    // Panel-specific colors; shared ones come from Theme.
    readonly property color chassisLight: "#E6E4DC"
    readonly property color chassisDark: "#D2CFC5"
    readonly property color headerBar: "#363A3D"
    readonly property color lowerDeck: "#C9C6BC"
    readonly property color stripBar: "#4A4E52"

    // Knob sections, left to right. Width in columns; each knob sits at a
    // (column, row) inside its section. Two pages: SOUND (the voices' own
    // knobs) and MIX (each voice's pan, drive and effect sends).
    readonly property real margin: 24
    property bool mixPage: false
    readonly property var shownSections: mixPage ? mixSections : sections
    readonly property real columns: shownSections.reduce((sum, s) => sum + s.columns, 0)
    readonly property real column: (width - 2 * margin) / columns
    readonly property real sectionTop: 286
    readonly property real sectionBottom: 594
    readonly property var rowLabelY: [326, 416, 506]
    readonly property real knobSize: 56

    readonly property var sections: [
        { title: "ACCENT", columns: 1, knobs: [
            { key: "accent", label: "TOTAL", col: 0, row: 0 } ] },
        { title: "BASS DRUM", columns: 3, knobs: [
            { key: "bd_tune", label: "TUNE", col: 0, row: 0 }, { key: "bd_level", label: "LEVEL", col: 1, row: 0 },
            { key: "bd_decay", label: "DECAY", col: 2, row: 0 },
            { key: "bd_attack", label: "ATTACK", col: 0, row: 1 }, { key: "bd_sweep", label: "SWEEP", col: 1, row: 1 },
            { key: "bd_bend", label: "BEND", col: 2, row: 1 },
            { key: "bd_tone", label: "TONE", col: 0, row: 2 } ] },
        { title: "SNARE DRUM", columns: 2, knobs: [
            { key: "sd_tune", label: "TUNE", col: 0, row: 0 }, { key: "sd_level", label: "LEVEL", col: 1, row: 0 },
            { key: "sd_tone", label: "TONE", col: 0, row: 1 }, { key: "sd_snappy", label: "SNAPPY", col: 1, row: 1 },
            { key: "sd_decay", label: "DECAY", col: 0, row: 2 }, { key: "sd_crack", label: "CRACK", col: 1, row: 2 } ] },
        { title: "LOW TOM", columns: 1, knobs: [
            { key: "lt_tune", label: "TUNE", col: 0, row: 0 }, { key: "lt_level", label: "LEVEL", col: 0, row: 1 },
            { key: "lt_decay", label: "DECAY", col: 0, row: 2 } ] },
        { title: "MID TOM", columns: 1, knobs: [
            { key: "mt_tune", label: "TUNE", col: 0, row: 0 }, { key: "mt_level", label: "LEVEL", col: 0, row: 1 },
            { key: "mt_decay", label: "DECAY", col: 0, row: 2 } ] },
        { title: "HI TOM", columns: 1, knobs: [
            { key: "ht_tune", label: "TUNE", col: 0, row: 0 }, { key: "ht_level", label: "LEVEL", col: 0, row: 1 },
            { key: "ht_decay", label: "DECAY", col: 0, row: 2 } ] },
        { title: "RIM", columns: 1, knobs: [
            { key: "rs_tune", label: "TUNE", col: 0, row: 0 }, { key: "rs_level", label: "LEVEL", col: 0, row: 1 },
            { key: "rs_decay", label: "DECAY", col: 0, row: 2 } ] },
        { title: "CLAP", columns: 2, knobs: [
            { key: "cp_tune", label: "TUNE", col: 0, row: 0 }, { key: "cp_level", label: "LEVEL", col: 1, row: 0 },
            { key: "cp_decay", label: "DECAY", col: 0, row: 1 }, { key: "cp_spread", label: "SPREAD", col: 1, row: 1 } ] },
        { title: "HI HAT", columns: 2, knobs: [
            { key: "ch_level", label: "CH LEVEL", col: 0, row: 0 }, { key: "oh_level", label: "OH LEVEL", col: 1, row: 0 },
            { key: "ch_decay", label: "CH DECAY", col: 0, row: 1 }, { key: "oh_decay", label: "OH DECAY", col: 1, row: 1 },
            { key: "hh_tune", label: "TUNE", col: 0.5, row: 2 } ] },
        { title: "CYMBAL", columns: 2, knobs: [
            { key: "cr_level", label: "CRASH LVL", col: 0, row: 0 }, { key: "rd_level", label: "RIDE LVL", col: 1, row: 0 },
            { key: "cr_tune", label: "CRASH TUNE", col: 0, row: 1 }, { key: "rd_tune", label: "RIDE TUNE", col: 1, row: 1 } ] }
    ]

    // MIX page: a strip per voice, in track order.
    readonly property var voices: [
        { prefix: "bd", title: "BASS DRUM" }, { prefix: "sd", title: "SNARE" }, { prefix: "lt", title: "LOW TOM" },
        { prefix: "mt", title: "MID TOM" }, { prefix: "ht", title: "HI TOM" }, { prefix: "rs", title: "RIM" },
        { prefix: "cp", title: "CLAP" }, { prefix: "ch", title: "CLOSED HAT" }, { prefix: "oh", title: "OPEN HAT" },
        { prefix: "cr", title: "CRASH" }, { prefix: "rd", title: "RIDE" }
    ]
    readonly property var mixSections: voices.map(v => ({ title: v.title, columns: 2, knobs: [
        { key: v.prefix + "_pan", label: "PAN", col: 0, row: 0 }, { key: v.prefix + "_drive", label: "DRIVE", col: 1, row: 0 },
        { key: v.prefix + "_rev", label: "REVERB", col: 0, row: 1 }, { key: v.prefix + "_dly", label: "DELAY", col: 1, row: 1 } ] }))

    // Sequencer tracks under the step buttons, each spanning its steps (1-based).
    readonly property real stepLeft: 220
    readonly property real stepWidth: (width - margin - stepLeft) / 16
    readonly property var tracks: [
        { name: "BASS DRUM", first: 1, steps: 2 }, { name: "SNARE DRUM", first: 3, steps: 2 },
        { name: "LOW TOM", first: 5, steps: 2 }, { name: "MID TOM", first: 7, steps: 2 },
        { name: "HI TOM", first: 9, steps: 2 }, { name: "RIM", first: 11, steps: 1 },
        { name: "CLAP", first: 12, steps: 1 }, { name: "CLOSED", first: 13, steps: 1 },
        { name: "OPEN", first: 14, steps: 1 }, { name: "CRASH", first: 15, steps: 1 },
        { name: "RIDE", first: 16, steps: 1 }
    ]

    // A knob bound to one of Drums' parameters by key, MIDI-learnable.
    component DrumKnob: Knob {
        id: drumKnob
        required property string key
        width: panel.knobSize
        height: panel.knobSize
        name: Drums.label(key)
        Accessible.description: displayText
        value: Drums.revision >= 0 ? Drums.value(key) : 0
        defaultValue: Drums.defaultValue(key)
        displayText: Drums.displayText(key, value)
        onMoved: v => Drums.setValue(key, v)
        capColor: "#3A3B3E"
        capShadeColor: "#111213"
        capBorderColor: "#08080A"
        pointerColor: Theme.labelOrange

        LearnOverlay {
            anchors.fill: parent
            paramKey: drumKnob.key
        }
    }

    // Chassis
    Rectangle {
        anchors.fill: parent
        radius: 8
        border.color: Theme.deckLine
        border.width: 2
        gradient: Gradient {
            GradientStop { position: 0.0; color: panel.chassisLight }
            GradientStop { position: 1.0; color: panel.chassisDark }
        }
    }

    // Branding
    Text {
        x: 64
        y: 40
        text: "Krate"
        color: Theme.labelOrange
        font.family: Theme.labelFont
        font.pixelSize: 40
        font.bold: true
        font.italic: true
    }
    Text {
        x: 60
        y: 150
        text: "K909"
        color: panel.headerBar
        font.family: Theme.labelFont
        font.pixelSize: 112
        font.bold: true
    }
    Text {
        anchors.right: parent.right
        anchors.rightMargin: 64
        y: 160
        text: "DRUM MACHINE"
        color: panel.headerBar
        font.family: Theme.labelFont
        font.pixelSize: 40
        font.letterSpacing: 6
    }

    // SOUND / MIX: which knobs the sections show
    Repeater {
        model: [{ label: "SOUND", mix: false, x: 1560 }, { label: "MIX", mix: true, x: 1660 }]

        PanelButton {
            required property var modelData
            x: modelData.x
            y: 236
            width: 90
            height: 36
            faceText: modelData.label
            text: modelData.mix ? "Mix page: each voice's pan, drive and effect sends" : "Sound page: each voice's own knobs"
            dark: panel.mixPage === modelData.mix
            hasLed: true
            ledOn: panel.mixPage === modelData.mix
            onClicked: panel.mixPage = modelData.mix
        }
    }

    // Knob sections
    Repeater {
        model: panel.shownSections

        Item {
            id: section
            required property var modelData
            required property int index

            readonly property real firstColumn: {
                let c = 0
                for (let i = 0; i < index; ++i)
                    c += panel.shownSections[i].columns
                return c
            }

            x: panel.margin + firstColumn * panel.column
            y: panel.sectionTop
            width: modelData.columns * panel.column
            height: panel.sectionBottom - panel.sectionTop

            Rectangle {
                x: 3
                width: parent.width - 6
                height: 26
                color: panel.headerBar
                PanelLabel {
                    anchors.centerIn: parent
                    text: section.modelData.title
                    orange: true
                    font.pixelSize: 15
                }
            }

            // Divider to the next section
            Rectangle {
                visible: section.index < panel.shownSections.length - 1
                x: parent.width - 1
                y: 32
                width: 2
                height: parent.height - 32
                color: Theme.deckLine
                opacity: 0.6
            }

            Repeater {
                model: section.modelData.knobs

                Item {
                    id: knobSlot
                    required property var modelData
                    readonly property real cx: (modelData.col + 0.5) * panel.column
                    readonly property real labelY: panel.rowLabelY[modelData.row] - panel.sectionTop

                    PanelLabel {
                        centerX: knobSlot.cx
                        y: knobSlot.labelY
                        text: knobSlot.modelData.label
                        font.pixelSize: 12
                    }
                    DrumKnob {
                        key: knobSlot.modelData.key
                        x: knobSlot.cx - width / 2
                        y: knobSlot.labelY + 20
                    }
                }
            }
        }
    }

    // Middle deck labels (controls arrive in D2-D4)
    // Pattern buttons A-H: LED = the pattern playing (blinking: waiting for the
    // end of the pass); the dark button is the one shown and edited.
    readonly property string letters: "ABCDEFGH"
    property bool blink: false
    Timer {
        interval: 220
        repeat: true
        running: Drums.queuedPattern >= 0
        onTriggered: panel.blink = !panel.blink
        onRunningChanged: panel.blink = false
    }

    PanelLabel { x: 64; y: 614; text: "PATTERN"; orange: true }
    Repeater {
        model: 8

        PanelButton {
            id: patternButton
            required property int index
            x: 64 + (index % 4) * 78
            y: index < 4 ? 642 : 702
            width: 64
            height: 50
            faceText: panel.letters[index]
            text: "Pattern " + panel.letters[index]
            dark: Drums.patternIndex === index
            hasLed: true
            ledOn: Drums.queuedPattern === index ? panel.blink : Drums.playingPattern === index
            onClicked: Drums.patternIndex = index

            LearnOverlay {
                anchors.fill: parent
                paramKey: "pattern" + panel.letters[patternButton.index]
                round: false
            }
        }
    }

    // LCD block: the shown pattern, its length, step size and play order.
    Rectangle {
        x: 400
        y: 612
        width: 440
        height: 150
        radius: 4
        color: "transparent"
        border.color: Theme.deckLine
        border.width: 2
    }
    Repeater {
        model: [
            { label: "PATTERN", row: 0 }, { label: "LENGTH", row: 1 },
            { label: "SCALE", row: 2 }, { label: "ORDER", row: 3 }
        ]

        PanelLabel {
            required property var modelData
            x: 418
            y: 626 + modelData.row * 34
            text: modelData.label
            font.pixelSize: 13
        }
    }
    LcdField {
        name: "Pattern"
        x: 510
        y: 620
        width: 316
        height: 30
        text: "Pattern " + panel.letters[Drums.patternIndex]
              + (Drums.queuedPattern === Drums.patternIndex ? "  (next)"
                 : Drums.playing && Drums.playingPattern !== Drums.patternIndex
                   ? "  (" + panel.letters[Drums.playingPattern] + " playing)" : "")
        onUp: Drums.patternIndex = Math.min(7, Drums.patternIndex + 1)
        onDown: Drums.patternIndex = Math.max(0, Drums.patternIndex - 1)
    }
    LcdField {
        name: "Length"
        x: 510
        y: 654
        width: 316
        height: 30
        text: Drums.length + (Drums.length === 1 ? " step" : " steps")
        onUp: Drums.length = Drums.length + 1
        onDown: Drums.length = Drums.length - 1
    }
    LcdField {
        name: "Scale"
        x: 510
        y: 688
        width: 316
        height: 30
        text: Drums.scaleNames[Drums.scaleIndex]
        onUp: Drums.scaleIndex = Math.min(Drums.scaleNames.length - 1, Drums.scaleIndex + 1)
        onDown: Drums.scaleIndex = Math.max(0, Drums.scaleIndex - 1)
    }
    LcdField {
        name: "Play order"
        x: 510
        y: 722
        width: 316
        height: 30
        text: Drums.orderNames[Drums.orderIndex]
        onUp: Drums.orderIndex = Math.min(Drums.orderNames.length - 1, Drums.orderIndex + 1)
        onDown: Drums.orderIndex = Math.max(0, Drums.orderIndex - 1)
    }
    // Effects: the reverb (and its gate) and delay the voices send to, the bus compressor and crusher
    PanelLabel { x: 862; y: 614; text: "EFFECTS"; orange: true }
    Repeater {
        model: [{ key: "rev_size", label: "REV SIZE", cx: 890, detents: 0 }, { key: "rev_gate", label: "GATE", cx: 960, detents: 2 },
                { key: "dly_time", label: "DLY TIME", cx: 1030, detents: 7 }, { key: "dly_fb", label: "FEEDBACK", cx: 1100, detents: 0 },
                { key: "comp", label: "COMP", cx: 1170, detents: 0 }, { key: "crush", label: "CRUSH", cx: 1240, detents: 0 }]

        Item {
            id: fxSlot
            required property var modelData
            PanelLabel { centerX: fxSlot.modelData.cx; y: 644; text: fxSlot.modelData.label; font.pixelSize: 12 }
            DrumKnob {
                id: fxKnob
                key: fxSlot.modelData.key
                detents: fxSlot.modelData.detents
                x: fxSlot.modelData.cx - width / 2
                y: 664
            }
            PanelLabel {
                centerX: fxSlot.modelData.cx
                y: 724
                text: fxKnob.displayText
                font.pixelSize: 11
                font.bold: false
            }
        }
    }

    PanelLabel { centerX: 1330; y: 614; text: "TEMPO"; visible: Drums.ownsTransport }
    PanelLabel { centerX: 1440; y: 614; text: "VOLUME" }
    PanelLabel { centerX: 1550; y: 614; text: "SHUFFLE" }
    DrumKnob {
        key: "tempo"
        visible: Drums.ownsTransport
        x: 1330 - width / 2
        y: 642
        width: 84
        height: 84
    }
    DrumKnob {
        key: "volume"
        x: 1440 - width / 2
        y: 638
        width: 100
        height: 100
    }
    DrumKnob {
        key: "shuffle"
        detents: 6
        x: 1550 - width / 2
        y: 646
        width: 70
        height: 70
    }

    // Utilities: clear all MIDI mappings, audio status (click to reconnect),
    // MIDI Learn; the status line and output meter below.
    Repeater {
        model: [{ name: "UNMAP", cx: 1626 }, { name: "AUDIO", cx: 1688 }, { name: "LEARN", cx: 1750 }]

        Item {
            id: utility
            required property var modelData
            readonly property string name: modelData.name

            PanelLabel {
                centerX: utility.modelData.cx
                y: 614
                text: utility.name
                font.pixelSize: 12
            }
            PanelButton {
                x: utility.modelData.cx - width / 2
                y: 634
                width: 52
                height: 30
                text: utility.name === "AUDIO" ? "Audio: reconnect"
                    : utility.name === "LEARN" ? "MIDI Learn" : "Clear all MIDI mappings"
                hasLed: utility.name !== "UNMAP"
                ledOn: utility.name === "AUDIO" ? Host.audioConnected : MidiLearn.active
                onClicked: {
                    if (utility.name === "AUDIO")
                        Host.connectAudio()
                    else if (utility.name === "LEARN")
                        MidiLearn.active = !MidiLearn.active
                    else
                        MidiLearn.clearAll()
                }
            }
        }
    }
    PanelLabel { // audio status: rate, buffer, xruns, MIDI, or AUDIO OFFLINE
        x: 1600
        y: 682
        width: 176
        horizontalAlignment: Text.AlignLeft
        elide: Text.ElideRight
        text: Host.audioStatus
        color: Host.audioConnected ? Theme.ink : Theme.ledOn
        font.pixelSize: 11
    }
    Rectangle { // output level meter
        x: 1600
        y: 710
        width: 176
        height: 10
        radius: 2
        color: Theme.ledOff

        Rectangle {
            width: parent.width * Host.outputLevel
            height: parent.height
            radius: 2
            color: Theme.ledOn
        }
    }

    // Lower deck: transport buttons, step buttons and the track strip
    Rectangle {
        x: panel.margin
        y: 790
        width: panel.width - 2 * panel.margin
        height: panel.height - y - panel.margin
        radius: 6
        color: panel.lowerDeck
        border.color: Theme.deckLine
    }
    // START / STOP (standalone only: the rack's transport bar owns them) and CLEAR
    PanelButton {
        visible: Drums.canRun
        x: 60
        y: 812
        width: 120
        height: 54
        faceText: "START"
        text: "Start"
        hasLed: true
        ledOn: Drums.playing
        onClicked: Drums.playing = true

        LearnOverlay { // a MIDI button learned here toggles start/stop
            anchors.fill: parent
            paramKey: "startstop"
            round: false
        }
    }
    PanelButton {
        visible: Drums.canRun
        x: 60
        y: 876
        width: 120
        height: 44
        faceText: "STOP"
        text: "Stop"
        onClicked: Drums.playing = false
    }
    PanelButton {
        x: 60
        y: 932
        width: 120
        height: 36
        dark: true
        faceText: "CLEAR"
        text: "Clear track (Shift: whole pattern)"
        onClicked: (panelKeys.shift ? Drums.clearPattern() : Drums.clearTrack(Drums.selectedTrack))
    }

    // Step numbers, playhead LEDs and the step buttons for the selected track
    Repeater {
        model: 16

        Item {
            id: stepSlot
            required property int index
            readonly property real cx: panel.stepLeft + (index + 0.5) * panel.stepWidth
            readonly property int state: Drums.patternRevision >= 0
                                         ? Drums.stepState(Drums.selectedTrack, index) : 0
            readonly property bool inPattern: index < Drums.length

            PanelLabel {
                centerX: stepSlot.cx
                y: 806
                text: stepSlot.index + 1
                font.pixelSize: 15
                opacity: stepSlot.inPattern ? 1.0 : 0.35
            }
            Led {
                x: stepSlot.cx - width / 2
                y: 832
                on: Drums.currentStep === stepSlot.index
            }
            Rectangle { // the step button: cream face, lit inset
                id: stepButton
                activeFocusOnTab: true
                Keys.onReturnPressed: Drums.cycleStep(stepSlot.index)
                Keys.onEnterPressed: Drums.cycleStep(stepSlot.index)
                x: stepSlot.cx - width / 2
                y: stepMouse.pressed ? 854 : 852
                width: panel.stepWidth - 18
                height: 64
                radius: 5
                border.color: Theme.buttonEdge
                opacity: stepSlot.inPattern ? 1.0 : 0.5
                gradient: Gradient {
                    GradientStop { position: 0.0; color: Theme.buttonLight }
                    GradientStop { position: 1.0; color: Theme.buttonLightShade }
                }

                Accessible.role: Accessible.Button
                Accessible.name: "Step " + (stepSlot.index + 1)
                    + [": off", ": on", ": accent"][stepSlot.state]

                Rectangle {
                    anchors.centerIn: parent
                    width: parent.width * 0.5
                    height: parent.height * 0.42
                    radius: 3
                    border.color: "#2A2B2D"
                    color: stepSlot.state === 2 ? Theme.ledOn
                         : stepSlot.state === 1 ? Theme.labelOrange : "#55575B"
                }

                MouseArea {
                    id: stepMouse
                    anchors.fill: parent
                    onClicked: Drums.cycleStep(stepSlot.index)
                }
            }
        }
    }

    // Track strip: click selects the track the step buttons edit; right-click auditions it.
    Repeater {
        model: panel.tracks

        Rectangle {
            id: trackCell
            required property var modelData
            required property int index
            readonly property bool selected: Drums.selectedTrack === index
            x: panel.stepLeft + (modelData.first - 1) * panel.stepWidth + 4
            y: 936
            width: modelData.steps * panel.stepWidth - 8
            height: 30
            radius: 4
            color: trackMouse.pressed || selected ? Theme.labelOrange : "transparent"
            border.color: panel.stripBar
            border.width: 2

            Accessible.role: Accessible.Button
            Accessible.name: modelData.name + (selected ? ", selected" : "")

            PanelLabel {
                anchors.centerIn: parent
                text: trackCell.modelData.name
                font.pixelSize: 13
                inverse: trackCell.selected
            }

            MouseArea {
                id: trackMouse
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onPressed: mouse => {
                    if (mouse.button === Qt.RightButton)
                        Drums.audition(trackCell.index)
                    else
                        Drums.selectedTrack = trackCell.index
                }
            }
        }
    }

    // Keyboard (docs/UI.md): Space run/stop, Up/Down change track, A-H pattern,
    // L learn, Esc leaves learn. Shift is tracked for CLEAR.
    Item {
        id: panelKeys
        property bool shift: false
    }
    Keys.onSpacePressed: Drums.playing = !Drums.playing // no-op in the rack (no transport of its own)
    Keys.onUpPressed: Drums.selectedTrack = Drums.selectedTrack - 1
    Keys.onDownPressed: Drums.selectedTrack = Drums.selectedTrack + 1
    Keys.onPressed: event => {
        panelKeys.shift = (event.modifiers & Qt.ShiftModifier) !== 0
        if (event.modifiers === Qt.NoModifier && event.key === Qt.Key_L) {
            MidiLearn.active = !MidiLearn.active
            event.accepted = true
            return
        }
        if (event.key === Qt.Key_Escape && MidiLearn.active) {
            MidiLearn.active = false
            event.accepted = true
            return
        }
        // A-H select a pattern (no modifiers, so shortcuts elsewhere stay free).
        if (event.key >= Qt.Key_A && event.key <= Qt.Key_H && event.modifiers === Qt.NoModifier) {
            Drums.patternIndex = event.key - Qt.Key_A
            event.accepted = true
        }
    }
    Keys.onReleased: event => panelKeys.shift = (event.modifiers & Qt.ShiftModifier) !== 0
}

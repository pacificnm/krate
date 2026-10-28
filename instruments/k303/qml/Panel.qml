pragma ComponentBehavior: Bound

import QtQuick
import Krate

// The whole instrument at design resolution (1600 x 692); layout in docs/UI.md.
// Upper deck 0-340: sound knobs, tempo, pattern display, volume. Lower deck
// 340-692: run/stop, step row, note keys, step function buttons. All state
// lives in the Synth and Pattern singletons.
Item {
    id: panel

    width: Theme.designWidth
    height: Theme.designHeight

    // Space toggles run/stop from anywhere: no control accepts Space, so the key
    // propagates up from whatever has focus.
    focus: true
    Keys.onSpacePressed: Synth.playing = !Synth.playing

    // Editing shortcuts (docs/UI.md#keyboard-shortcuts)
    Keys.onPressed: event => {
        if (event.modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier))
            return
        switch (event.key) {
        case Qt.Key_Left: Pattern.selectPrevious(); break
        case Qt.Key_Right: Pattern.selectNext(); break
        case Qt.Key_A: Pattern.toggleAccent(); break
        case Qt.Key_S: Pattern.toggleSlide(); break
        case Qt.Key_N: Pattern.setGate(0); break
        case Qt.Key_T: Pattern.setGate(1); break
        case Qt.Key_R: Pattern.setGate(2); break
        case Qt.Key_L: MidiLearn.active = !MidiLearn.active; break
        case Qt.Key_Escape:
            if (!MidiLearn.active)
                return
            MidiLearn.active = false
            break
        default:
            if (event.key >= Qt.Key_1 && event.key <= Qt.Key_8)
                Pattern.index = event.key - Qt.Key_1
            else
                return
        }
        event.accepted = true
    }

    // ---- Upper deck ---------------------------------------------------------

    Rectangle {
        width: panel.width
        height: 342
        radius: 6
        border.color: Theme.deckLine
        border.width: 2
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.silverLight }
            GradientStop { position: 1.0; color: Theme.silverDark }
        }
    }

    Rectangle { // rule under the top row
        x: 16
        y: 140
        width: panel.width - 32
        height: 1
        color: Theme.deckLine
    }

    WaveSwitch {
        x: 96
        y: 6
        position: Synth.waveform >= 0.5 ? 1 : 0
        onToggled: position => Synth.waveform = position

        LearnOverlay {
            anchors.fill: parent
            paramKey: "waveform"
            round: false
        }
    }

    Row { // maker mark
        x: 28
        y: 76
        spacing: 10

        Rectangle {
            width: 40
            height: 40
            radius: 4
            color: Theme.ink

            Text {
                anchors.centerIn: parent
                text: "K"
                color: Theme.inkInverse
                font.family: Theme.labelFont
                font.pixelSize: 28
                font.bold: true
            }
        }

        Text {
            text: "k303"
            color: Theme.ink
            font.family: Theme.labelFont
            font.pixelSize: 38
            font.bold: true
            font.italic: true
        }
    }

    Repeater { // sound knobs
        model: [
            { name: "TUNING", key: "tuning", cx: 506 },
            { name: "CUT OFF FREQ", key: "cutoff", cx: 624 },
            { name: "RESONANCE", key: "resonance", cx: 742 },
            { name: "ENV MOD", key: "envmod", cx: 864 },
            { name: "DECAY", key: "decay", cx: 982 },
            { name: "ACCENT", key: "accent", cx: 1104 }
        ]

        Item {
            id: soundKnob
            required property var modelData

            PanelLabel {
                centerX: soundKnob.modelData.cx
                y: 8
                text: soundKnob.modelData.name
                font.pixelSize: 13
            }

            Knob {
                x: soundKnob.modelData.cx - width / 2
                y: 34
                width: 72
                height: 72
                name: soundKnob.modelData.name
                value: Synth[soundKnob.modelData.key]
                defaultValue: Synth.defaultValue(soundKnob.modelData.key)
                displayText: Synth.displayText(soundKnob.modelData.key, value)
                onMoved: v => Synth.setParamByKey(soundKnob.modelData.key, v)

                LearnOverlay {
                    anchors.fill: parent
                    paramKey: soundKnob.modelData.key
                }
            }
        }
    }

    Text { // wordmark
        x: 1196
        y: 58
        text: "Acid Bass"
        color: Theme.ink
        font.family: Theme.labelFont
        font.pixelSize: 58
        font.weight: Font.Black
        font.letterSpacing: 1
    }

    // Utility buttons, beat LED, pattern select
    Repeater {
        model: [{ name: "UNMAP", cx: 52 }, { name: "AUDIO", cx: 112 }, { name: "ABOUT", cx: 172 }]

        Item {
            id: utility
            required property var modelData

            PanelLabel {
                centerX: utility.modelData.cx
                y: 150
                text: utility.modelData.name
                font.pixelSize: 12
            }

            PanelButton {
                readonly property bool isAudio: utility.modelData.name === "AUDIO"

                x: utility.modelData.cx - width / 2
                y: 170
                width: 48
                height: 24
                text: isAudio ? "Audio: reconnect" : utility.modelData.name
                hasLed: isAudio
                ledOn: isAudio && Synth.audioConnected
                onClicked: {
                    if (isAudio)
                        Synth.connectAudio()
                    else if (utility.modelData.name === "UNMAP")
                        MidiLearn.clearAll()
                }
            }
        }
    }

    PanelLabel { // audio status: rate, buffer, xruns, or AUDIO OFFLINE
        x: 30
        y: 306
        text: Synth.audioStatus
        color: Synth.audioConnected ? Theme.ink : Theme.ledOn
        font.pixelSize: 12
    }

    Led { // beat indicator
        x: 32
        y: 226
        width: 40
        height: 26
        radius: 3
        on: Synth.beat // lit for the first step of each beat, at any Scale
    }

    Grid {
        x: 96
        y: 222
        columns: 4
        spacing: 10

        Repeater {
            model: 8

            PanelButton {
                required property int index
                width: 34
                height: 30
                faceText: index + 1
                text: "Pattern " + (index + 1)
                dark: Pattern.index === index
                onClicked: Pattern.index = index
            }
        }
    }

    // Tempo
    PanelLabel { centerX: 358; y: 150; text: "TEMPO" }
    Knob {
        x: 358 - width / 2
        y: 188
        width: 116
        height: 116
        name: "Tempo"
        value: Synth.tempo
        defaultValue: Synth.defaultValue("tempo")
        displayText: Synth.displayText("tempo", value)
        onMoved: v => Synth.tempo = v

        LearnOverlay {
            anchors.fill: parent
            paramKey: "tempo"
        }
    }

    // Scale selector (later feature)
    PanelLabel { centerX: 482; y: 150; text: "SCALE" }
    Knob {
        id: scaleKnob
        x: 482 - width / 2
        y: 216
        width: 66
        height: 66
        name: "Scale"
        value: Synth.scale
        displayText: Synth.displayText("scale", value)
        onMoved: v => Synth.scale = v
        detents: 3
        minAngle: -60
        maxAngle: 60

        LearnOverlay {
            anchors.fill: parent
            paramKey: "scale"
        }
    }
    Repeater {
        model: ["1/16", "1/32", "1/16T"]

        PanelLabel {
            required property int index
            required property string modelData
            readonly property real a: (-60 + index * 60) * Math.PI / 180
            centerX: 482 + 52 * Math.sin(a)
            y: 249 - 52 * Math.cos(a) - height / 2
            text: modelData
            font.pixelSize: 11
        }
    }

    // Play mode selector (later feature)
    PanelLabel { centerX: 610; y: 150; text: "PLAY MODE" }
    Knob {
        id: playModeKnob
        x: 604 - width / 2
        y: 216
        width: 66
        height: 66
        name: "Play mode"
        value: Synth.playMode
        displayText: Synth.displayText("playmode", value)
        onMoved: v => Synth.playMode = v
        detents: 5
        minAngle: 35
        maxAngle: 145

        LearnOverlay {
            anchors.fill: parent
            paramKey: "playmode"
        }
    }
    Column {
        x: 652
        y: 186
        spacing: 3

        Repeater {
            model: ["FORWARD", "REVERSE", "FWD+REV", "INVERT", "RANDOM"]

            Rectangle {
                required property int index
                required property string modelData
                readonly property bool active: Math.round(playModeKnob.value * 4) === index

                width: 80
                height: 19
                radius: 2
                color: active ? Theme.labelOrange : "transparent"

                PanelLabel {
                    anchors.verticalCenter: parent.verticalCenter
                    centerX: parent.width / 2
                    text: parent.modelData
                    font.pixelSize: 11
                    inverse: parent.active
                }
            }
        }
    }

    // Pattern / length display and output meter
    PanelLabel { x: 752; y: 150; text: "PATTERN" }
    LcdField {
        x: 752
        y: 170
        width: 352
        text: "Pattern " + (Pattern.index + 1)
        onUp: Pattern.index = Math.min(7, Pattern.index + 1)
        onDown: Pattern.index = Math.max(0, Pattern.index - 1)
    }
    PanelLabel { x: 752; y: 222; text: "LENGTH" }
    LcdField {
        x: 752
        y: 242
        width: 352
        text: Pattern.length + (Pattern.length === 1 ? " step" : " steps")
        onUp: Pattern.length = Pattern.length + 1
        onDown: Pattern.length = Pattern.length - 1
    }
    Rectangle { // output level meter
        x: 752
        y: 298
        width: 352
        height: 10
        radius: 2
        color: Theme.ledOff

        Rectangle {
            width: parent.width * Synth.outputLevel
            height: parent.height
            radius: 2
            color: Theme.ledOn
        }
    }

    // Model plate
    Rectangle {
        x: 1132
        y: 186
        width: 270
        height: 108
        color: "transparent"
        border.color: Theme.ink
        border.width: 2

        Rectangle {
            y: 58
            width: parent.width
            height: 2
            color: Theme.ink
        }

        Text {
            anchors.right: parent.right
            anchors.rightMargin: 14
            y: 6
            text: "K-303"
            color: Theme.ink
            font.family: Theme.labelFont
            font.pixelSize: 42
        }

        Text {
            anchors.right: parent.right
            anchors.rightMargin: 14
            y: 66
            text: "Acid Bass Synthesizer"
            color: Theme.ink
            font.family: Theme.labelFont
            font.pixelSize: 24
        }
    }

    // Volume
    PanelLabel { centerX: 1494; y: 160; text: "VOLUME" }
    Knob {
        x: 1494 - width / 2
        y: 188
        width: 116
        height: 116
        name: "Volume"
        value: Synth.volume
        defaultValue: Synth.defaultValue("volume")
        displayText: Synth.displayText("volume", value)
        onMoved: v => Synth.volume = v

        LearnOverlay {
            anchors.fill: parent
            paramKey: "volume"
        }
    }

    // ---- Lower deck ---------------------------------------------------------

    Rectangle {
        y: 342
        width: panel.width
        height: panel.height - 342
        radius: 6
        color: Theme.lowerDeck
        border.color: Theme.deckLine
        border.width: 2
    }

    Repeater { // group dividers
        model: [198, 1416]

        Rectangle {
            required property real modelData
            x: modelData
            y: 356
            width: 2
            height: 320
            color: Theme.deckLine
        }
    }

    // MIDI learn and run/stop
    PanelLabel { centerX: 103; y: 366; text: "MIDI LEARN"; font.pixelSize: 13 }
    PanelButton {
        x: 103 - width / 2
        y: 390
        width: 58
        height: 36
        text: "MIDI Learn"
        hasLed: true
        ledOn: MidiLearn.active
        onClicked: MidiLearn.active = !MidiLearn.active
    }

    PanelButton {
        id: runStop
        x: 32
        y: 506
        width: 142
        height: 132
        text: "Run / Stop"
        indicatorBar: true
        ledOn: Synth.playing
        onClicked: Synth.playing = !Synth.playing
    }
    PanelLabel { centerX: 103; y: 650; text: "RUN / STOP" }

    // Keyboard and time-mode buttons
    PanelButton {
        id: keyboardButton
        x: 211
        y: 360
        width: 140
        height: 86
        dark: true
        hasLed: true
        ledOn: Pattern.keyboardMode
        text: "Keyboard: note keys play"
        onClicked: Pattern.keyboardMode = !Pattern.keyboardMode
    }
    PanelLabel {
        centerX: 281
        y: 404
        text: "KEYBOARD"
        inverse: true
        font.pixelSize: 14
    }

    PanelLabel { centerX: 280; y: 536; text: "TIME" }
    PanelButton {
        x: 280 - width / 2
        y: 558
        width: 58
        height: 80
        text: "Time mode"
        hasLed: true
        ledOn: Pattern.timeMode
        onClicked: Pattern.timeMode = !Pattern.timeMode
    }

    // Steps and note keys
    StepRow {
        x: 362
        y: 360
        selectedStep: Pattern.selectedStep
        playStep: Synth.currentStep
        onStepClicked: step => Pattern.selectedStep = step
    }

    PianoKeys {
        x: 362
        y: 458
        litKey: Pattern.keyboardMode ? -1 : Pattern.selectedNote
        onKeyClicked: semitone => {
            if (!Pattern.keyboardMode)
                Pattern.setNote(semitone)
        }
        onKeyPressed: semitone => {
            if (Pattern.keyboardMode)
                Pattern.auditionOn(semitone)
        }
        onKeyReleased: () => {
            if (Pattern.keyboardMode)
                Pattern.auditionOff()
        }
    }

    // Step function buttons
    Rectangle {
        x: 1082
        y: 458
        width: 326
        height: 210
        radius: 4
        color: Theme.keyBed
        border.color: Theme.deckLine
    }
    Rectangle {
        x: 1086
        y: 488
        width: 318
        height: 30
        color: Theme.band
    }

    Repeater {
        model: [
            { name: "ACCENT", time: "REST", cx: 1123 },
            { name: "SLIDE", time: "", cx: 1204 },
            { name: "DOWN", time: "NOTE", cx: 1286 },
            { name: "UP", time: "TIE", cx: 1367 }
        ]

        Item {
            id: fn

            required property var modelData
            required property int index

            // Time mode shows the gate on ACCENT (rest), DOWN (note) and UP (tie).
            readonly property bool lit: index === 1 ? Pattern.selectedSlide
                                      : Pattern.timeMode ? Pattern.selectedGate === [2, -1, 0, 1][index]
                                      : index === 0 ? Pattern.selectedAccent
                                      : index === 2 ? Pattern.selectedOctave < 0
                                      : Pattern.selectedOctave > 0

            PanelLabel {
                centerX: fn.modelData.cx
                y: 466
                text: fn.modelData.time
                font.pixelSize: 12
            }

            PanelLabel {
                centerX: fn.modelData.cx
                y: 494
                text: fn.modelData.name
                inverse: true
                font.pixelSize: 13
            }

            Led {
                x: fn.modelData.cx - width / 2
                y: 532
                on: fn.lit
            }

            PanelButton {
                x: fn.modelData.cx - width / 2
                y: 558
                width: 58
                height: 80
                text: fn.modelData.name
                onClicked: {
                    if (fn.index === 1)
                        Pattern.toggleSlide()
                    else if (Pattern.timeMode)
                        Pattern.setGate([2, -1, 0, 1][fn.index]) // Rest, -, Note, Tie
                    else if (fn.index === 0)
                        Pattern.toggleAccent()
                    else if (fn.index === 2)
                        Pattern.octaveDown()
                    else
                        Pattern.octaveUp()
                }
            }
        }
    }

    PanelLabel {
        centerX: 1326
        y: 642
        text: "OCTAVE"
        orange: true
        font.pixelSize: 13
    }
    Rectangle {
        x: 1250
        y: 661
        width: 152
        height: 2
        color: Theme.labelOrange
    }

    // Randomize and clear
    PanelLabel { centerX: 1504; y: 366; text: "RANDOMIZE"; font.pixelSize: 13 }
    PanelButton {
        x: 1504 - width / 2
        y: 390
        width: 58
        height: 36
        text: "Randomize pattern"
        onClicked: Pattern.randomize()
    }

    PanelButton {
        x: 1432
        y: 506
        width: 142
        height: 132
        text: "Clear pattern"
        onClicked: Pattern.clear()
    }
    PanelLabel { centerX: 1503; y: 650; text: "CLEAR" }
}

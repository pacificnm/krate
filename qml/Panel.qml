pragma ComponentBehavior: Bound

import QtQuick

// The whole instrument at design resolution (1600 x 692), laid out after the
// reference panel in docs/UI.md. Upper deck 0-340: sound knobs, tempo, pattern
// display, volume. Lower deck 340-692: run/stop, step row, note keys, step
// function buttons.
//
// Control state below is local placeholder state for the layout milestone; it
// moves to the C++ singletons in M2 (knobs) and M5 (pattern editing).
Item {
    id: panel

    property int selectedStep: 0
    property int selectedPattern: 0
    property int patternLength: 16
    property int litKey: 0
    property int octave: 0
    property bool accent: false
    property bool slide: false

    width: Theme.designWidth
    height: Theme.designHeight

    // Space toggles run/stop from anywhere: no control accepts Space, so the key
    // propagates up from whatever has focus.
    focus: true
    Keys.onSpacePressed: Synth.playing = !Synth.playing

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
        on: Synth.currentStep >= 0 && Synth.currentStep % 4 === 0 // lit for the first step of each beat
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
                checked: panel.selectedPattern === index
                dark: panel.selectedPattern === index
                onClicked: panel.selectedPattern = index
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
        value: 0
        onMoved: v => value = v // later feature: no model yet
        detents: 3
        minAngle: -60
        maxAngle: 60
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
        value: 0
        onMoved: v => value = v // later feature: no model yet
        detents: 5
        minAngle: 35
        maxAngle: 145
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
        text: "Pattern " + (panel.selectedPattern + 1)
        onUp: panel.selectedPattern = Math.min(7, panel.selectedPattern + 1)
        onDown: panel.selectedPattern = Math.max(0, panel.selectedPattern - 1)
    }
    PanelLabel { x: 752; y: 222; text: "LENGTH" }
    LcdField {
        x: 752
        y: 242
        width: 352
        text: panel.patternLength + " steps"
        onUp: panel.patternLength = Math.min(16, panel.patternLength + 1)
        onDown: panel.patternLength = Math.max(1, panel.patternLength - 1)
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
        checkable: true
        hasLed: true
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
        checkable: true
        hasLed: true
        text: "Keyboard"
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
        checkable: true
        hasLed: true
    }

    // Steps and note keys
    StepRow {
        x: 362
        y: 360
        selectedStep: panel.selectedStep
        playStep: Synth.currentStep
        onStepClicked: step => panel.selectedStep = step
    }

    PianoKeys {
        x: 362
        y: 458
        litKey: panel.litKey
        onKeyClicked: semitone => panel.litKey = semitone
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

            readonly property bool lit: index === 0 ? panel.accent
                                      : index === 1 ? panel.slide
                                      : index === 2 ? panel.octave < 0
                                      : panel.octave > 0

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
                    if (fn.index === 0) panel.accent = !panel.accent
                    else if (fn.index === 1) panel.slide = !panel.slide
                    else if (fn.index === 2) panel.octave = Math.max(-1, panel.octave - 1)
                    else panel.octave = Math.min(1, panel.octave + 1)
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
        text: "Randomize"
    }

    PanelButton {
        x: 1432
        y: 506
        width: 142
        height: 132
        text: "Clear pattern"
    }
    PanelLabel { centerX: 1503; y: 650; text: "CLEAR" }
}

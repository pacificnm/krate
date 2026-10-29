pragma ComponentBehavior: Bound

import QtQuick
import Krate

// Kalliope's panel at design size 1600 x 1100 (docs/UI.md): the sound sections
// (sliders, selectors, switches), the function deck (arpeggio, key mode, effects;
// patches arrive in P5) and the keyboard deck (volume, tempo, octave, the
// on-screen keyboard, utilities). Every control is bound to Kalliope by key.
Item {
    id: panel

    width: 1600
    height: 1100

    // Panel-specific colors; shared ones come from Theme.
    readonly property color chassis: "#1C1D1F"
    readonly property color chassisEdge: "#0B0B0C"
    readonly property color deck: "#26272A"
    readonly property color header: "#E2701F"
    readonly property color silk: "#ECECEC"
    readonly property color dim: "#8E9094"

    // Sound sections, left to right. Width in columns; each control sits at a
    // column inside its section. kind: slider, knob (selector or continuous), switch.
    readonly property real margin: 24
    readonly property real columns: 34.5
    readonly property real column: (width - 2 * margin) / columns
    readonly property real sectionTop: 250
    readonly property real sectionBottom: 540

    readonly property var sections: [
        { title: "LFO", columns: 3, controls: [
            { key: "lfo_rate", label: "RATE", kind: "slider", col: 0 },
            { key: "lfo_delay", label: "DELAY\nTIME", kind: "slider", col: 1 },
            { key: "lfo_wave", label: "WAVE", kind: "knob", col: 2, valueBelow: true } ] },
        { title: "VCO MODULATOR", columns: 5, controls: [
            { key: "mod_lfo", label: "LFO\nMOD", kind: "slider", col: 0 },
            { key: "mod_env", label: "ENV\nMOD", kind: "slider", col: 1 },
            { key: "mod_dest", label: "DEST", kind: "switch", col: 2 },
            { key: "pwm", label: "PW\nMOD", kind: "slider", col: 3 },
            { key: "pwm_src", label: "PWM\nSRC", kind: "switch", col: 4 } ] },
        { title: "VCO-1", columns: 3, controls: [
            { key: "cross_mod", label: "CROSS\nMOD", kind: "slider", col: 0 },
            { key: "vco1_range", label: "RANGE", kind: "knob", col: 1.5 },
            { key: "vco1_wave", label: "WAVE", kind: "knob", col: 1.5, row: 1 } ] },
        { title: "VCO-2", columns: 3, controls: [
            { key: "vco2_range", label: "RANGE", kind: "knob", col: 0.5 },
            { key: "vco2_fine", label: "FINE\nTUNE", kind: "knob", col: 2 },
            { key: "vco2_wave", label: "WAVE", kind: "knob", col: 0.5, row: 1 },
            { key: "sync", label: "SYNC", kind: "switch", col: 2, row: 1 } ] },
        { title: "MIX", columns: 1.5, controls: [
            { key: "mix", label: "SOURCE\nMIX", kind: "knob", col: 0.25 } ] },
        { title: "HPF", columns: 1, controls: [
            { key: "hpf", label: "FREQ", kind: "slider", col: 0 } ] },
        { title: "VCF", columns: 7, controls: [
            { key: "cutoff", label: "CUTOFF\nFREQ", kind: "slider", col: 0 },
            { key: "res", label: "RES", kind: "slider", col: 1 },
            { key: "slope", label: "SLOPE", kind: "switch", col: 2 },
            { key: "vcf_env", label: "ENV\nMOD", kind: "slider", col: 3 },
            { key: "vcf_env_src", label: "ENV\nSRC", kind: "switch", col: 4 },
            { key: "vcf_lfo", label: "LFO\nMOD", kind: "slider", col: 5 },
            { key: "key_follow", label: "KEY\nFOLLOW", kind: "slider", col: 6 } ] },
        { title: "VCA", columns: 2, controls: [
            { key: "vca_level", label: "LEVEL", kind: "slider", col: 0 },
            { key: "vca_lfo", label: "LFO\nMOD", kind: "slider", col: 1 } ] },
        { title: "ENV-1", columns: 4, controls: [
            { key: "env1_a", label: "A", kind: "slider", col: 0 },
            { key: "env1_d", label: "D", kind: "slider", col: 1 },
            { key: "env1_s", label: "S", kind: "slider", col: 2 },
            { key: "env1_r", label: "R", kind: "slider", col: 3 } ] },
        { title: "ENV-2", columns: 5, controls: [
            { key: "env2_a", label: "A", kind: "slider", col: 0 },
            { key: "env2_d", label: "D", kind: "slider", col: 1 },
            { key: "env2_s", label: "S", kind: "slider", col: 2 },
            { key: "env2_r", label: "R", kind: "slider", col: 3 },
            { key: "env_key", label: "KEY\nFOLLOW", kind: "switch", col: 4 } ] }
    ]

    // ---- Controls bound to Kalliope's parameters by key; all MIDI-learnable.

    component ParamSlider: PanelSlider {
        id: paramSlider
        required property string key
        name: Kalliope.label(key)
        value: Kalliope.revision >= 0 ? Kalliope.value(key) : 0
        defaultValue: Kalliope.defaultValue(key)
        displayText: Kalliope.displayText(key, value)
        onMoved: v => Kalliope.setValue(key, v)

        LearnOverlay {
            anchors.fill: parent
            paramKey: paramSlider.key
            round: false
        }
    }

    component ParamSwitch: PanelSwitch {
        id: paramSwitch
        required property string key
        name: Kalliope.label(key)
        positions: Kalliope.detents(key)
        value: Kalliope.revision >= 0 ? Kalliope.value(key) : 0
        displayText: Kalliope.displayText(key, value)
        onMoved: v => Kalliope.setValue(key, v)

        LearnOverlay {
            anchors.fill: parent
            paramKey: paramSwitch.key
            round: false
        }
    }

    // Black knob, white pointer; a selector (detents) or continuous.
    component ParamKnob: Knob {
        id: paramKnob
        required property string key
        name: Kalliope.label(key)
        detents: Kalliope.detents(key)
        value: Kalliope.revision >= 0 ? Kalliope.value(key) : 0
        defaultValue: Kalliope.defaultValue(key)
        displayText: Kalliope.displayText(key, value)
        onMoved: v => Kalliope.setValue(key, v)
        capColor: "#2C2D30"
        capShadeColor: "#101112"
        capBorderColor: "#060607"
        pointerColor: "#F0F0F0"
        Accessible.description: displayText

        LearnOverlay {
            anchors.fill: parent
            paramKey: paramKnob.key
        }
    }

    // A selector with its positions written around it (ring), or, where the
    // names are too long for that, its current position written under it.
    component Selector: Item {
        id: selector
        required property string key
        property real size: 56
        property bool ring: true
        readonly property int count: Kalliope.detents(key)

        width: size
        height: size

        ParamKnob {
            key: selector.key
            anchors.fill: parent
        }
        ValueText {
            visible: !selector.ring
            x: selector.size / 2 - width / 2
            y: selector.size + 4
            width: selector.size + 20
            text: Kalliope.revision >= 0 ? Kalliope.displayText(selector.key, Kalliope.value(selector.key)) : ""
        }
        Repeater {
            model: selector.ring ? selector.count : 0

            Text {
                required property int index
                readonly property real angle: (-135 + 270 * index / (selector.count - 1)) * Math.PI / 180
                readonly property real radius: selector.size / 2 + 13
                x: selector.size / 2 + radius * Math.sin(angle) - width / 2
                y: selector.size / 2 - radius * Math.cos(angle) - height / 2
                text: Kalliope.displayText(selector.key, index / (selector.count - 1))
                color: panel.silk
                font.family: Theme.labelFont
                font.pixelSize: 10
            }
        }
    }

    // Small caption under a control: its current value.
    component ValueText: Text {
        color: panel.dim
        font.family: Theme.labelFont
        font.pixelSize: 10
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
    }

    component DeckLabel: Text {
        color: panel.silk
        font.family: Theme.labelFont
        font.pixelSize: 12
        font.bold: true
        horizontalAlignment: Text.AlignHCenter
    }

    // Chassis
    Rectangle {
        anchors.fill: parent
        radius: 8
        color: panel.chassis
        border.color: panel.chassisEdge
        border.width: 3
    }

    // Branding
    Text {
        x: 52
        y: 60
        text: "Krate"
        color: panel.header
        font.family: Theme.labelFont
        font.pixelSize: 40
        font.bold: true
        font.italic: true
    }
    Text {
        id: wordmark
        anchors.right: parent.right
        anchors.rightMargin: 52
        y: 70
        text: "KALLIOPE"
        color: panel.silk
        font.family: Theme.labelFont
        font.pixelSize: 84
        font.weight: Font.Black
        font.letterSpacing: 8
    }
    Text {
        anchors.right: wordmark.right
        y: 172
        text: "POLYPHONIC SYNTHESIZER"
        color: panel.dim
        font.family: Theme.labelFont
        font.pixelSize: 22
        font.letterSpacing: 6
    }

    // Sound sections
    Repeater {
        model: panel.sections

        Item {
            id: section
            required property var modelData
            required property int index

            readonly property real firstColumn: {
                let c = 0
                for (let i = 0; i < index; ++i)
                    c += panel.sections[i].columns
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
                color: panel.header
                Text {
                    anchors.centerIn: parent
                    text: section.modelData.title
                    color: "#111111"
                    font.family: Theme.labelFont
                    font.pixelSize: 15
                    font.bold: true
                }
            }

            Rectangle { // divider to the next section
                visible: section.index < panel.sections.length - 1
                x: parent.width - 1
                y: 34
                width: 2
                height: parent.height - 34
                color: "#3A3B3E"
            }

            Repeater {
                model: section.modelData.controls.filter(c => c.kind === "slider")

                ParamSlider {
                    required property var modelData
                    key: modelData.key
                    x: (modelData.col + 0.5) * panel.column - width / 2
                    y: 80
                    height: 184
                }
            }
            Repeater {
                model: section.modelData.controls.filter(c => c.kind === "knob")

                Item {
                    id: knobSlot
                    required property var modelData
                    readonly property bool selector: Kalliope.detents(modelData.key) > 1
                    readonly property real size: 52
                    x: (modelData.col + 0.5) * panel.column - size / 2
                    y: 92 + (modelData.row ?? 0) * 124

                    Selector {
                        visible: knobSlot.selector
                        key: knobSlot.modelData.key
                        size: knobSlot.size
                        ring: !(knobSlot.modelData.valueBelow ?? false)
                    }
                    ParamKnob {
                        visible: !knobSlot.selector
                        key: knobSlot.modelData.key
                        width: knobSlot.size
                        height: knobSlot.size
                    }
                }
            }
            Repeater {
                model: section.modelData.controls.filter(c => c.kind === "switch")

                Item {
                    id: switchSlot
                    required property var modelData
                    readonly property real cx: (modelData.col + 0.5) * panel.column
                    readonly property real switchY: (modelData.row ?? 0) === 0 ? 96 : 196

                    ParamSwitch {
                        id: control
                        key: switchSlot.modelData.key
                        x: switchSlot.cx - width / 2
                        y: switchSlot.switchY
                    }
                    ValueText {
                        x: switchSlot.cx - width / 2
                        y: switchSlot.switchY + control.height + 4
                        width: panel.column + 6
                        text: control.displayText
                    }
                }
            }
            Repeater {
                model: section.modelData.controls

                Text {
                    required property var modelData
                    readonly property real cx: (modelData.col + 0.5) * panel.column
                    x: cx - width / 2
                    y: 40 + (modelData.row ?? 0) * 130
                    text: modelData.label
                    color: panel.silk
                    font.family: Theme.labelFont
                    font.pixelSize: 12
                    font.bold: true
                    lineHeight: 0.9
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }
    }

    // ---- Function deck: play mode (note / arpeggio / sequencer), key mode, patches or steps, effects

    readonly property bool seqMode: Kalliope.revision >= 0 && Kalliope.value("play_mode") > 0.75
    property bool blink: false
    Timer {
        interval: 250
        repeat: true
        running: Patches.writing || Patches.pendingBank >= 0 || Sequencer.cursor >= 0
        onTriggered: panel.blink = !panel.blink
        onRunningChanged: panel.blink = false
    }

    Rectangle {
        x: panel.margin
        y: 560
        width: panel.width - 2 * panel.margin
        height: 200
        radius: 4
        color: panel.deck
    }
    Repeater {
        model: [
            { title: "ARPEGGIO  ·  SEQUENCER", x: 40, w: 430 },
            { title: "KEY MODE", x: 485, w: 155 },
            { title: "PATCH", x: 655, w: 205 },
            { title: "BANK  ·  PATCH NUMBER", x: 875, w: 365 },
            { title: "FX", x: 1255, w: 321 }
        ]

        Rectangle {
            required property var modelData
            x: modelData.x
            y: 572
            width: modelData.w
            height: 22
            color: panel.header

            Text {
                anchors.centerIn: parent
                // The step rows' header changes with the mode.
                text: parent.modelData.x === 875 && panel.seqMode ? "SEQUENCER STEPS  1 – 16" : parent.modelData.title
                color: "#111111"
                font.family: Theme.labelFont
                font.pixelSize: 13
                font.bold: true
            }
        }
    }

    // A function button bound to a switch parameter (on/off).
    component ToggleButton: PanelButton {
        id: toggle
        required property string key
        text: Kalliope.label(key)
        hasLed: true
        ledOn: Kalliope.revision >= 0 && Kalliope.value(key) > 0.5
        onClicked: Kalliope.setValue(key, ledOn ? 0 : 1)
        width: 56
        height: 40

        LearnOverlay {
            anchors.fill: parent
            paramKey: toggle.key
            round: false
        }
    }

    // A button that fires one of Kalliope's learnable actions.
    component ActionButton: PanelButton {
        id: actionButton
        required property string action
        width: 56
        height: 36

        LearnOverlay {
            anchors.fill: parent
            paramKey: actionButton.action
            round: false
        }
    }

    // A small knob with its label above and its value under it.
    component DeckKnob: Item {
        id: deckKnob
        required property string key
        required property string label
        required property real cx
        property real knobY: 682
        property real size: 36
        DeckLabel { x: deckKnob.cx - width / 2; y: deckKnob.knobY - 18; text: deckKnob.label }
        ParamKnob {
            id: deckControl
            key: deckKnob.key
            x: deckKnob.cx - width / 2
            y: deckKnob.knobY
            width: deckKnob.size
            height: deckKnob.size
        }
        ValueText {
            x: deckKnob.cx - width / 2
            y: deckKnob.knobY + deckKnob.size + 2
            width: 58
            text: deckControl.displayText
        }
    }

    // Play mode: NOTE / ARP / SEQ (one lit), HOLD, START, CLEAR
    Repeater {
        model: [{ label: "NOTE", tint: "#D8D8D4" }, { label: "ARP", tint: "#E2701F" }, { label: "SEQ", tint: "#E8C12A" }]

        PanelButton {
            id: playButton
            required property int index
            required property var modelData
            readonly property real position: index / 2
            readonly property bool lit: Kalliope.revision >= 0 && Math.abs(Kalliope.value("play_mode") - position) < 0.01
            x: 52 + index * 62
            y: 614
            width: 56
            height: 40
            text: "Play mode " + modelData.label
            tint: lit ? Qt.lighter(modelData.tint, 1.25) : Qt.darker(modelData.tint, 1.6)
            onClicked: Kalliope.setValue("play_mode", position)

            DeckLabel { x: playButton.width / 2 - width / 2; y: -18; text: playButton.modelData.label }
        }
    }
    Item {
        LearnOverlay { // the play mode buttons learn as one selector
            x: 48
            y: 610
            width: 186
            height: 48
            paramKey: "play_mode"
            round: false
        }
    }
    DeckLabel { x: 248; y: 596; width: 56; text: "HOLD" }
    ToggleButton { key: "arp_hold"; x: 248; y: 614; tint: "#E8C12A" }
    DeckLabel { x: 330; y: 596; width: 56; text: "START"; visible: Kalliope.ownsTransport }
    ActionButton {
        action: "start"
        visible: Kalliope.ownsTransport
        x: 330
        y: 614
        height: 40
        text: "Start or stop"
        tint: "#F0F0EE"
        hasLed: true
        ledOn: Kalliope.playing
        onClicked: Kalliope.playing = !Kalliope.playing
    }
    DeckLabel { x: 400; y: 596; width: 56; text: "CLEAR"; visible: panel.seqMode }
    PanelButton {
        visible: panel.seqMode
        x: 400
        y: 614
        width: 56
        height: 40
        dark: true
        text: "Clear the sequencer pattern"
        onClicked: Sequencer.clear()
    }

    // Arpeggio and generator knobs; generator and step-entry buttons
    DeckKnob { key: "arp_mode"; label: "MODE"; cx: 70 }
    DeckKnob { key: "arp_range"; label: "RANGE"; cx: 124 }
    DeckKnob { key: "arp_rate"; label: "RATE"; cx: 178 }
    DeckKnob { key: "gen_density"; label: "DENSITY"; cx: 236 }
    Repeater {
        model: [{ label: "GEN", action: "generate", x: 282, tint: "#E2701F" },
                { label: "REST", action: "rest", x: 344, tint: "#9A9B9E" },
                { label: "TIE", action: "tie", x: 406, tint: "#9A9B9E" }]

        ActionButton {
            id: seqButton
            required property var modelData
            action: modelData.action
            x: modelData.x
            y: 686
            text: modelData.label
            tint: modelData.tint
            enabled: panel.seqMode
            opacity: enabled ? 1.0 : 0.35
            onClicked: {
                if (modelData.action === "generate")
                    Sequencer.generate()
                else if (modelData.action === "rest")
                    Sequencer.rest()
                else
                    Sequencer.tie()
            }

            DeckLabel { x: seqButton.width / 2 - width / 2; y: -18; text: seqButton.modelData.label }
        }
    }

    // Key mode: three buttons, one lit; velocity and bend range under them
    Repeater {
        model: ["POLY", "UNISON", "SOLO"]

        PanelButton {
            id: modeButton
            required property int index
            required property string modelData
            readonly property real position: index / 2
            x: 494 + index * 50
            y: 614
            width: 44
            height: 40
            text: "Key mode " + modelData
            tint: "#3F7FD0"
            hasLed: true
            ledOn: Kalliope.revision >= 0 && Math.abs(Kalliope.value("key_mode") - position) < 0.01
            onClicked: Kalliope.setValue("key_mode", position)

            DeckLabel {
                x: modeButton.width / 2 - width / 2
                y: -18
                text: modeButton.modelData
                font.pixelSize: 10
            }
        }
    }
    Item {
        LearnOverlay { // the key mode buttons learn as one selector
            x: 490
            y: 610
            width: 148
            height: 48
            paramKey: "key_mode"
            round: false
        }
    }
    DeckKnob { key: "velocity"; label: "VELOCITY"; cx: 528 }
    DeckKnob { key: "bend_range"; label: "BEND"; cx: 604 }

    // Patches (docs/PLAY.md#patches): display, arrows, WRITE, MANUAL. In SEQ
    // mode the display shows the sequencer pattern (P1..P8) and the arrows pick it.
    Rectangle { // two-digit display: bank and patch with a dot while edited, or the pattern
        x: 662
        y: 606
        width: 84
        height: 62
        radius: 4
        color: "#120606"
        border.color: "#3A3B3E"
        border.width: 2

        Accessible.role: Accessible.StaticText
        Accessible.name: panel.seqMode ? "Sequencer pattern " + (Sequencer.patternIndex + 1)
                                       : "Patch " + patchDigits.text + ", " + Patches.name

        Text {
            id: patchDigits
            anchors.centerIn: parent
            text: panel.seqMode ? "P" + (Sequencer.patternIndex + 1)
                : Patches.manual ? "--"
                : (Patches.pendingBank >= 0 ? (panel.blink ? " " : String(Patches.pendingBank + 1)) : String(Patches.bank + 1))
                  + String(Patches.number + 1) + (Patches.edited ? "." : "")
            color: "#FF3A26"
            font.family: "monospace"
            font.pixelSize: 38
            font.bold: true
        }
    }
    ValueText {
        x: 658
        y: 674
        width: 92
        elide: Text.ElideRight
        wrapMode: Text.NoWrap
        text: !panel.seqMode ? Patches.name
            : Sequencer.cursor >= 0 ? "Step " + (Sequencer.cursor + 1) + ": "
                                      + (Sequencer.revision >= 0 ? Sequencer.stepText(Sequencer.cursor) : "")
            : Sequencer.length + " steps"
        color: panel.silk
        font.pixelSize: 11
    }
    Column {
        x: 750
        y: 606
        spacing: 4
        PanelButton {
            width: 24
            height: 29
            faceText: "▲"
            text: panel.seqMode ? "Next pattern" : "Next patch"
            onClicked: panel.seqMode ? Sequencer.patternIndex = (Sequencer.patternIndex + 1) % 8 : Patches.step(1)
        }
        PanelButton {
            width: 24
            height: 29
            faceText: "▼"
            text: panel.seqMode ? "Previous pattern" : "Previous patch"
            onClicked: panel.seqMode ? Sequencer.patternIndex = (Sequencer.patternIndex + 7) % 8 : Patches.step(-1)
        }
    }
    ActionButton {
        action: "write"
        visible: !panel.seqMode
        x: 780
        y: 606
        width: 72
        height: 34
        faceText: "WRITE"
        text: "Write: then press a patch number to store the panel there"
        tint: Patches.writing && panel.blink ? "#FF6A55" : "#B8302A"
        onClicked: Patches.toggleWrite()
    }
    PanelButton {
        visible: !panel.seqMode
        x: 780
        y: 648
        width: 72
        height: 34
        faceText: "MANUAL"
        text: "Manual: the panel as set, not tied to a patch"
        tint: Patches.manual ? "#5FCB52" : "#3C8F35"
        onClicked: Patches.pressManual()
    }
    Text { // WRITE armed
        visible: Patches.writing && !panel.seqMode
        x: 662
        y: 692
        text: "WRITE: press a patch number"
        color: "#FF3A26"
        font.family: Theme.labelFont
        font.pixelSize: 11
        font.bold: true
    }
    Text { // recording
        visible: panel.seqMode && Sequencer.cursor >= 0
        x: 662
        y: 692
        width: 190
        wrapMode: Text.WordWrap
        text: "REC: play a note or chord, or REST / TIE. Shift-click a step: last step"
        color: "#FF3A26"
        font.family: Theme.labelFont
        font.pixelSize: 10
        font.bold: true
    }

    // Banks and patch numbers, or (SEQ) the sequencer's 16 steps
    DeckLabel { x: 882; y: 620; text: panel.seqMode ? "1–8" : "BANK" }
    DeckLabel { x: 882; y: 676; text: panel.seqMode ? "9–16" : "PATCH" }
    Repeater {
        model: 8

        ActionButton {
            id: bankButton
            required property int index
            action: "bank" + (index + 1)
            visible: !panel.seqMode
            x: 930 + index * 39
            y: 610
            width: 35
            height: 34
            faceText: String(index + 1)
            text: "Bank " + (index + 1)
            // Lit (bright) = the current bank; blinking = chosen, waiting for a patch number.
            readonly property bool lit: Patches.pendingBank === index ? panel.blink
                                      : Patches.pendingBank < 0 && !Patches.manual && Patches.bank === index
            tint: lit ? "#FFFFFF" : "#9A9B9E"
            onClicked: Patches.pressBank(index)
        }
    }
    Repeater {
        model: 8

        ActionButton {
            id: patchButton
            required property int index
            action: "patch" + (index + 1)
            visible: !panel.seqMode
            x: 930 + index * 39
            y: 666
            width: 35
            height: 34
            faceText: String(index + 1)
            text: "Patch " + (index + 1)
            readonly property bool lit: !Patches.manual && Patches.number === index && Patches.pendingBank < 0
            tint: lit ? "#FFE45C" : "#A88A1E"
            onClicked: Patches.pressPatch(index)
        }
    }
    Repeater { // the sequencer's steps
        model: 16

        Rectangle {
            id: stepButton
            required property int index
            readonly property int state: Sequencer.revision >= 0 ? Sequencer.stepState(index) : 0
            readonly property bool inPattern: index < Sequencer.length
            readonly property bool playing: Sequencer.playingStep === index
            readonly property bool recording: Sequencer.cursor === index
            visible: panel.seqMode
            x: 930 + (index % 8) * 39
            y: index < 8 ? 610 : 666
            width: 35
            height: 34
            radius: 4
            border.color: "#0E0E0F"
            opacity: inPattern ? 1.0 : 0.3
            color: recording ? (panel.blink ? "#FF3A26" : "#6A1A12")
                 : playing ? "#FFFFFF"
                 : state === 1 ? "#FFE45C" : state === 2 ? "#C8892A" : "#4A4B4E"

            Accessible.role: Accessible.Button
            Accessible.name: "Step " + (index + 1) + ": " + (Sequencer.revision >= 0 ? Sequencer.stepText(index) : "")

            Text {
                anchors.centerIn: parent
                text: stepButton.index + 1
                color: stepButton.state === 0 && !stepButton.playing && !stepButton.recording ? "#C8C9CC" : "#111"
                font.family: Theme.labelFont
                font.pixelSize: 13
                font.bold: true
            }
            MouseArea {
                anchors.fill: parent
                onClicked: mouse => Sequencer.pressStep(stepButton.index, (mouse.modifiers & Qt.ShiftModifier) !== 0)
            }
        }
    }

    // Effects: chorus (Off / I / II) and the delay
    DeckLabel { x: 1270; y: 604; width: 70; text: "CHORUS" }
    PanelButton {
        id: chorusButton
        readonly property int position: Kalliope.revision >= 0 ? Math.round(Kalliope.value("chorus") * 2) : 0
        x: 1276
        y: 624
        width: 58
        height: 46
        tint: "#E2701F"
        text: "Chorus"
        hasLed: true
        ledOn: position > 0
        onClicked: Kalliope.setValue("chorus", ((position + 1) % 3) / 2)

        LearnOverlay {
            anchors.fill: parent
            paramKey: "chorus"
            round: false
        }
    }
    ValueText { x: 1270; y: 676; width: 70; text: Kalliope.displayText("chorus", chorusButton.position / 2) }
    Repeater {
        model: [{ key: "delay_level", label: "DELAY", cx: 1390 }, { key: "delay_time", label: "TIME", cx: 1458 },
                { key: "delay_fb", label: "FEEDBACK", cx: 1526 }]

        Item {
            id: fxKnob
            required property var modelData
            DeckLabel { x: fxKnob.modelData.cx - width / 2; y: 604; text: fxKnob.modelData.label }
            ParamKnob {
                id: fxControl
                key: fxKnob.modelData.key
                x: fxKnob.modelData.cx - width / 2
                y: 624
                width: 46
                height: 46
            }
            ValueText {
                x: fxKnob.modelData.cx - width / 2
                y: 676
                width: 60
                text: fxControl.displayText
            }
        }
    }

    // ---- Keyboard deck: volume, tempo, octave; the keyboard; utilities

    Rectangle {
        x: panel.margin
        y: 780
        width: panel.width - 2 * panel.margin
        height: panel.height - y - panel.margin
        radius: 4
        color: "#0F0F10"
    }

    DeckLabel { x: 100 - width / 2; y: 800; text: "VOLUME" }
    ParamKnob { key: "volume"; x: 60; y: 822; width: 80; height: 80 }
    DeckLabel { x: 225 - width / 2; y: 800; text: "TEMPO"; visible: Kalliope.ownsTransport }
    ParamKnob { key: "tempo"; x: 195; y: 828; width: 60; height: 60; visible: Kalliope.ownsTransport }

    // Octave (Z / X): shifts the keys, the LX49's and the on-screen keyboard's, -3..+3
    readonly property int octave: Kalliope.revision >= 0 ? Math.round(Kalliope.value("octave") * 6) - 3 : 0
    function shiftOctave(delta) {
        Kalliope.setValue("octave", (Math.max(-3, Math.min(3, octave + delta)) + 3) / 6)
    }
    DeckLabel { x: 160 - width / 2; y: 940; text: "OCTAVE" }
    ActionButton {
        action: "octave_down"
        x: 62
        y: 962
        width: 52
        height: 40
        faceText: "◂"
        text: "Octave down"
        onClicked: panel.shiftOctave(-1)
    }
    ActionButton {
        action: "octave_up"
        x: 206
        y: 962
        width: 52
        height: 40
        faceText: "▸"
        text: "Octave up"
        onClicked: panel.shiftOctave(1)
    }
    Text {
        id: octaveText
        x: 160 - width / 2
        y: 960
        text: "C" + (3 + panel.octave)
        color: "#FF3A26"
        font.family: "monospace"
        font.pixelSize: 22
        font.bold: true

        Accessible.role: Accessible.StaticText
        Accessible.name: "Octave " + Kalliope.displayText("octave", Kalliope.value("octave"))
    }
    ValueText {
        x: 160 - width / 2
        y: 988
        width: 60
        text: panel.octave === 0 ? "" : (panel.octave > 0 ? "+" : "") + panel.octave + " oct"
    }
    Item {
        LearnOverlay { // a knob or slider can drive Octave as a selector
            x: 124
            y: 956
            width: 72
            height: 48
            paramKey: "octave"
            round: false
        }
    }

    PianoKeyboard {
        instrument: Kalliope
        x: 320
        y: 806
        width: 960
        height: 250
        baseNote: 48 + 12 * panel.octave
    }

    // Utilities: clear all MIDI mappings, audio (click to reconnect), MIDI Learn
    Repeater {
        model: [{ name: "UNMAP", cx: 1360 }, { name: "AUDIO", cx: 1440 }, { name: "LEARN", cx: 1520 }]

        Item {
            id: utility
            required property var modelData
            readonly property string name: modelData.name

            DeckLabel { x: utility.modelData.cx - width / 2; y: 800; text: utility.name }
            PanelButton {
                x: utility.modelData.cx - width / 2
                y: 822
                width: 60
                height: 36
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
    Text { // audio status: rate, buffer, xruns, MIDI, or AUDIO OFFLINE
        x: 1312
        y: 876
        width: 250
        elide: Text.ElideRight
        text: Host.audioStatus
        color: Host.audioConnected ? panel.dim : Theme.ledOn
        font.family: Theme.labelFont
        font.pixelSize: 11
    }
    Rectangle { // output level meter
        x: 1312
        y: 900
        width: 250
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

    // Keyboard (docs/UI.md): Space run/stop, Z / X octave, L learn, Esc leaves learn.
    Keys.onSpacePressed: Kalliope.playing = !Kalliope.playing // no-op in the rack
    Keys.onPressed: event => {
        if (event.modifiers !== Qt.NoModifier && event.modifiers !== Qt.KeypadModifier)
            return
        switch (event.key) {
        case Qt.Key_Z: panel.shiftOctave(-1); break
        case Qt.Key_X: panel.shiftOctave(1); break
        case Qt.Key_L: MidiLearn.active = !MidiLearn.active; break
        case Qt.Key_Escape:
            if (!MidiLearn.active)
                return
            MidiLearn.active = false
            break
        default:
            return
        }
        event.accepted = true
    }
}

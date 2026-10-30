pragma ComponentBehavior: Bound

import QtQuick
import Krate

// Kobalt's panel at design size 1600 x 1100: the sound sections (sliders,
// switches), the function deck (play mode, bender, patches, effects) and the
// keyboard deck (volume, tempo, octave, the on-screen keyboard, utilities).
// Every control is bound to Kobalt by key.
Item {
    id: panel

    width: 1600
    height: 1100

    // Panel-specific colors; shared ones come from Theme.
    readonly property color chassis: "#1C3B7C"
    readonly property color chassisEdge: "#0A1834"
    readonly property color deck: "#152E63"
    readonly property color header: "#DCE4F4"
    readonly property color headerText: "#0E1D40"
    readonly property color silk: "#F1F4FA"
    readonly property color dim: "#A9B9DA"
    readonly property color accent: "#F2B632"

    // Sound sections, left to right. Width in columns; each control sits at a
    // column inside its section. kind: slider, knob (selector), switch.
    readonly property real margin: 24
    readonly property real columns: 24
    readonly property real column: (width - 2 * margin) / columns
    readonly property real sectionTop: 250
    readonly property real sectionBottom: 540

    readonly property var sections: [
        { title: "MODULATOR", columns: 2, controls: [
            { key: "lfo_rate", label: "RATE", kind: "slider", col: 0 },
            { key: "lfo_wave", label: "WAVE", kind: "knob", col: 1 } ] },
        { title: "VCO", columns: 4, controls: [
            { key: "vco_mod", label: "MOD", kind: "slider", col: 0 },
            { key: "vco_range", label: "RANGE", kind: "knob", col: 1 },
            { key: "pw", label: "PULSE\nWIDTH", kind: "slider", col: 2 },
            { key: "pwm_src", label: "PWM\nSOURCE", kind: "switch", col: 3 } ] },
        { title: "SOURCE MIXER", columns: 5, controls: [
            { key: "pulse_level", label: "PULSE", kind: "slider", col: 0 },
            { key: "saw_level", label: "SAW", kind: "slider", col: 1 },
            { key: "sub_level", label: "SUB", kind: "slider", col: 2 },
            { key: "sub_type", label: "SUB\nTYPE", kind: "switch", col: 3 },
            { key: "noise_level", label: "NOISE", kind: "slider", col: 4 } ] },
        { title: "VCF", columns: 5, controls: [
            { key: "cutoff", label: "FREQ", kind: "slider", col: 0 },
            { key: "res", label: "RES", kind: "slider", col: 1 },
            { key: "vcf_env", label: "ENV", kind: "slider", col: 2 },
            { key: "vcf_mod", label: "MOD", kind: "slider", col: 3 },
            { key: "key_follow", label: "KYBD", kind: "slider", col: 4 } ] },
        { title: "VCA", columns: 1, controls: [
            { key: "vca_mode", label: "MODE", kind: "switch", col: 0 } ] },
        { title: "ENV", columns: 5, controls: [
            { key: "env_trig", label: "TRIGGER", kind: "switch", col: 0 },
            { key: "env_a", label: "A", kind: "slider", col: 1 },
            { key: "env_d", label: "D", kind: "slider", col: 2 },
            { key: "env_s", label: "S", kind: "slider", col: 3 },
            { key: "env_r", label: "R", kind: "slider", col: 4 } ] },
        { title: "PORTAMENTO", columns: 2, controls: [
            { key: "porta_time", label: "TIME", kind: "slider", col: 0 },
            { key: "porta_mode", label: "MODE", kind: "switch", col: 1 } ] }
    ]

    // ---- Controls bound to Kobalt's parameters by key; all MIDI-learnable.

    component ParamSlider: PanelSlider {
        id: paramSlider
        required property string key
        name: Kobalt.label(key)
        value: Kobalt.revision >= 0 ? Kobalt.value(key) : 0
        defaultValue: Kobalt.defaultValue(key)
        displayText: Kobalt.displayText(key, value)
        onMoved: v => Kobalt.setValue(key, v)

        LearnOverlay {
            anchors.fill: parent
            paramKey: paramSlider.key
            round: false
        }
    }

    component ParamSwitch: PanelSwitch {
        id: paramSwitch
        required property string key
        name: Kobalt.label(key)
        positions: Kobalt.detents(key)
        value: Kobalt.revision >= 0 ? Kobalt.value(key) : 0
        displayText: Kobalt.displayText(key, value)
        onMoved: v => Kobalt.setValue(key, v)

        LearnOverlay {
            anchors.fill: parent
            paramKey: paramSwitch.key
            round: false
        }
    }

    // Dark knob, white pointer; a selector (detents) or continuous.
    component ParamKnob: Knob {
        id: paramKnob
        required property string key
        name: Kobalt.label(key)
        detents: Kobalt.detents(key)
        value: Kobalt.revision >= 0 ? Kobalt.value(key) : 0
        defaultValue: Kobalt.defaultValue(key)
        displayText: Kobalt.displayText(key, value)
        onMoved: v => Kobalt.setValue(key, v)
        capColor: "#26292F"
        capShadeColor: "#0E1013"
        capBorderColor: "#05060A"
        pointerColor: "#F0F0F0"
        Accessible.description: displayText

        LearnOverlay {
            anchors.fill: parent
            paramKey: paramKnob.key
        }
    }

    // A selector with its positions written around it.
    component Selector: Item {
        id: selector
        required property string key
        property real size: 56
        readonly property int count: Kobalt.detents(key)

        width: size
        height: size

        ParamKnob {
            key: selector.key
            anchors.fill: parent
        }
        Repeater {
            model: selector.count

            Text {
                required property int index
                readonly property real angle: (-135 + 270 * index / (selector.count - 1)) * Math.PI / 180
                readonly property real radius: selector.size / 2 + 14
                x: selector.size / 2 + radius * Math.sin(angle) - width / 2
                y: selector.size / 2 - radius * Math.cos(angle) - height / 2
                text: Kobalt.displayText(selector.key, index / (selector.count - 1))
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
        color: panel.accent
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
        text: "KOBALT"
        color: panel.silk
        font.family: Theme.labelFont
        font.pixelSize: 84
        font.weight: Font.Black
        font.letterSpacing: 10
    }
    Text {
        anchors.right: wordmark.right
        y: 172
        text: "MONOPHONIC SYNTHESIZER"
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
                radius: 3
                color: panel.header
                Text {
                    anchors.centerIn: parent
                    text: section.modelData.title
                    color: panel.headerText
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
                color: "#2E4F95"
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

                Selector {
                    required property var modelData
                    key: modelData.key
                    size: 46
                    x: (modelData.col + 0.5) * panel.column - size / 2
                    y: 112
                }
            }
            Repeater {
                model: section.modelData.controls.filter(c => c.kind === "switch")

                Item {
                    id: switchSlot
                    required property var modelData
                    readonly property real cx: (modelData.col + 0.5) * panel.column

                    ParamSwitch {
                        id: control
                        key: switchSlot.modelData.key
                        x: switchSlot.cx - width / 2
                        y: 96
                    }
                    ValueText {
                        x: switchSlot.cx - width / 2
                        y: 96 + control.height + 4
                        width: panel.column + 4
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
                    y: 40
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

    // ---- Function deck: play mode, bender, patches, effects

    property bool blink: false
    Timer {
        interval: 250
        repeat: true
        running: Patches.writing || Patches.pendingBank >= 0
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
            { title: "BENDER", x: 485, w: 155 },
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
            radius: 3
            color: panel.header

            Text {
                anchors.centerIn: parent
                text: parent.modelData.title
                color: panel.headerText
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
        text: Kobalt.label(key)
        hasLed: true
        ledOn: Kobalt.revision >= 0 && Kobalt.value(key) > 0.5
        onClicked: Kobalt.setValue(key, ledOn ? 0 : 1)
        width: 56
        height: 40

        LearnOverlay {
            anchors.fill: parent
            paramKey: toggle.key
            round: false
        }
    }

    // A button that fires one of Kobalt's learnable actions.
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
            width: 62
            text: deckControl.displayText
        }
    }

    // Play mode: NOTE / ARP / SEQ (one lit), HOLD, START
    Repeater {
        model: [{ label: "NOTE", tint: "#D8DCE4" }, { label: "ARP", tint: "#F2B632" }, { label: "SEQ", tint: "#5FB7E8" }]

        PanelButton {
            id: playButton
            required property int index
            required property var modelData
            readonly property real position: index / 2
            readonly property bool lit: Kobalt.revision >= 0 && Math.abs(Kobalt.value("play_mode") - position) < 0.01
            x: 52 + index * 62
            y: 614
            width: 56
            height: 40
            text: "Play mode " + modelData.label
            tint: lit ? Qt.lighter(modelData.tint, 1.25) : Qt.darker(modelData.tint, 1.6)
            onClicked: Kobalt.setValue("play_mode", position)

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
    ToggleButton { key: "arp_hold"; x: 248; y: 614; tint: "#F2B632" }
    DeckLabel { x: 330; y: 596; width: 56; text: "START"; visible: Kobalt.canRun }
    ActionButton {
        action: "start"
        visible: Kobalt.canRun
        x: 330
        y: 614
        height: 40
        text: "Start or stop"
        tint: "#F0F0EE"
        hasLed: true
        ledOn: Kobalt.playing
        onClicked: Kobalt.playing = !Kobalt.playing
    }
    DeckKnob { key: "arp_mode"; label: "MODE"; cx: 70 }
    DeckKnob { key: "arp_range"; label: "RANGE"; cx: 130 }
    DeckKnob { key: "arp_rate"; label: "RATE"; cx: 190 }

    // Bender: how far the bender moves the pitch and the cutoff, and the lever's vibrato
    DeckKnob { key: "bend_vco"; label: "VCO"; cx: 515; knobY: 620 }
    DeckKnob { key: "bend_vcf"; label: "VCF"; cx: 563; knobY: 620 }
    DeckKnob { key: "lever_mod"; label: "LEVER"; cx: 611; knobY: 620 }
    Text {
        x: 495
        y: 690
        width: 135
        wrapMode: Text.WordWrap
        horizontalAlignment: Text.AlignHCenter
        text: "Pitch bend moves VCO and VCF; the mod wheel brings in the LFO by LEVER"
        color: panel.dim
        font.family: Theme.labelFont
        font.pixelSize: 10
    }

    // Patches: display, arrows, WRITE, MANUAL
    Rectangle { // two-digit display: bank and patch, with a dot while edited
        x: 662
        y: 606
        width: 84
        height: 62
        radius: 4
        color: "#06101F"
        border.color: "#2E4F95"
        border.width: 2

        Accessible.role: Accessible.StaticText
        Accessible.name: "Patch " + patchDigits.text + ", " + Patches.name

        Text {
            id: patchDigits
            anchors.centerIn: parent
            text: Patches.manual ? "--"
                : (Patches.pendingBank >= 0 ? (panel.blink ? " " : String(Patches.pendingBank + 1)) : String(Patches.bank + 1))
                  + String(Patches.number + 1) + (Patches.edited ? "." : "")
            color: "#5FD0FF"
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
        text: Patches.name
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
            text: "Next patch"
            onClicked: Patches.step(1)
        }
        PanelButton {
            width: 24
            height: 29
            faceText: "▼"
            text: "Previous patch"
            onClicked: Patches.step(-1)
        }
    }
    ActionButton {
        action: "write"
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
        visible: Patches.writing
        x: 662
        y: 692
        text: "WRITE: press a patch number"
        color: "#FF6A55"
        font.family: Theme.labelFont
        font.pixelSize: 11
        font.bold: true
    }

    // Banks and patch numbers
    DeckLabel { x: 882; y: 620; text: "BANK" }
    DeckLabel { x: 882; y: 676; text: "PATCH" }
    Repeater {
        model: 8

        ActionButton {
            required property int index
            action: "bank" + (index + 1)
            x: 930 + index * 39
            y: 610
            width: 35
            height: 34
            faceText: String(index + 1)
            text: "Bank " + (index + 1)
            // Lit (bright) = the current bank; blinking = chosen, waiting for a patch number.
            readonly property bool lit: Patches.pendingBank === index ? panel.blink
                                      : Patches.pendingBank < 0 && !Patches.manual && Patches.bank === index
            tint: lit ? "#FFFFFF" : "#8E9AB4"
            onClicked: Patches.pressBank(index)
        }
    }
    Repeater {
        model: 8

        ActionButton {
            required property int index
            action: "patch" + (index + 1)
            x: 930 + index * 39
            y: 666
            width: 35
            height: 34
            faceText: String(index + 1)
            text: "Patch " + (index + 1)
            readonly property bool lit: !Patches.manual && Patches.number === index && Patches.pendingBank < 0
            tint: lit ? "#5FD0FF" : "#2C6E9A"
            onClicked: Patches.pressPatch(index)
        }
    }

    // Effects: the delay, and the reverb under it
    DeckKnob { key: "delay_level"; label: "DELAY"; cx: 1300; knobY: 614 }
    DeckKnob { key: "delay_time"; label: "TIME"; cx: 1380; knobY: 614 }
    DeckKnob { key: "delay_fb"; label: "FEEDBACK"; cx: 1460; knobY: 614 }
    DeckKnob { key: "reverb_level"; label: "REVERB"; cx: 1300; knobY: 690 }
    DeckKnob { key: "reverb_size"; label: "SIZE"; cx: 1380; knobY: 690 }

    // ---- Keyboard deck: volume, tempo, octave; the keyboard; utilities

    Rectangle {
        x: panel.margin
        y: 780
        width: panel.width - 2 * panel.margin
        height: panel.height - y - panel.margin
        radius: 4
        color: "#0B1630"
    }

    DeckLabel { x: 100 - width / 2; y: 800; text: "VOLUME" }
    ParamKnob { key: "volume"; x: 60; y: 822; width: 80; height: 80 }
    DeckLabel { x: 225 - width / 2; y: 800; text: "TEMPO"; visible: Kobalt.ownsTransport }
    ParamKnob { key: "tempo"; x: 195; y: 828; width: 60; height: 60; visible: Kobalt.ownsTransport }

    // Octave (Z / X): shifts the keys, the controller's and the on-screen keyboard's, -3..+3
    readonly property int octave: Kobalt.revision >= 0 ? Math.round(Kobalt.value("octave") * 6) - 3 : 0
    function shiftOctave(delta) {
        Kobalt.setValue("octave", (Math.max(-3, Math.min(3, octave + delta)) + 3) / 6)
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
    Text { // the octave you're playing in, as on Krimson: your keyboard's (as the keys show it) plus Kobalt's own
        readonly property int total: panel.octave + keys.controllerOctave
        x: 160 - width / 2
        y: 960
        text: (total > 0 ? "+" : "") + total
        color: "#5FD0FF"
        font.family: "monospace"
        font.pixelSize: 22
        font.bold: true

        Accessible.role: Accessible.StaticText
        Accessible.name: "Octave " + Kobalt.displayText("octave", Kobalt.value("octave"))
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

    ControllerPianoKeyboard { // key for key with your MIDI keyboard (the rack's KEYS setting)
        id: keys
        instrument: Kobalt
        x: 320
        y: 806
        width: 960
        height: 250
        instrumentOctave: panel.octave
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
    // Playing: velocity, analog
    Repeater {
        model: [{ key: "velocity", label: "VELOCITY", cx: 1400 }, { key: "analog", label: "ANALOG", cx: 1480 }]

        DeckKnob {
            required property var modelData
            key: modelData.key
            label: modelData.label
            cx: modelData.cx
            knobY: 950
            size: 44
        }
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

    // Keyboard: Space run/stop, Z / X octave, L learn, Esc leaves learn.
    Keys.onSpacePressed: Kobalt.playing = !Kobalt.playing // no-op in the rack
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

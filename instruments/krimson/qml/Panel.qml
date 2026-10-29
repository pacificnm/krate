pragma ComponentBehavior: Bound

import QtQuick
import Krate

// Krimson's panel at design size 1600 x 900 (docs/UI.md): a red body with a
// black control panel. Left block: master level, tempo, octave, wheel, morph,
// the program display and STORE, the slot buttons and the edited slot's setup. Then the sections, left
// to right: arpeggiator and voice, LFOs, mod envelope, oscillators, filter,
// amp envelope, effects, output. Wheels, performances and keyboard at the bottom. Every
// control edits the edited slot (Krimson.editedSlot) by key and is
// MIDI-learnable.
Item {
    id: panel

    width: 1600
    height: 900

    readonly property color body: "#B3121B"
    readonly property color bodyDark: "#8C0D14"
    readonly property color black: "#141415"
    readonly property color frame: "#3A3B3E"
    readonly property color frameFill: "#1E1F21"
    readonly property color silk: "#EDEDED"
    readonly property color dim: "#9A9B9E"
    readonly property color led: "#FF3A26"
    readonly property real sectionTop: 70

    // ---- Controls bound to Krimson's parameters by key

    // A ring knob with its label above and its value under it.
    component KKnob: Item {
        id: kk
        required property string key
        property string label: ""
        property real size: 46
        property bool bipolar: false
        width: size + 24
        height: size + 34

        PanelLabel {
            centerX: kk.width / 2
            y: 0
            text: kk.label
            inverse: true
            font.pixelSize: 10
        }
        RingKnob {
            id: kkKnob
            // MORPH lit: the knob shows and sets its morph target (starting from its value).
            readonly property real target: Krimson.morphRevision >= 0 ? Krimson.morph(kk.key) : -1
            readonly property bool morphing: Krimson.morphEdit && Krimson.morphable(kk.key)
            x: (kk.width - width) / 2
            y: 13
            width: kk.size
            height: kk.size
            bipolar: kk.bipolar
            name: Krimson.label(kk.key) + (morphing ? " morph target" : "")
            detents: Krimson.detents(kk.key)
            value: morphing && target >= 0 ? target : (Krimson.revision >= 0 ? Krimson.value(kk.key) : 0)
            morphValue: morphing ? -1 : target
            ringColor: morphing ? morphColor : "#FF4A3D"
            defaultValue: Krimson.defaultValue(kk.key)
            displayText: Krimson.displayText(kk.key, value)
            onMoved: v => morphing ? Krimson.setMorph(kk.key, v) : Krimson.setValue(kk.key, v)

            MouseArea { // MORPH lit: right-click clears this knob's target
                anchors.fill: parent
                enabled: kkKnob.morphing
                acceptedButtons: Qt.RightButton
                onClicked: Krimson.clearMorph(kk.key)
            }
            LearnOverlay {
                anchors.fill: parent
                paramKey: kk.key
            }
        }
        Text {
            x: 0
            y: kk.size + 15
            width: kk.width
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            text: kkKnob.displayText
            color: panel.dim
            font.family: Theme.labelFont
            font.pixelSize: 9
        }
    }

    // A list of choices with an LED each (wave, filter type, destinations):
    // click a choice to pick it.
    component Choices: Item {
        id: choices
        required property string key
        property string label: ""
        property real rowHeight: 15
        readonly property int count: Krimson.detents(key)
        readonly property int current: Krimson.revision >= 0 ? Math.round(Krimson.value(key) * (count - 1)) : 0
        width: 96
        height: 14 + count * rowHeight

        Accessible.role: Accessible.List
        Accessible.name: Krimson.label(key) + ": " + Krimson.displayText(key, Krimson.value(key))

        PanelLabel {
            x: 0
            y: 0
            text: choices.label
            inverse: true
            font.pixelSize: 10
            horizontalAlignment: Text.AlignLeft
        }
        Repeater {
            model: choices.count

            Item {
                id: choiceRow
                required property int index
                y: 14 + index * choices.rowHeight
                width: choices.width
                height: choices.rowHeight

                Rectangle {
                    x: 2
                    y: (choiceRow.height - height) / 2
                    width: 7
                    height: 7
                    radius: 4
                    color: choices.current === choiceRow.index ? panel.led : "#4A1612"
                }
                Text {
                    x: 14
                    anchors.verticalCenter: parent.verticalCenter
                    text: Krimson.displayText(choices.key, choiceRow.index / (choices.count - 1))
                    color: choices.current === choiceRow.index ? panel.silk : panel.dim
                    font.family: Theme.labelFont
                    font.pixelSize: 10
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: Krimson.setValue(choices.key, choiceRow.index / (choices.count - 1))
                }
            }
        }
        LearnOverlay {
            anchors.fill: parent
            paramKey: choices.key
            round: false
        }
    }

    // An on/off switch parameter as a small button: red when on.
    component Toggle: PanelButton {
        id: toggle
        required property string key
        property string label: ""
        readonly property bool on: Krimson.revision >= 0 && Krimson.value(key) > 0.5
        width: 50
        height: 26
        dark: true
        tint: on ? "#E2343A" : "transparent"
        faceText: label
        text: Krimson.label(key) + (on ? ": on" : ": off")
        onClicked: Krimson.setValue(key, on ? 0 : 1)

        LearnOverlay {
            anchors.fill: parent
            paramKey: toggle.key
            round: false
        }
    }

    // Two arrows around a value: the slot setup's steppers.
    component Stepper: Row {
        id: stepper
        property string label: ""
        property string value: ""
        signal down()
        signal up()
        spacing: 4

        Text {
            width: 42
            anchors.verticalCenter: parent.verticalCenter
            text: stepper.label
            color: panel.silk
            font.family: Theme.labelFont
            font.pixelSize: 11
            font.bold: true
        }
        PanelButton { width: 22; height: 22; dark: true; faceText: "◂"; text: stepper.label + " down"; onClicked: stepper.down() }
        Text {
            width: 60
            anchors.verticalCenter: parent.verticalCenter
            horizontalAlignment: Text.AlignHCenter
            text: stepper.value
            color: panel.led
            font.family: "monospace"
            font.pixelSize: 14
            font.bold: true
        }
        PanelButton { width: 22; height: 22; dark: true; faceText: "▸"; text: stepper.label + " up"; onClicked: stepper.up() }
    }

    // A section frame with its title bar.
    component Section: Rectangle {
        id: section
        required property string title
        y: panel.sectionTop
        height: 418
        radius: 4
        color: panel.frameFill
        border.color: panel.frame
        border.width: 2

        Rectangle {
            x: 5
            y: 5
            width: parent.width - 10
            height: 20
            radius: 3
            color: "#5A5B5E"

            Text {
                anchors.centerIn: parent
                text: section.title
                color: panel.silk
                font.family: Theme.labelFont
                font.pixelSize: 12
                font.bold: true
            }
        }
    }

    // A wheel: drag up / down. The pitch wheel springs back to the centre.
    component Wheel: Item {
        id: wheel
        property string label: ""
        property bool springs: false
        property real amount: springs ? 0.5 : 0.0 // 0..1
        signal changed(real amount)
        width: 50
        height: 240

        Accessible.role: Accessible.Slider
        Accessible.name: label

        PanelLabel { centerX: wheel.width / 2; y: 0; text: wheel.label; inverse: true; font.pixelSize: 10 }
        Rectangle {
            y: 18
            width: wheel.width
            height: wheel.height - 18
            radius: 6
            border.color: panel.frame
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#3A3B3E" }
                GradientStop { position: 0.5; color: "#5A5B5E" }
                GradientStop { position: 1.0; color: "#3A3B3E" }
            }
            Rectangle {
                x: 6
                width: parent.width - 12
                height: 8
                radius: 2
                y: (parent.height - height) * (1 - wheel.amount)
                color: panel.silk
            }
            MouseArea {
                anchors.fill: parent
                function set(y) {
                    wheel.amount = Math.max(0, Math.min(1, 1 - y / height))
                    wheel.changed(wheel.amount)
                }
                onPressed: mouse => set(mouse.y)
                onPositionChanged: mouse => set(mouse.y)
                onReleased: {
                    if (wheel.springs) {
                        wheel.amount = 0.5
                        wheel.changed(0.5)
                    }
                }
            }
        }
    }

    function noteName(n) {
        return ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"][n % 12] + (Math.floor(n / 12) - 1)
    }

    // ---- Body and branding

    Rectangle {
        anchors.fill: parent
        radius: 10
        gradient: Gradient {
            GradientStop { position: 0.0; color: panel.body }
            GradientStop { position: 1.0; color: panel.bodyDark }
        }
        border.color: "#5E070C"
        border.width: 3
    }
    Text {
        x: 32
        y: 14
        text: "Krate"
        color: panel.silk
        font.family: Theme.labelFont
        font.pixelSize: 26
        font.bold: true
        font.italic: true
    }
    Text {
        anchors.right: subtitle.left
        anchors.rightMargin: 16
        y: 10
        text: "Krimson"
        color: panel.silk
        font.family: Theme.labelFont
        font.pixelSize: 36
        font.weight: Font.Light
    }
    Text {
        id: subtitle
        anchors.right: parent.right
        anchors.rightMargin: 32
        y: 28
        text: "VIRTUAL ANALOG SYNTHESIZER"
        color: "#F3C7C9"
        font.family: Theme.labelFont
        font.pixelSize: 12
        font.letterSpacing: 2
    }

    Rectangle { // the black control panel
        x: 24
        y: 60
        width: 1552
        height: 440
        radius: 6
        color: panel.black
        border.color: "#000"
    }

    // ---- Left block

    KKnob { key: "volume"; label: "MASTER LEVEL"; size: 52; x: 34; y: panel.sectionTop }
    KKnob { key: "tempo"; label: "TEMPO"; size: 40; x: 120; y: panel.sectionTop + 6; visible: Krimson.ownsTransport }
    Item { // octave: shifts the keys (MIDI and on-screen)
        id: octave
        readonly property int shift: Krimson.revision >= 0 ? Math.round(Krimson.value("octave") * 6) - 3 : 0
        function step(delta) { Krimson.setValue("octave", (Math.max(-3, Math.min(3, shift + delta)) + 3) / 6) }
        x: 200
        y: panel.sectionTop
        width: 116
        height: 80

        PanelLabel { centerX: 58; y: 0; text: "OCTAVE"; inverse: true; font.pixelSize: 10 }
        PanelButton { // learnable as a button: octave_down
            x: 4; y: 22; width: 30; height: 26; dark: true; faceText: "◂"; text: "Octave down"; onClicked: octave.step(-1)
            LearnOverlay { anchors.fill: parent; paramKey: "octave_down"; round: false }
        }
        PanelButton { // octave_up
            x: 82; y: 22; width: 30; height: 26; dark: true; faceText: "▸"; text: "Octave up"; onClicked: octave.step(1)
            LearnOverlay { anchors.fill: parent; paramKey: "octave_up"; round: false }
        }
        Text {
            x: 58 - width / 2
            y: 24
            text: (octave.shift > 0 ? "+" : "") + octave.shift
            color: panel.led
            font.family: "monospace"
            font.pixelSize: 18
            font.bold: true
        }
        LearnOverlay { x: 36; y: 18; width: 44; height: 32; paramKey: "octave"; round: false }
    }

    Choices { key: "wheel_dest"; label: "WHEEL"; x: 40; y: panel.sectionTop + 96; width: 70 }
    // MORPH: while lit, knobs set their morph targets (blue); the wheel (or
    // velocity) then sweeps them. CLEAR removes the edited slot's targets.
    PanelButton {
        x: 116
        y: panel.sectionTop + 100
        width: 72
        height: 26
        dark: true
        tint: Krimson.morphEdit ? "#4FA3FF" : "transparent"
        faceText: "MORPH"
        text: "Morph edit" + (Krimson.morphEdit ? ": on" : "")
        onClicked: Krimson.morphEdit = !Krimson.morphEdit

        LearnOverlay {
            anchors.fill: parent
            paramKey: "morph"
            round: false
        }
    }
    PanelButton {
        x: 116
        y: panel.sectionTop + 132
        width: 72
        height: 20
        dark: true
        faceText: "CLEAR"
        text: "Clear the slot's morph targets"
        onClicked: Krimson.clearAllMorph()
    }
    // The edited slot's program: bank and number, a dot while edited; while
    // STORE is armed, the blinking destination (▲▼ choose it, STORE writes).
    property bool blink: false
    Timer {
        interval: 250
        repeat: true
        running: Programs.storing
        onTriggered: panel.blink = !panel.blink
        onRunningChanged: panel.blink = false
    }
    Item {
        id: programBox
        readonly property int shown: Programs.storing ? Programs.storeTarget : Programs.number
        x: 200
        y: panel.sectionTop + 96
        width: 116
        height: 72

        PanelLabel { x: 0; y: 0; text: "PROGRAM"; inverse: true; font.pixelSize: 10; horizontalAlignment: Text.AlignLeft }
        Rectangle {
            y: 14
            width: 84
            height: 38
            radius: 3
            color: "#120606"
            border.color: Programs.storing ? panel.led : panel.frame

            Accessible.role: Accessible.StaticText
            Accessible.name: "Program " + programDigits.text + ", " + Programs.name

            Text {
                id: programDigits
                anchors.centerIn: parent
                text: Programs.storing && panel.blink ? ""
                    : (Math.floor(programBox.shown / 8) + 1) + "" + (programBox.shown % 8 + 1)
                      + (Programs.edited && !Programs.storing ? "." : "")
                color: panel.led
                font.family: "monospace"
                font.pixelSize: 26
                font.bold: true
            }
        }
        Column {
            x: 88
            y: 14
            spacing: 2
            PanelButton { width: 26; height: 18; dark: true; faceText: "▲"; text: "Next program"; onClicked: Programs.step(1) }
            PanelButton { width: 26; height: 18; dark: true; faceText: "▼"; text: "Previous program"; onClicked: Programs.step(-1) }
        }
        Text {
            y: 56
            width: 116
            elide: Text.ElideRight
            text: Programs.storing ? "STORE to " + Programs.programName(Programs.storeTarget) : Programs.name
            color: Programs.storing ? panel.led : panel.dim
            font.family: Theme.labelFont
            font.pixelSize: 10
        }
    }
    PanelButton {
        x: 116
        y: panel.sectionTop + 156
        width: 72
        height: 18
        dark: true
        tint: Programs.storing ? "#E2343A" : "transparent"
        faceText: "STORE"
        text: Programs.storing ? "Store: press again to write" : "Store the slot's sound"
        onClicked: Programs.pressStore()

        LearnOverlay {
            anchors.fill: parent
            paramKey: "store"
            round: false
        }
    }

    PanelLabel { x: 40; y: panel.sectionTop + 172; text: "SLOT"; inverse: true; font.pixelSize: 11; horizontalAlignment: Text.AlignLeft }
    Repeater {
        model: ["A", "B", "C", "D"]

        Rectangle {
            id: slotButton
            required property int index
            required property string modelData
            readonly property bool lit: Slots.revision >= 0 && Slots.isLit(index)
            readonly property bool edited: Krimson.editedSlot === index
            x: 40 + index * 68
            y: panel.sectionTop + 190
            width: 60
            height: 40
            radius: 4
            color: edited ? "#5A5B5E" : "#2A2B2E"
            border.color: edited ? panel.silk : "#0E0E0F"
            border.width: edited ? 2 : 1

            Accessible.role: Accessible.Button
            Accessible.name: "Slot " + modelData + (lit ? ", playing" : "") + (edited ? ", edited" : "")

            Rectangle { // lit: the keyboard plays this slot
                x: 7
                y: 7
                width: 8
                height: 8
                radius: 4
                color: slotButton.lit ? panel.led : "#4A1612"
            }
            Text {
                anchors.centerIn: parent
                text: slotButton.modelData
                color: panel.silk
                font.family: Theme.labelFont
                font.pixelSize: 18
                font.bold: true
            }
            MouseArea {
                anchors.fill: parent
                onClicked: mouse => Slots.pressSlot(slotButton.index, (mouse.modifiers & Qt.ShiftModifier) !== 0)
            }
            LearnOverlay {
                anchors.fill: parent
                paramKey: "slot" + slotButton.modelData
                round: false
            }
        }
    }
    Text {
        x: 40
        y: panel.sectionTop + 236
        text: "Click: play and edit · Shift-click: layer"
        color: panel.dim
        font.family: Theme.labelFont
        font.pixelSize: 10
    }

    // The edited slot's setup
    Column {
        x: 40
        y: panel.sectionTop + 262
        spacing: 8

        Text {
            text: "SLOT " + "ABCD"[Krimson.editedSlot] + " SETUP"
            color: panel.silk
            font.family: Theme.labelFont
            font.pixelSize: 11
            font.bold: true
        }
        Stepper {
            readonly property int channel: Slots.revision >= 0 ? Slots.channel(Krimson.editedSlot) : 0
            label: "CH"
            value: channel > 0 ? String(channel) : "--"
            onDown: Slots.setChannel(Krimson.editedSlot, (channel + 16) % 17)
            onUp: Slots.setChannel(Krimson.editedSlot, (channel + 1) % 17)
        }
        Stepper {
            readonly property int zone: Slots.revision >= 0 ? Slots.zone(Krimson.editedSlot) : 0
            label: "ZONE"
            value: ["Whole", "Lower", "Upper"][zone]
            onDown: Slots.setZone(Krimson.editedSlot, (zone + 2) % 3)
            onUp: Slots.setZone(Krimson.editedSlot, (zone + 1) % 3)
        }
        Stepper {
            readonly property int split: Slots.revision >= 0 ? Slots.splitNote : -1
            label: "SPLIT"
            value: split < 0 ? "Off" : panel.noteName(split)
            onDown: Slots.splitNote = split < 0 ? 60 : split - 1
            onUp: Slots.splitNote = split < 0 ? 60 : split + 1
        }
        Row {
            spacing: 8
            PanelButton {
                readonly property bool on: Slots.revision >= 0 && Slots.isOn(Krimson.editedSlot)
                width: 64
                height: 24
                dark: true
                tint: on ? "#E2343A" : "transparent"
                faceText: "ON"
                text: "Slot on" + (on ? "" : ": off")
                onClicked: Slots.setOn(Krimson.editedSlot, !on)
            }
            PanelButton {
                width: 90
                height: 24
                dark: true
                faceText: "SPLIT OFF"
                text: "No split"
                onClicked: Slots.splitNote = -1
            }
        }
    }

    // ---- Sections

    Section { // arpeggiator (it plays from C8) and voice
        title: "ARP · VOICE"
        x: 330
        width: 150

        Toggle { key: "arp_on"; label: "ARP"; x: 12; y: 32 }
        Toggle { key: "arp_hold"; label: "HOLD"; x: 86; y: 32 }
        KKnob { key: "arp_mode"; label: "MODE"; size: 30; x: 2; y: 66 }
        KKnob { key: "arp_range"; label: "RANGE"; size: 30; x: 50; y: 66 }
        KKnob { key: "arp_rate"; label: "RATE"; size: 30; x: 98; y: 66 }
        Choices { key: "voice_mode"; label: "VOICE MODE"; x: 12; y: 142 }
        KKnob { key: "glide"; label: "GLIDE"; size: 38; x: 6; y: 228 }
        KKnob { key: "unison_detune"; label: "DETUNE"; size: 38; x: 78; y: 228 }
        KKnob { key: "bend_range"; label: "BEND"; size: 34; x: 40; y: 310 }
    }

    Section {
        title: "LFO 1 · LFO 2"
        x: 486
        width: 180

        KKnob { key: "lfo1_rate"; label: "LFO 1 RATE"; size: 40; x: 6; y: 30 }
        KKnob { key: "lfo1_amt"; label: "AMOUNT"; size: 40; x: 6; y: 110 }
        Choices { key: "lfo1_wave"; label: "WAVE"; x: 8; y: 194; width: 82 }
        Choices { key: "lfo1_dest"; label: "DEST"; x: 8; y: 296; width: 82 }
        KKnob { key: "lfo2_rate"; label: "LFO 2 RATE"; size: 40; x: 96; y: 30 }
        KKnob { key: "lfo2_amt"; label: "AMOUNT"; size: 40; x: 96; y: 110 }
        Choices { key: "lfo2_dest"; label: "DEST"; x: 98; y: 194; width: 78 }
    }

    Section {
        title: "MOD ENV"
        x: 672
        width: 96

        KKnob { key: "menv_a"; label: "ATTACK"; size: 40; x: 13; y: 30 }
        KKnob { key: "menv_d"; label: "DECAY"; size: 40; x: 13; y: 110 }
        KKnob { key: "menv_amt"; label: "AMOUNT"; size: 40; x: 13; y: 190; bipolar: true }
        Choices { key: "menv_dest"; label: "DEST"; x: 6; y: 280; width: 88 }
    }

    Section {
        title: "OSCILLATORS"
        x: 774
        width: 226

        Choices { key: "osc1_wave"; label: "OSC 1"; x: 10; y: 30 }
        Choices { key: "osc2_wave"; label: "OSC 2"; x: 120; y: 30 }
        KKnob { key: "pw"; label: "PULSE WIDTH"; size: 40; x: 18; y: 112 }
        KKnob { key: "osc2_semi"; label: "SEMITONES"; size: 40; x: 128; y: 112 }
        KKnob { key: "osc_mix"; label: "MIX"; size: 40; x: 18; y: 192 }
        KKnob { key: "osc2_fine"; label: "FINE"; size: 40; x: 128; y: 192; bipolar: true }
        KKnob { key: "fm"; label: "FM"; size: 40; x: 18; y: 272 }
        Toggle { key: "osc2_kbd"; label: "KBD"; x: 140; y: 290 }
        Toggle { key: "sync"; label: "SYNC"; x: 24; y: 366 }
        Toggle { key: "ring"; label: "RING"; x: 140; y: 366 }
    }

    Section {
        title: "FILTER"
        x: 1006
        width: 226

        Choices { key: "filter_type"; label: "TYPE"; x: 10; y: 30 }
        KKnob { key: "cutoff"; label: "FREQ"; size: 58; x: 126; y: 28 }
        KKnob { key: "res"; label: "RES"; size: 40; x: 18; y: 124 }
        KKnob { key: "drive"; label: "DRIVE"; size: 40; x: 132; y: 124 }
        KKnob { key: "fenv_amt"; label: "ENV AMT"; size: 40; x: 18; y: 200 }
        KKnob { key: "fenv_vel"; label: "VELOCITY"; size: 40; x: 132; y: 200 }
        Choices { key: "kbd_track"; label: "KBD TRACK"; x: 10; y: 278; width: 100 }
        Text { x: 120; y: 280; text: "FILTER ENV"; color: panel.silk; font.family: Theme.labelFont; font.pixelSize: 10; font.bold: true }
        KKnob { key: "fenv_a"; label: "A"; size: 26; x: 108; y: 296 }
        KKnob { key: "fenv_d"; label: "D"; size: 26; x: 164; y: 296 }
        KKnob { key: "fenv_s"; label: "S"; size: 26; x: 108; y: 356 }
        KKnob { key: "fenv_r"; label: "R"; size: 26; x: 164; y: 356 }
    }

    Section {
        title: "AMP ENV"
        x: 1238
        width: 110

        KKnob { key: "aenv_a"; label: "ATTACK"; size: 34; x: 0; y: 30 }
        KKnob { key: "aenv_d"; label: "DECAY"; size: 34; x: 54; y: 30 }
        KKnob { key: "aenv_s"; label: "SUSTAIN"; size: 34; x: 0; y: 104 }
        KKnob { key: "aenv_r"; label: "RELEASE"; size: 34; x: 54; y: 104 }
        KKnob { key: "amp_vel"; label: "VELOCITY"; size: 40; x: 21; y: 186 }
        KKnob { key: "gain"; label: "GAIN"; size: 40; x: 21; y: 266 }
    }

    Section { // effects (they sound from C6)
        title: "FX"
        x: 1354
        width: 110

        Choices { key: "chorus"; label: "CHORUS"; x: 8; y: 30 }
        KKnob { key: "delay_level"; label: "DELAY"; size: 30; x: 0; y: 98 }
        KKnob { key: "delay_fb"; label: "FEEDBACK"; size: 30; x: 54; y: 98 }
        KKnob { key: "delay_time"; label: "TIME"; size: 30; x: 27; y: 168 }
        KKnob { key: "reverb_level"; label: "REVERB"; size: 30; x: 0; y: 240 }
        KKnob { key: "reverb_size"; label: "SIZE"; size: 30; x: 54; y: 240 }
    }

    Section {
        title: "OUTPUT"
        x: 1470
        width: 98

        KKnob { key: "slot_level"; label: "LEVEL"; size: 44; x: 15; y: 30 }
        KKnob { key: "slot_pan"; label: "PAN"; size: 44; x: 15; y: 116; bipolar: true }
        Text {
            x: 8
            y: 210
            width: 82
            wrapMode: Text.WordWrap
            text: "Slot " + "ABCD"[Krimson.editedSlot] + "'s level and pan"
            color: panel.dim
            font.family: Theme.labelFont
            font.pixelSize: 10
        }
    }

    // ---- Wheels and keyboard

    Rectangle {
        x: 24
        y: 516
        width: 1552
        height: 360
        radius: 6
        color: "#1A1A1B"
    }
    Wheel { x: 50; y: 560; label: "PITCH"; springs: true; height: 200; onChanged: a => Krimson.pitchBend(a * 2 - 1) }
    Wheel { x: 120; y: 560; label: "MOD"; height: 200; onChanged: a => Krimson.modWheel(a) }

    // Performances: all four slots' sounds and setup, 1-8.
    Column {
        x: 40
        y: 776
        spacing: 6
        Row {
            spacing: 4
            Text {
                width: 38
                anchors.verticalCenter: parent.verticalCenter
                text: "PERF"
                color: panel.silk
                font.family: Theme.labelFont
                font.pixelSize: 11
                font.bold: true
            }
            PanelButton { width: 22; height: 22; dark: true; faceText: "◂"; text: "Previous performance"; onClicked: Programs.stepPerformance(-1) }
            Text {
                width: 40
                anchors.verticalCenter: parent.verticalCenter
                horizontalAlignment: Text.AlignHCenter
                text: (Programs.performance + 1) + (Programs.revision >= 0 && Programs.performanceUsed(Programs.performance) ? "" : "·")
                color: panel.led
                font.family: "monospace"
                font.pixelSize: 16
                font.bold: true
            }
            PanelButton { width: 22; height: 22; dark: true; faceText: "▸"; text: "Next performance"; onClicked: Programs.stepPerformance(1) }
        }
        Row {
            spacing: 6
            PanelButton {
                width: 64
                height: 24
                dark: true
                faceText: "LOAD"
                text: "Load the performance"
                enabled: Programs.revision >= 0 && Programs.performanceUsed(Programs.performance)
                opacity: enabled ? 1.0 : 0.4
                onClicked: Programs.loadPerformance(Programs.performance)
            }
            PanelButton {
                width: 64
                height: 24
                dark: true
                faceText: "SAVE"
                text: "Save all four slots as this performance"
                onClicked: Programs.savePerformance(Programs.performance)
            }
        }
    }

    ControllerPianoKeyboard { // key for key with your MIDI keyboard (the rack's KEYS setting)
        instrument: Krimson
        x: 200
        y: 540
        width: 1360
        height: 320
        instrumentOctave: octave.shift
        litColor: "#E2343A"
    }

    // Keyboard shortcuts (standalone): Space run/stop, L learn, Esc leaves learn.
    Keys.onSpacePressed: Krimson.playing = !Krimson.playing
    Keys.onPressed: event => {
        if (event.modifiers !== Qt.NoModifier)
            return
        if (event.key === Qt.Key_L) {
            MidiLearn.active = !MidiLearn.active
            event.accepted = true
        } else if (event.key === Qt.Key_Escape && MidiLearn.active) {
            MidiLearn.active = false
            event.accepted = true
        }
    }
}

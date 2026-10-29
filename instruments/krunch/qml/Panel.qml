pragma ComponentBehavior: Bound

import QtQuick
import Krate

// Krunch's panel at design size 1400 x 600: grille cloth with the logo and
// where the guitar comes from; below, the control plate: input, the four
// channel buttons, the playing channel's controls, the power amp, level.
Item {
    id: panel

    width: 1400
    height: 600

    readonly property int input: Krunch.revision >= 0 && Krunch.value("input") >= 0.5 ? 1 : 0
    readonly property int channel: Krunch.revision >= 0 ? Math.round(Krunch.value("channel") * 3) : 0
    readonly property string inputPort: Host.inputPorts[input] ?? ""
    readonly property bool inputConnected: Host.inputsConnected[input] ?? false
    readonly property color cream: "#E9DFC4"
    readonly property var voiceNames: ["CLEAN", "CRUNCH", "HEAVY", "LEAD"]
    readonly property var channelColors: ["#5FB0E8", "#F2C230", "#F08A2B", "#E2412E"]

    function chKey(name) { return "ch" + (channel + 1) + "_" + name }
    function voiceOf(c) { return Krunch.revision >= 0 ? Math.round(Krunch.value("ch" + (c + 1) + "_voice") * 3) : 0 }

    component Meter: Rectangle { // vertical, -60..0 dB
        property real level: 0
        width: 16
        height: 120
        radius: 3
        color: "#0C0C0D"
        border.color: "#3A3B3E"
        Rectangle {
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 2
            x: 2
            width: parent.width - 4
            height: (parent.height - 4) * parent.level
            radius: 2
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#FF3A2A" }
                GradientStop { position: 0.15; color: "#F2C230" }
                GradientStop { position: 0.4; color: "#5FD35A" }
                GradientStop { position: 1.0; color: "#2F9A35" }
            }
        }
    }

    component Label: Text {
        color: panel.cream
        font.family: Theme.labelFont
        font.pixelSize: 14
        font.bold: true
        horizontalAlignment: Text.AlignHCenter
    }

    // A knob with its label above and its value under it, centred on cx.
    component AmpKnob: Item {
        id: ampKnob
        required property string key
        required property string label
        required property real cx
        property real size: 84
        property int detents: 0
        x: cx - 70
        width: 140
        height: size + 54

        Label { width: parent.width; text: ampKnob.label }
        Knob {
            x: (parent.width - width) / 2
            y: 22
            width: ampKnob.size
            height: ampKnob.size
            name: Krunch.label(ampKnob.key)
            detents: ampKnob.detents
            value: Krunch.revision >= 0 ? Krunch.value(ampKnob.key) : 0
            defaultValue: Krunch.defaultValue(ampKnob.key)
            displayText: Krunch.displayText(ampKnob.key, value)
            onMoved: v => Krunch.setValue(ampKnob.key, v)

            LearnOverlay { anchors.fill: parent; paramKey: ampKnob.key }
        }
        Label {
            y: ampKnob.size + 28
            width: parent.width
            text: Krunch.displayText(ampKnob.key, Krunch.revision >= 0 ? Krunch.value(ampKnob.key) : 0)
            font.pixelSize: 12
            font.bold: false
        }
    }

    Rectangle { // the cabinet: black tolex
        anchors.fill: parent
        radius: 10
        color: "#161617"
        border.color: "#050505"
        border.width: 3
    }

    Rectangle { // grille cloth
        x: 24
        y: 18
        width: parent.width - 48
        height: 130
        radius: 6
        color: "#2B2A27"
        border.color: "#0A0A0A"
        border.width: 2
        clip: true

        Repeater { // the weave
            model: 80
            Rectangle {
                required property int index
                x: index * 19 - 60
                y: -20
                width: 3
                height: 180
                rotation: 30
                color: "#33322E"
            }
        }

        Text {
            x: 40
            anchors.verticalCenter: parent.verticalCenter
            text: "Krunch"
            color: panel.cream
            style: Text.Raised
            styleColor: "#000"
            font.family: Theme.labelFont
            font.pixelSize: 80
            font.bold: true
            font.italic: true
        }

        // Where the guitar comes from, and how long it takes to reach the speakers.
        Column {
            x: 520
            width: parent.width - 560
            anchors.verticalCenter: parent.verticalCenter
            spacing: 6

            Text {
                width: parent.width
                horizontalAlignment: Text.AlignRight
                text: panel.inputConnected ? "GUITAR  " + Host.portLabel(panel.inputPort)
                                           : panel.inputPort === "" ? "NO INPUT CHOSEN"
                                                                    : "WAITING FOR  " + Host.portLabel(panel.inputPort)
                color: panel.inputConnected ? panel.cream : Theme.labelOrange
                font.family: Theme.labelFont
                font.pixelSize: 17
                font.bold: true
                elide: Text.ElideLeft
            }
            Text {
                width: parent.width
                horizontalAlignment: Text.AlignRight
                visible: !panel.inputConnected
                text: panel.inputPort === ""
                      ? "Choose your audio interface's input under AUDIO in the top bar (as input " + (panel.input + 1) + ")."
                      : "Krunch connects as soon as that device appears. Is it plugged in?"
                color: "#B9B2A0"
                font.family: Theme.labelFont
                font.pixelSize: 13
                wrapMode: Text.WordWrap
            }
            Text {
                width: parent.width
                horizontalAlignment: Text.AlignRight
                text: Host.audioConnected
                      ? "DELAY  " + Host.latencyMs.toFixed(1) + " ms  ·  " + Host.bufferSize + " frames"
                        + (Host.latencyMs > 12 ? "  ·  set a smaller buffer under AUDIO" : "")
                      : "AUDIO OFFLINE"
                color: Host.latencyMs > 12 ? Theme.labelOrange : panel.cream
                font.family: Theme.labelFont
                font.pixelSize: 15
                font.bold: true
            }
        }
    }

    Rectangle { // control plate
        id: plate
        x: 24
        y: 164
        width: parent.width - 48
        height: 418
        radius: 6
        border.color: "#3A3B3E"
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#232325" }
            GradientStop { position: 1.0; color: "#0F0F10" }
        }

        // --- Top row: input, channels, power amp, level ---------------------------

        AmpKnob { y: 14; cx: 70; key: "input"; label: "INPUT"; size: 64; detents: 2 }

        Label { x: 128; y: 14; width: 40; text: "IN" }
        Meter { x: 140; y: 38; height: 96; level: Krunch.inputLevel }
        Led { x: 142; y: 142; width: 12; height: 12; on: Krunch.clipping }
        Label { x: 118; y: 158; width: 60; text: "CLIP"; font.pixelSize: 10 }

        Label { x: 210; y: 14; width: 440; text: "CHANNEL" }
        Repeater {
            model: 4
            Column {
                id: channelButton
                required property int index
                x: 210 + index * 112
                y: 40
                spacing: 6
                PanelButton {
                    width: 104
                    height: 56
                    dark: true
                    faceText: (channelButton.index + 1).toString()
                    text: "Channel " + (channelButton.index + 1)
                    hasLed: true
                    ledOn: panel.channel === channelButton.index
                    tint: panel.channel === channelButton.index ? panel.channelColors[channelButton.index] : "transparent"
                    onClicked: Krunch.setValue("channel", channelButton.index / 3)

                    LearnOverlay {
                        anchors.fill: parent
                        paramKey: "channel" + (channelButton.index + 1)
                        round: false
                    }
                }
                Label {
                    width: 104
                    text: panel.voiceNames[panel.voiceOf(channelButton.index)]
                    font.pixelSize: 12
                    color: panel.channel === channelButton.index ? panel.channelColors[channelButton.index] : "#8F897A"
                }
            }
        }

        Item { // the gate's light: shut while you're not playing
            x: 676
            y: 40
            width: 60
            height: 80
            Led { x: 24; y: 18; width: 12; height: 12; on: Krunch.gateClosed }
            Label { y: 40; width: parent.width; text: "GATE\nSHUT"; font.pixelSize: 10 }
        }

        Rectangle { // power amp section
            x: 760
            y: 8
            width: 410
            height: 176
            radius: 6
            color: "transparent"
            border.color: "#3A3B3E"
            Label { x: 10; y: 152; text: "POWER AMP"; font.pixelSize: 11; color: "#8F897A" }
        }
        AmpKnob { y: 14; cx: 830; key: "master"; label: "MASTER" }
        AmpKnob { y: 14; cx: 965; key: "sag"; label: "SAG" }
        AmpKnob { y: 14; cx: 1100; key: "resonance"; label: "RESONANCE" }

        AmpKnob { y: 14; cx: 1240; key: "level"; label: "LEVEL" }
        Label { x: 1296; y: 14; width: 40; text: "OUT" }
        Meter { x: 1308; y: 38; height: 116; level: Krunch.outputLevel }

        // --- The playing channel ----------------------------------------------------

        Rectangle {
            x: 14
            y: 196
            width: plate.width - 28
            height: 208
            radius: 6
            color: "#1A1A1C"
            border.color: panel.channelColors[panel.channel]
            border.width: 2

            Label {
                x: 16
                y: 182
                text: "CHANNEL " + (panel.channel + 1) + "  ·  " + panel.voiceNames[panel.voiceOf(panel.channel)]
                font.pixelSize: 12
                color: panel.channelColors[panel.channel]
            }
        }
        Repeater {
            model: [
                { name: "voice", label: "VOICE", detents: 4 },
                { name: "gain", label: "GAIN" },
                { name: "bass", label: "BASS" },
                { name: "mid", label: "MID" },
                { name: "treble", label: "TREBLE" },
                { name: "presence", label: "PRESENCE" },
                { name: "volume", label: "VOLUME" },
                { name: "gate", label: "GATE" }
            ]
            AmpKnob {
                required property var modelData
                required property int index
                y: 214
                cx: 96 + index * 142
                size: 96
                key: panel.chKey(modelData.name)
                label: modelData.label
                detents: modelData.detents ?? 0
            }
        }
        Column { // the channel's effects loop
            x: 1200
            y: 232
            spacing: 6
            PanelButton {
                width: 104
                height: 56
                dark: true
                faceText: "LOOP"
                text: "Effects loop for this channel"
                hasLed: true
                ledOn: Krunch.revision >= 0 && Krunch.value(panel.chKey("loop")) >= 0.5
                onClicked: Krunch.setValue(panel.chKey("loop"), ledOn ? 0 : 1)

                LearnOverlay { anchors.fill: parent; paramKey: "loop"; round: false }
            }
            Label { width: 104; text: "FX LOOP"; font.pixelSize: 12 }
            Label { width: 104; text: "(pedals later)"; font.pixelSize: 10; font.bold: false; color: "#8F897A" }
        }
    }
}

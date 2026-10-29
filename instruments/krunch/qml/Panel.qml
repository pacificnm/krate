pragma ComponentBehavior: Bound

import QtQuick
import Krate

// Krunch's panel at design size 1200 x 480: grille cloth with the logo above,
// a black control plate below. G0: which interface input it plays, its meter
// and clip light, the level, and the delay from guitar to speakers.
Item {
    id: panel

    width: 1200
    height: 480

    readonly property int input: Krunch.revision >= 0 && Krunch.value("input") >= 0.5 ? 1 : 0
    readonly property string inputPort: Host.inputPorts[input] ?? ""
    readonly property bool inputConnected: Host.inputsConnected[input] ?? false
    readonly property color cream: "#E9DFC4"

    component Meter: Rectangle { // vertical, -60..0 dB
        property real level: 0
        width: 18
        height: 150
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
        font.pixelSize: 15
        font.bold: true
        horizontalAlignment: Text.AlignHCenter
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
        y: 20
        width: parent.width - 48
        height: 190
        radius: 6
        color: "#2B2A27"
        border.color: "#0A0A0A"
        border.width: 2
        clip: true

        Repeater { // the weave
            model: 64
            Rectangle {
                required property int index
                x: index * 19 - 40
                y: -20
                width: 3
                height: 260
                rotation: 30
                color: "#33322E"
            }
        }

        Text {
            anchors.centerIn: parent
            text: "Krunch"
            color: panel.cream
            style: Text.Raised
            styleColor: "#000"
            font.family: Theme.labelFont
            font.pixelSize: 96
            font.bold: true
            font.italic: true
        }
    }

    Rectangle { // control plate
        id: plate
        x: 24
        y: 228
        width: parent.width - 48
        height: 232
        radius: 6
        border.color: "#3A3B3E"
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#232325" }
            GradientStop { position: 1.0; color: "#0F0F10" }
        }

        // INPUT: which of the interface's inputs the guitar is on.
        Label { x: 40; y: 18; width: 120; text: "INPUT" }
        Knob {
            x: 50
            y: 50
            width: 100
            height: 100
            name: "Input"
            detents: 2
            minAngle: -45
            maxAngle: 45
            value: Krunch.revision >= 0 ? Krunch.value("input") : 0
            defaultValue: Krunch.defaultValue("input")
            displayText: Krunch.displayText("input", value)
            onMoved: v => Krunch.setValue("input", v)

            LearnOverlay { anchors.fill: parent; paramKey: "input" }
        }
        Label { x: 40; y: 160; width: 120; text: "1        2"; font.pixelSize: 13 }

        // The input as it arrives, and the clip light.
        Label { x: 190; y: 18; width: 60; text: "IN" }
        Meter { x: 211; y: 46; level: Krunch.inputLevel }
        Led { x: 214; y: 214; width: 12; height: 12; on: Krunch.clipping }
        Label { x: 190; y: 199; width: 60; text: "CLIP"; font.pixelSize: 11 }

        // LEVEL
        Label { x: 280; y: 18; width: 140; text: "LEVEL" }
        Knob {
            x: 290
            y: 50
            width: 120
            height: 120
            name: "Level"
            value: Krunch.revision >= 0 ? Krunch.value("level") : 0
            defaultValue: Krunch.defaultValue("level")
            displayText: Krunch.displayText("level", value)
            onMoved: v => Krunch.setValue("level", v)

            LearnOverlay { anchors.fill: parent; paramKey: "level" }
        }
        Label {
            x: 280
            y: 180
            width: 140
            text: Krunch.displayText("level", Krunch.revision >= 0 ? Krunch.value("level") : 0)
            font.pixelSize: 13
            font.bold: false
        }

        Label { x: 440; y: 18; width: 60; text: "OUT" }
        Meter { x: 461; y: 46; level: Krunch.outputLevel }

        // Where the guitar comes from, and how long it takes to reach the speakers.
        Column {
            x: 560
            y: 30
            width: plate.width - 600
            spacing: 14

            Text {
                width: parent.width
                text: panel.inputConnected ? "GUITAR  " + Host.portLabel(panel.inputPort)
                                           : panel.inputPort === "" ? "NO INPUT CHOSEN"
                                                                    : "WAITING FOR  " + Host.portLabel(panel.inputPort)
                color: panel.inputConnected ? panel.cream : Theme.labelOrange
                font.family: Theme.labelFont
                font.pixelSize: 18
                font.bold: true
                elide: Text.ElideRight
            }
            Text {
                width: parent.width
                visible: !panel.inputConnected
                text: panel.inputPort === ""
                      ? "Plug the guitar into your audio interface's instrument input, then choose that input under AUDIO IN in the top bar (as input " + (panel.input + 1) + ")."
                      : "Krunch connects as soon as that device appears. Is it plugged in?"
                color: "#B9B2A0"
                font.family: Theme.labelFont
                font.pixelSize: 14
                wrapMode: Text.WordWrap
            }
            Text {
                width: parent.width
                text: Host.audioConnected
                      ? "DELAY  " + Host.latencyMs.toFixed(1) + " ms  ·  " + Host.bufferSize + " frames per block"
                      : "AUDIO OFFLINE"
                color: Host.latencyMs > 12 ? Theme.labelOrange : panel.cream
                font.family: Theme.labelFont
                font.pixelSize: 16
                font.bold: true
            }
            Text {
                width: parent.width
                visible: Host.audioConnected && Host.latencyMs > 12
                text: "Too slow to play through: set a smaller buffer (128 or 64) under AUDIO IN."
                color: "#B9B2A0"
                font.family: Theme.labelFont
                font.pixelSize: 14
                wrapMode: Text.WordWrap
            }
        }
    }
}

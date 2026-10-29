pragma ComponentBehavior: Bound

import QtQuick
import Krate

// Krimson's panel at design size 1600 x 900 (docs/UI.md). C0: the red body,
// the black control panel with its section frames, branding and the slot
// buttons A-D (which slot the panel edits); the controls arrive in C4.
Item {
    id: panel

    width: 1600
    height: 900

    readonly property color body: "#B3121B"
    readonly property color bodyDark: "#8C0D14"
    readonly property color black: "#141415"
    readonly property color frame: "#3A3B3E"
    readonly property color frameFill: "#232426"
    readonly property color silk: "#EDEDED"

    // The control panel's sections, left to right (x, width in design pixels).
    readonly property var sections: [
        { title: "ARPEGGIATOR", x: 322, w: 132 },
        { title: "VIBRATO · LFO", x: 460, w: 108 },
        { title: "MOD ENV", x: 574, w: 92 },
        { title: "OSCILLATORS", x: 672, w: 206 },
        { title: "FILTER", x: 884, w: 176 },
        { title: "AMP ENV", x: 1066, w: 92 },
        { title: "DELAY · FX", x: 1164, w: 150 },
        { title: "OUTPUT", x: 1320, w: 92 }
    ]

    // Body
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

    // The black control panel (left block and sections)
    Rectangle {
        x: 24
        y: 40
        width: 1400
        height: 440
        radius: 6
        color: panel.black
        border.color: "#000"
    }

    // Branding, on the red to the right
    Text {
        x: 1436
        y: 300
        text: "Krimson"
        color: panel.silk
        font.family: Theme.labelFont
        font.pixelSize: 38
        font.weight: Font.Light
    }
    Text {
        x: 1438
        y: 356
        width: 150
        text: "VIRTUAL ANALOG\nSYNTHESIZER"
        color: "#F3C7C9"
        font.family: Theme.labelFont
        font.pixelSize: 13
        font.letterSpacing: 2
    }
    Text {
        x: 1438
        y: 60
        text: "Krate"
        color: panel.silk
        font.family: Theme.labelFont
        font.pixelSize: 28
        font.bold: true
        font.italic: true
    }

    // Sections
    Repeater {
        model: panel.sections

        Rectangle {
            id: sectionFrame
            required property var modelData
            x: modelData.x
            y: 52
            width: modelData.w
            height: 416
            radius: 4
            color: panel.frameFill
            border.color: panel.frame
            border.width: 2

            Rectangle {
                x: 6
                y: 6
                width: parent.width - 12
                height: 20
                radius: 3
                color: "#5A5B5E"

                Text {
                    anchors.centerIn: parent
                    text: sectionFrame.modelData.title
                    color: panel.silk
                    font.family: Theme.labelFont
                    font.pixelSize: 12
                    font.bold: true
                }
            }
        }
    }

    // Left block: master, morph, program, slots, octave (controls arrive in C4-C7)
    Repeater {
        model: [
            { label: "MASTER LEVEL", y: 58 },
            { label: "MORPH", y: 150 },
            { label: "PROGRAM", y: 222 }
        ]

        Text {
            required property var modelData
            x: 44
            y: modelData.y
            text: modelData.label
            color: panel.silk
            font.family: Theme.labelFont
            font.pixelSize: 12
            font.bold: true
        }
    }

    Text {
        x: 44
        y: 330
        text: "SLOT"
        color: panel.silk
        font.family: Theme.labelFont
        font.pixelSize: 12
        font.bold: true
    }
    Repeater {
        model: ["A", "B", "C", "D"]

        PanelButton {
            required property int index
            required property string modelData
            x: 44 + index * 66
            y: 350
            width: 56
            height: 34
            faceText: modelData
            text: "Edit slot " + modelData
            dark: true
            hasLed: true
            ledOn: Krimson.editedSlot === index
            onClicked: Krimson.editedSlot = index
        }
    }

    // Keyboard area (wheels and keyboard arrive in C4)
    Rectangle {
        x: 24
        y: 500
        width: 1552
        height: 376
        radius: 6
        color: "#1A1A1B"
    }
}

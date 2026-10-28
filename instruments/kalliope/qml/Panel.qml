pragma ComponentBehavior: Bound

import QtQuick
import Krate

// Kalliope's panel at design size 1600 x 1100 (docs/UI.md). P0: chassis,
// branding, section headers and control labels; the controls arrive in P3.
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
    readonly property real columns: 33.5
    readonly property real column: (width - 2 * margin) / columns
    readonly property real sectionTop: 250
    readonly property real sectionBottom: 540

    readonly property var sections: [
        { title: "LFO", columns: 3, controls: [
            { key: "lfo_rate", label: "RATE", kind: "slider", col: 0 },
            { key: "lfo_delay", label: "DELAY\nTIME", kind: "slider", col: 1 },
            { key: "lfo_wave", label: "WAVE", kind: "knob", col: 2 } ] },
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
        { title: "ENV-2", columns: 4, controls: [
            { key: "env2_a", label: "A", kind: "slider", col: 0 },
            { key: "env2_d", label: "D", kind: "slider", col: 1 },
            { key: "env2_s", label: "S", kind: "slider", col: 2 },
            { key: "env2_r", label: "R", kind: "slider", col: 3 } ] }
    ]

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

    // Function deck: arpeggiator, patch display, key mode, banks and patches, effects
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
            { title: "ARPEGGIO", x: 40, w: 300 },
            { title: "PATCH", x: 360, w: 240 },
            { title: "KEY MODE", x: 620, w: 220 },
            { title: "BANK", x: 860, w: 300 },
            { title: "PATCH NUMBER", x: 1180, w: 230 },
            { title: "FX", x: 1430, w: 146 }
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
                text: parent.modelData.title
                color: "#111111"
                font.family: Theme.labelFont
                font.pixelSize: 13
                font.bold: true
            }
        }
    }

    // Keyboard deck (the keyboard arrives in P3)
    Rectangle {
        x: panel.margin
        y: 780
        width: panel.width - 2 * panel.margin
        height: panel.height - y - panel.margin
        radius: 4
        color: "#0F0F10"
    }
}

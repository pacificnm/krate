pragma ComponentBehavior: Bound

import QtQuick
import Krate

// K909's panel at design size 1600 x 940 (docs/UI.md). D0: chassis, branding,
// section headers, labels and every knob; the buttons arrive in D3-D5.
Item {
    id: panel

    width: 1600
    height: 940

    // Panel-specific colors; shared ones come from Theme.
    readonly property color chassisLight: "#E6E4DC"
    readonly property color chassisDark: "#D2CFC5"
    readonly property color headerBar: "#363A3D"
    readonly property color lowerDeck: "#C9C6BC"
    readonly property color stripBar: "#4A4E52"

    // Knob sections, left to right. Width in columns; each knob sits at a
    // (column, row) inside its section.
    readonly property real margin: 24
    readonly property real columns: 17.5
    readonly property real column: (width - 2 * margin) / columns
    readonly property real sectionTop: 286
    readonly property real sectionBottom: 540
    readonly property var rowLabelY: [330, 436]

    readonly property var sections: [
        { title: "TOTAL ACCENT", columns: 1.5, knobs: [
            { key: "accent", label: "", col: 0.25, row: 0 } ] },
        { title: "BASS DRUM", columns: 2, knobs: [
            { key: "bd_tune", label: "TUNE", col: 0, row: 0 }, { key: "bd_level", label: "LEVEL", col: 1, row: 0 },
            { key: "bd_attack", label: "ATTACK", col: 0, row: 1 }, { key: "bd_decay", label: "DECAY", col: 1, row: 1 } ] },
        { title: "SNARE DRUM", columns: 2, knobs: [
            { key: "sd_tune", label: "TUNE", col: 0, row: 0 }, { key: "sd_level", label: "LEVEL", col: 1, row: 0 },
            { key: "sd_tone", label: "TONE", col: 0, row: 1 }, { key: "sd_snappy", label: "SNAPPY", col: 1, row: 1 } ] },
        { title: "LOW TOM", columns: 2, knobs: [
            { key: "lt_tune", label: "TUNE", col: 0, row: 0 }, { key: "lt_level", label: "LEVEL", col: 1, row: 0 },
            { key: "lt_decay", label: "DECAY", col: 0, row: 1 } ] },
        { title: "MID TOM", columns: 2, knobs: [
            { key: "mt_tune", label: "TUNE", col: 0, row: 0 }, { key: "mt_level", label: "LEVEL", col: 1, row: 0 },
            { key: "mt_decay", label: "DECAY", col: 0, row: 1 } ] },
        { title: "HI TOM", columns: 2, knobs: [
            { key: "ht_tune", label: "TUNE", col: 0, row: 0 }, { key: "ht_level", label: "LEVEL", col: 1, row: 0 },
            { key: "ht_decay", label: "DECAY", col: 0, row: 1 } ] },
        { title: "RIM", columns: 1, knobs: [
            { key: "rs_level", label: "LEVEL", col: 0, row: 0 } ] },
        { title: "CLAP", columns: 1, knobs: [
            { key: "cp_level", label: "LEVEL", col: 0, row: 0 } ] },
        { title: "HI HAT", columns: 2, knobs: [
            { key: "ch_level", label: "CH LEVEL", col: 0, row: 0 }, { key: "oh_level", label: "OH LEVEL", col: 1, row: 0 },
            { key: "ch_decay", label: "CH DECAY", col: 0, row: 1 }, { key: "oh_decay", label: "OH DECAY", col: 1, row: 1 } ] },
        { title: "CYMBAL", columns: 2, knobs: [
            { key: "cr_level", label: "CRASH LVL", col: 0, row: 0 }, { key: "rd_level", label: "RIDE LVL", col: 1, row: 0 },
            { key: "cr_tune", label: "CRASH TUNE", col: 0, row: 1 }, { key: "rd_tune", label: "RIDE TUNE", col: 1, row: 1 } ] }
    ]

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
        width: 62
        height: 62
        name: key
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
        y: 214
        text: "DRUM MACHINE"
        color: panel.headerBar
        font.family: Theme.labelFont
        font.pixelSize: 40
        font.letterSpacing: 6
    }

    // Knob sections
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
                visible: section.index < panel.sections.length - 1
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
                        font.pixelSize: 13
                    }
                    DrumKnob {
                        key: knobSlot.modelData.key
                        x: knobSlot.cx - width / 2
                        y: knobSlot.labelY + 24
                    }
                }
            }
        }
    }

    // Middle deck labels (controls arrive in D2-D4)
    PanelLabel { x: 70; y: 572; text: "PATTERN"; orange: true }
    PanelLabel { centerX: 1060; y: 572; text: "TEMPO"; visible: Drums.ownsTransport }
    PanelLabel { centerX: 1190; y: 572; text: "VOLUME" }
    PanelLabel { centerX: 1320; y: 572; text: "SHUFFLE" }
    DrumKnob {
        key: "tempo"
        visible: Drums.ownsTransport
        x: 1060 - width / 2
        y: 600
        width: 84
        height: 84
    }
    DrumKnob {
        key: "volume"
        x: 1190 - width / 2
        y: 596
        width: 100
        height: 100
    }
    DrumKnob {
        key: "shuffle"
        detents: 6
        x: 1320 - width / 2
        y: 604
        width: 70
        height: 70
    }

    // Lower deck: transport buttons, step buttons and the track strip
    Rectangle {
        x: panel.margin
        y: 730
        width: panel.width - 2 * panel.margin
        height: panel.height - y - panel.margin
        radius: 6
        color: panel.lowerDeck
        border.color: Theme.deckLine
    }
    PanelLabel { centerX: 120; y: 760; text: "START"; visible: Drums.ownsTransport }
    PanelLabel { centerX: 120; y: 820; text: "STOP"; visible: Drums.ownsTransport }
    PanelLabel { centerX: 120; y: 870; text: "CLEAR" }

    Repeater {
        model: 16

        PanelLabel {
            required property int index
            centerX: panel.stepLeft + (index + 0.5) * panel.stepWidth
            y: 752
            text: index + 1
            font.pixelSize: 15
        }
    }

    // Track strip: right-click auditions a voice (left-click selects its track from D4).
    Repeater {
        model: panel.tracks

        Rectangle {
            id: trackCell
            required property var modelData
            required property int index
            x: panel.stepLeft + (modelData.first - 1) * panel.stepWidth + 4
            y: 876
            width: modelData.steps * panel.stepWidth - 8
            height: 30
            radius: 4
            color: trackMouse.pressed ? Theme.labelOrange : "transparent"
            border.color: panel.stripBar
            border.width: 2

            Accessible.role: Accessible.Button
            Accessible.name: modelData.name

            PanelLabel {
                anchors.centerIn: parent
                text: trackCell.modelData.name
                font.pixelSize: 13
            }

            MouseArea {
                id: trackMouse
                anchors.fill: parent
                acceptedButtons: Qt.RightButton
                onPressed: Drums.audition(trackCell.index)
            }
        }
    }
}

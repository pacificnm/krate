pragma ComponentBehavior: Bound

import QtQuick
import Krate

// Klick's panel at design size 900 x 360: four knobs on a silver plate.
Item {
    id: panel

    width: 900
    height: 360

    Rectangle {
        anchors.fill: parent
        radius: 6
        border.color: Theme.deckLine
        border.width: 2
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.silverLight }
            GradientStop { position: 1.0; color: Theme.silverDark }
        }
    }

    Text {
        x: 36
        y: 26
        text: "Klick"
        color: Theme.ink
        font.family: Theme.labelFont
        font.pixelSize: 44
        font.bold: true
        font.italic: true
    }
    PanelLabel {
        x: 40
        y: 84
        text: "METRONOME"
        font.pixelSize: 14
    }

    Repeater {
        model: [
            { label: "LEVEL", key: "level", cx: 150 },
            { label: "PITCH", key: "pitch", cx: 350 },
            { label: "ACCENT", key: "accent", cx: 550 },
            { label: "BEATS", key: "beats", cx: 750, detents: 6 }
        ]

        Item {
            id: knobSlot
            required property var modelData

            PanelLabel {
                centerX: knobSlot.modelData.cx
                y: 150
                text: knobSlot.modelData.label
            }

            Knob {
                x: knobSlot.modelData.cx - width / 2
                y: 182
                width: 120
                height: 120
                name: knobSlot.modelData.label
                detents: knobSlot.modelData.detents ?? 0
                value: Klick.revision >= 0 ? Klick.value(knobSlot.modelData.key) : 0
                defaultValue: Klick.defaultValue(knobSlot.modelData.key)
                displayText: Klick.displayText(knobSlot.modelData.key, value)
                onMoved: v => Klick.setValue(knobSlot.modelData.key, v)
            }

            PanelLabel {
                centerX: knobSlot.modelData.cx
                y: 312
                text: Klick.displayText(knobSlot.modelData.key, Klick.revision >= 0 ? Klick.value(knobSlot.modelData.key) : 0)
                font.pixelSize: 13
                font.bold: false
            }
        }
    }
}

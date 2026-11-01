pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Krate
import KrateRack

// Your MIDI keyboard, so the on-screen keyboards match it key for key: in the
// KEYS popup and on the Settings screen.
Column {
    spacing: 8
    Text {
        text: "YOUR MIDI KEYBOARD"
        color: Theme.ink
        font.family: Theme.labelFont
        font.pixelSize: 13
        font.bold: true
    }
    Repeater {
        model: [
            { label: "Keys", key: "keys", from: 13, to: 88 },
            { label: "First key's note at octave 0", key: "firstNote", from: 0, to: 127 },
            { label: "Octaves down", key: "octaveDown", from: 0, to: 8 },
            { label: "Octaves up", key: "octaveUp", from: 0, to: 8 }
        ]
        Row {
            id: setting
            required property var modelData
            spacing: 8
            Text {
                width: 180
                anchors.verticalCenter: parent.verticalCenter
                text: setting.modelData.label
                color: Theme.ink
                font.family: Theme.labelFont
                font.pixelSize: 12
            }
            SpinBox {
                width: 116
                from: setting.modelData.from
                to: setting.modelData.to
                editable: true
                value: ControllerKeyboard[setting.modelData.key]
                onValueModified: ControllerKeyboard[setting.modelData.key] = value
                Accessible.name: setting.modelData.label
            }
        }
    }
    Row {
        spacing: 8
        PanelButton {
            width: 150
            height: 30
            faceText: ControllerKeyboard.capturing ? "PRESS LOWEST KEY" : "SET FROM KEY"
            text: "Set the first key's note: press this, then your keyboard's lowest key (at octave 0)"
            tint: ControllerKeyboard.capturing ? Theme.labelOrange : "transparent"
            onClicked: ControllerKeyboard.capturing = !ControllerKeyboard.capturing
        }
        PanelButton {
            width: 90
            height: 30
            faceText: "DEFAULTS"
            text: "49 keys, first note 35, octaves -3 to +4"
            onClicked: ControllerKeyboard.restoreDefaults()
        }
    }
    Row { // the keyboard plays the focused instrument
        spacing: 8
        Text {
            width: 120
            anchors.verticalCenter: parent.verticalCenter
            text: "Keys play focus on"
            color: Theme.ink
            font.family: Theme.labelFont
            font.pixelSize: 12
        }
        ComboBox {
            id: keysChannel
            width: 170
            // 0: any channel (16), 1..16: that channel (0..15), 17: off (-1)
            model: ["Any channel"].concat(Array.from({ length: 16 }, (_, i) => "Channel " + (i + 1)))
                                  .concat(["Off: mixer channels"])
            currentIndex: Rack.keysChannel === 16 ? 0 : Rack.keysChannel < 0 ? 17 : Rack.keysChannel + 1
            onActivated: index => Rack.keysChannel = index === 0 ? 16 : index === 17 ? -1 : index - 1
            Accessible.name: "Channel your keyboard plays the focused instrument on"
        }
    }
    Text {
        width: parent.width
        text: Rack.keysChannel === 16
              ? "Every note you play goes to the instrument in focus (its tab, Tab, or a learned focus button), whatever channels the mixer gives the instruments."
              : Rack.keysChannel >= 0
                ? "Notes on channel " + (Rack.keysChannel + 1) + " go to the instrument in focus; other channels follow the mixer's CH settings (pads, other gear)."
                : "Off: notes go to the instrument holding their channel (the mixer's CH), else to the one in focus."
        color: "#55575B"
        font.family: Theme.labelFont
        font.pixelSize: 11
        wrapMode: Text.WordWrap
    }
    Text {
        width: parent.width
        text: "SET FROM KEY: set your keyboard to octave 0, press it, then your lowest key (with Krimson or Kalliope in view)."
        color: "#55575B"
        font.family: Theme.labelFont
        font.pixelSize: 11
        wrapMode: Text.WordWrap
    }
}

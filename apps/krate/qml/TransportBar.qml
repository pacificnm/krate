import QtQuick
import QtQuick.Controls
import Krate

// The rack's transport: play/stop, tempo, beat LED, audio status.
Rectangle {
    id: bar

    height: 76
    color: Theme.silverDark
    border.color: Theme.deckLine

    Row {
        x: 16
        anchors.verticalCenter: parent.verticalCenter
        spacing: 18

        Column {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2
            PanelButton {
                width: 92
                height: 46
                text: "Play / Stop"
                indicatorBar: true
                ledOn: Rack.playing
                onClicked: Rack.playing = !Rack.playing

                LearnOverlay {
                    anchors.fill: parent
                    paramKey: "rack:playstop"
                    round: false
                }
            }
            PanelLabel {
                width: 92
                text: "PLAY / STOP"
                font.pixelSize: 10
            }
        }

        Led {
            anchors.verticalCenter: parent.verticalCenter
            width: 18
            height: 18
            on: Rack.beat
        }

        Column {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2
            PanelLabel { text: "TEMPO"; font.pixelSize: 11 }
            Text {
                text: Math.round(Rack.bpm) + " BPM"
                color: Theme.ink
                font.family: "monospace"
                font.pixelSize: 20
            }
        }

        Knob {
            anchors.verticalCenter: parent.verticalCenter
            width: 50
            height: 50
            name: "Tempo"
            dragRange: 400
            value: (Rack.bpm - 40) / 200
            defaultValue: 0.4
            displayText: Math.round(40 + value * 200) + " BPM"
            onMoved: v => Rack.bpm = 40 + v * 200
        }
    }

    Row { // MIDI learn
        anchors.right: status.left
        anchors.rightMargin: 30
        anchors.verticalCenter: parent.verticalCenter
        spacing: 12

        Column {
            spacing: 2
            PanelButton {
                width: 58
                height: 30
                text: "MIDI Learn"
                hasLed: true
                ledOn: MidiLearn.active
                onClicked: MidiLearn.active = !MidiLearn.active
            }
            PanelLabel { width: 58; text: "LEARN"; font.pixelSize: 10 }
        }
        Column {
            spacing: 2
            PanelButton {
                width: 58
                height: 30
                text: "Follow focus: learned knobs drive whichever instrument has focus"
                hasLed: true
                ledOn: MidiLearn.followFocus
                onClicked: MidiLearn.followFocus = !MidiLearn.followFocus
            }
            PanelLabel { width: 58; text: "FOLLOW"; font.pixelSize: 10 }
        }
        Column {
            spacing: 2
            PanelButton {
                width: 58
                height: 30
                text: "Your MIDI keyboard: keys, first note, octave range"
                hasLed: true
                ledOn: keysPopup.opened || ControllerKeyboard.capturing
                onClicked: keysPopup.opened ? keysPopup.close() : keysPopup.open()
            }
            PanelLabel { width: 58; text: "KEYS"; font.pixelSize: 10 }
        }
    }

    // Your MIDI keyboard, so the on-screen keyboards match it key for key.
    Popup {
        id: keysPopup
        x: parent.width - width - 200
        y: parent.height + 4
        width: 330
        padding: 14
        onClosed: ControllerKeyboard.capturing = false
        background: Rectangle {
            radius: 6
            color: Theme.silverLight
            border.color: Theme.deckLine
        }

        Column {
            width: parent.width
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
            Text {
                width: parent.width
                text: "SET FROM KEY: set your keyboard to octave 0, press it, then your lowest key (with Krimson or Kalliope in view)."
                color: "#55575B"
                font.family: Theme.labelFont
                font.pixelSize: 11
                wrapMode: Text.WordWrap
            }
        }
    }

    Column {
        id: status
        anchors.right: title.left
        anchors.rightMargin: 30
        anchors.verticalCenter: parent.verticalCenter
        spacing: 4
        PanelButton {
            width: 48
            height: 22
            text: "Audio: reconnect"
            hasLed: true
            ledOn: Host.audioConnected
            onClicked: Host.connectAudio()
        }
        PanelLabel {
            text: Host.audioStatus
            color: Host.audioConnected ? Theme.ink : Theme.ledOn
            font.pixelSize: 12
        }
    }

    Text {
        id: title
        anchors.right: parent.right
        anchors.rightMargin: 20
        anchors.verticalCenter: parent.verticalCenter
        text: "krate"
        color: Theme.ink
        font.family: Theme.labelFont
        font.pixelSize: 36
        font.bold: true
        font.italic: true
    }
}

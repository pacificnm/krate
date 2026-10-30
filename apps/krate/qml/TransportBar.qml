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
        Column {
            spacing: 2
            PanelButton {
                width: 58
                height: 30
                text: "Audio: where Krate plays to, which input of your audio interface the guitar is on, and the buffer size"
                hasLed: true
                ledOn: audioPopup.opened || (Host.inputsConnected[0] ?? false) || (Host.inputsConnected[1] ?? false)
                onClicked: audioPopup.opened ? audioPopup.close() : audioPopup.open()
            }
            PanelLabel { width: 58; text: "AUDIO"; font.pixelSize: 10 }
        }
    }

    // The audio interface's inputs (a guitar, for Krunch) and the buffer size.
    Popup {
        id: audioPopup
        x: parent.width - width - 200
        y: parent.height + 4
        width: 560
        padding: 14
        background: Rectangle {
            radius: 6
            color: Theme.silverLight
            border.color: Theme.deckLine
        }

        Column {
            width: parent.width
            spacing: 10
            Text {
                text: "AUDIO"
                color: Theme.ink
                font.family: Theme.labelFont
                font.pixelSize: 13
                font.bold: true
            }
            Row {
                id: outputRow
                // Every playback device, plus the chosen one while it's unplugged.
                readonly property var choices: {
                    const list = [{ device: "", label: "First outputs found" }]
                    for (const d of Host.outputDevices)
                        list.push({ device: d, label: d })
                    if (Host.outputDevice !== "" && Host.outputDevices.indexOf(Host.outputDevice) < 0)
                        list.push({ device: Host.outputDevice, label: Host.outputDevice + " (unplugged)" })
                    return list
                }
                spacing: 8
                Text {
                    width: 56
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Output"
                    color: Theme.ink
                    font.family: Theme.labelFont
                    font.pixelSize: 12
                }
                ComboBox {
                    width: 368
                    model: outputRow.choices
                    textRole: "label"
                    currentIndex: outputRow.choices.findIndex(c => c.device === Host.outputDevice)
                    onActivated: index => Host.outputDevice = outputRow.choices[index].device
                    Accessible.name: "Output"
                }
            }
            Repeater {
                model: Host.inputCount
                Row {
                    id: inputRow
                    required property int index
                    readonly property string port: Host.inputPorts[index] ?? ""
                    // Every capture port, plus the chosen one while its device is unplugged.
                    readonly property var choices: {
                        const list = [{ port: "", label: "None" }]
                        for (const p of Host.capturePorts)
                            list.push({ port: p, label: Host.portLabel(p) })
                        if (port !== "" && Host.capturePorts.indexOf(port) < 0)
                            list.push({ port: port, label: Host.portLabel(port) + " (unplugged)" })
                        return list
                    }
                    spacing: 8
                    Text {
                        width: 56
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Input " + (inputRow.index + 1)
                        color: Theme.ink
                        font.family: Theme.labelFont
                        font.pixelSize: 12
                    }
                    ComboBox {
                        width: 368
                        model: inputRow.choices
                        textRole: "label"
                        currentIndex: inputRow.choices.findIndex(c => c.port === inputRow.port)
                        onActivated: index => Host.setInputPort(inputRow.index, inputRow.choices[index].port)
                        Accessible.name: "Input " + (inputRow.index + 1)
                    }
                    Led {
                        anchors.verticalCenter: parent.verticalCenter
                        on: Host.inputsConnected[inputRow.index] ?? false
                    }
                    Rectangle { // level
                        anchors.verticalCenter: parent.verticalCenter
                        width: 60
                        height: 10
                        radius: 2
                        color: "#1C1C1E"
                        Rectangle {
                            width: parent.width * (Host.inputLevels[inputRow.index] ?? 0)
                            height: parent.height
                            radius: 2
                            color: (Host.inputLevels[inputRow.index] ?? 0) > 0.99 ? Theme.ledOn : "#4CBF4A"
                        }
                    }
                }
            }
            Row {
                spacing: 8
                Text {
                    width: 56
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Buffer"
                    color: Theme.ink
                    font.family: Theme.labelFont
                    font.pixelSize: 12
                }
                ComboBox {
                    id: bufferBox
                    readonly property var sizes: [0, 64, 128, 256, 512, 1024]
                    width: 368
                    model: sizes.map(n => n === 0 ? "The audio server's own" : n + " frames")
                    currentIndex: Math.max(0, sizes.indexOf(Host.requestedBufferSize))
                    onActivated: index => Host.requestedBufferSize = sizes[index]
                    Accessible.name: "Buffer size"
                }
            }
            Text {
                text: Host.audioConnected ? "Delay from input to speakers: " + Host.latencyMs.toFixed(1) + " ms (" + Host.bufferSize + " frames per block)"
                                          : "Audio offline"
                color: Host.latencyMs > 12 ? Theme.labelOrange : Theme.ink
                font.family: Theme.labelFont
                font.pixelSize: 12
                font.bold: true
            }
            Text {
                width: parent.width
                text: "Under about 10 ms feels like playing an amp. 128 frames suits most computers; if you hear crackles (xruns), go up one."
                color: "#55575B"
                font.family: Theme.labelFont
                font.pixelSize: 11
                wrapMode: Text.WordWrap
            }
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

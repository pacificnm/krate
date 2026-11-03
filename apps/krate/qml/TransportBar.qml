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

        Column { // recording (docs/DAW.md, D2): armed audio tracks record while playing
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2
            PanelButton {
                width: 58
                height: 46
                text: "Record: armed audio tracks record while the transport plays"
                hasLed: true
                ledOn: Tracks.recording
                tint: Tracks.recording ? Theme.ledOn : "transparent"
                onClicked: Tracks.recording = !Tracks.recording

                LearnOverlay {
                    anchors.fill: parent
                    paramKey: "rack:record"
                    round: false
                }
            }
            PanelLabel {
                width: 58
                text: "REC"
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
        Column {
            spacing: 2
            PanelButton {
                width: 58
                height: 30
                faceText: "?"
                text: "Krate's manual: the screens, recording, saving, settings (each instrument has its own HELP)"
                onClicked: manual.open()
            }
            PanelLabel { width: 58; text: "HELP"; font.pixelSize: 10 }
        }
    }

    ManualView {
        id: manual
        title: "Krate"
        source: "qrc:/qt/qml/KrateRack/docs/MANUAL.md"
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

        AudioSettings {
            width: parent.width
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

        KeysSettings {
            width: parent.width
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
        PanelLabel { // a newer Krate: AUDIO has the button
            visible: Updates.available
            text: "Krate " + Updates.latestVersion + " is out: ⚙ ▸ UPDATE"
            color: Theme.accentUi
            font.pixelSize: 12
            font.bold: true
            MouseArea {
                anchors.fill: parent
                onClicked: Rack.settingsShown = true
            }
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

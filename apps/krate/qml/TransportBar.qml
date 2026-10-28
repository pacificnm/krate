import QtQuick
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

    Column {
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

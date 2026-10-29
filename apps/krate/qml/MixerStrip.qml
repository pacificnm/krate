import QtQuick
import Krate

// One mixer channel: name, pan, mute/solo, fader, meter, MIDI channel.
// device -1 = master.
Item {
    id: strip

    property int device: -1
    property string name: "MASTER"

    readonly property bool isMaster: device < 0
    // Re-evaluate the C++ getters whenever the mixer changes.
    readonly property int rev: Rack.mixerRevision

    width: 110
    height: 230

    Rectangle {
        anchors.fill: parent
        radius: 4
        color: Theme.lowerDeck
        border.color: Theme.deckLine
        border.width: strip.device === Rack.focus && !strip.isMaster ? 2 : 1
    }

    PanelLabel {
        centerX: strip.width / 2
        y: 8
        text: strip.name.toUpperCase()
        color: strip.device === Rack.focus && !strip.isMaster ? Theme.accentUi : Theme.ink
        font.pixelSize: 13
    }

    Knob { // pan
        visible: !strip.isMaster
        x: (strip.width - width) / 2
        y: 30
        width: 34
        height: 34
        name: strip.name + " pan"
        value: strip.rev >= 0 ? (Rack.pan(strip.device) + 1) / 2 : 0.5
        defaultValue: 0.5
        displayText: Math.abs(value - 0.5) < 0.01 ? "C" : (value < 0.5 ? "L" : "R") + Math.round(Math.abs(value - 0.5) * 200)
        onMoved: v => Rack.setPan(strip.device, v * 2 - 1)
    }

    Row {
        visible: !strip.isMaster
        x: (strip.width - width) / 2
        y: 72
        spacing: 6

        PanelButton {
            width: 38
            height: 22
            faceText: "M"
            text: strip.name + " mute"
            dark: strip.rev >= 0 && Rack.mute(strip.device)
            onClicked: Rack.setMute(strip.device, !Rack.mute(strip.device))
        }
        PanelButton {
            width: 38
            height: 22
            faceText: "S"
            text: strip.name + " solo"
            dark: strip.rev >= 0 && Rack.solo(strip.device)
            onClicked: Rack.setSolo(strip.device, !Rack.solo(strip.device))
        }
    }

    Fader {
        x: strip.width / 2 - width + 6
        y: strip.isMaster ? 34 : 102
        height: strip.isMaster ? 164 : 96
        value: strip.rev >= 0 ? Rack.fader(strip.device) : 0.8
        displayText: Rack.faderText(value)
        onMoved: v => Rack.setFader(strip.device, v)
    }

    Meter {
        x: strip.width / 2 + 14
        y: strip.isMaster ? 40 : 108
        height: strip.isMaster ? 152 : 84
        level: strip.isMaster ? Host.outputLevel : (Rack.meters[strip.device] ?? 0)
    }

    PanelLabel {
        centerX: strip.isMaster ? strip.width / 2 : 30
        y: strip.height - 22
        text: Rack.faderText(strip.rev >= 0 ? Rack.fader(strip.device) : 0.8)
        font.pixelSize: 11
        font.bold: false
    }

    // MIDI channel: FOCUS (keys reach it when it has focus) or 1-16 (always).
    // Click: next; right-click or wheel down: previous; double-click: the
    // instrument's own defaults (Krimson: 3-6).
    Rectangle {
        id: channelBox
        readonly property int channel: strip.rev >= 0 && !strip.isMaster ? Rack.midiChannel(strip.device) : 0
        readonly property int count: strip.rev >= 0 && !strip.isMaster ? Rack.midiChannelCount(strip.device) : 0
        function step(delta) { Rack.setMidiChannel(strip.device, (channel + delta + 17) % 17) }

        visible: !strip.isMaster
        x: strip.width - width - 6
        y: strip.height - 25
        width: 50
        height: 19
        radius: 3
        color: channel > 0 ? "#2A2B2E" : "#5A5C60"
        border.color: Theme.deckLine

        Accessible.role: Accessible.Button
        Accessible.name: strip.name + " MIDI channel: " + (channel > 0 ? channel + (count > 1 ? " and " + (count - 1) + " more" : "") : "follows focus")

        Text {
            anchors.centerIn: parent
            // Several channels (Krimson's slots): the first and how many more.
            text: channelBox.channel <= 0 ? "FOCUS"
                : channelBox.count > 1 ? "CH " + channelBox.channel + "+" + (channelBox.count - 1)
                : "CH " + channelBox.channel
            color: Theme.inkInverse
            font.family: Theme.labelFont
            font.pixelSize: 10
            font.bold: true
        }
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onClicked: mouse => channelBox.step(mouse.button === Qt.RightButton ? -1 : 1)
            onDoubleClicked: Rack.restoreMidiChannels(strip.device)
            onWheel: wheel => channelBox.step(wheel.angleDelta.y > 0 ? 1 : -1)
        }
    }
}

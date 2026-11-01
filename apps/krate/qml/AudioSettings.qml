pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Krate
import KrateRack

// Where Krate plays to, the audio interface's inputs (a guitar, for Krunch)
// and the buffer size: in the AUDIO popup and on the Settings screen.
Column {
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

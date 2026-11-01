pragma ComponentBehavior: Bound

import QtQuick
import Krate

// An audio track's panel (docs/DAW.md, D2) at design size 900 x 450: ARM,
// MONITOR, INPUT and where the input comes from, the input meter, the active
// take's waveform and the list of takes. REC is on the transport bar.
Item {
    id: panel

    property int rackDevice: -1 // set by the rack when it loads the panel
    readonly property int track: Tracks.trackOf(rackDevice)
    readonly property bool armed: Tracks.frame >= 0 && Tracks.param(track, 0) > 0.5
    readonly property bool recordingNow: Tracks.frame >= 0 && Tracks.isRecording(track)
    readonly property var takes: Tracks.revision >= 0 ? Tracks.takes(track) : []

    width: 900
    height: 450

    function mode(index) { return Tracks.frame >= 0 ? Math.round(Tracks.param(track, index) * 2) : 0 }

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
        y: 22
        text: "Audio " + (panel.track + 1)
        color: Theme.ink
        font.family: Theme.labelFont
        font.pixelSize: 44
        font.bold: true
        font.italic: true
    }
    PanelLabel {
        x: 40
        y: 80
        text: "AUDIO TRACK"
        font.pixelSize: 14
    }

    Row { // recording right now
        x: 250
        y: 40
        spacing: 10
        visible: panel.recordingNow
        Led { anchors.verticalCenter: parent.verticalCenter; width: 20; height: 20; on: true }
        PanelLabel { anchors.verticalCenter: parent.verticalCenter; text: "RECORDING"; font.pixelSize: 16 }
    }
    PanelLabel { // what REC will do
        x: 250
        y: 44
        visible: !panel.recordingNow && panel.armed
        text: Tracks.recording ? "ARMED: RECORDS WHEN YOU PLAY" : "ARMED: PRESS REC, THEN PLAY"
        font.pixelSize: 13
    }

    // ARM
    Column {
        x: 40
        y: 120
        spacing: 6
        PanelLabel { text: "ARM" }
        PanelButton {
            width: 90
            height: 50
            text: "Arm this track for recording"
            faceText: "ARM"
            tint: panel.armed ? Theme.ledOn : "transparent"
            onClicked: Tracks.setParam(panel.track, 0, panel.armed ? 0 : 1)
            LearnOverlay { anchors.fill: parent; paramKey: "arm"; round: false }
        }
    }

    // MONITOR and INPUT: three positions each
    Repeater {
        model: [
            { label: "MONITOR", param: 1, x: 170, faces: ["OFF", "AUTO", "ON"],
              names: ["Don't hear the input", "Hear the input while armed", "Always hear the input"] },
            { label: "INPUT", param: 2, x: 420, faces: ["STEREO", "IN 1", "IN 2"],
              names: ["Record both inputs", "Record input 1 (mono)", "Record input 2 (mono)"] }
        ]
        Column {
            id: selector
            required property var modelData
            x: modelData.x
            y: 120
            spacing: 6
            PanelLabel { text: selector.modelData.label }
            Row {
                spacing: 6
                Repeater {
                    model: 3
                    PanelButton {
                        required property int index
                        width: 74
                        height: 50
                        text: selector.modelData.names[index]
                        faceText: selector.modelData.faces[index]
                        tint: panel.mode(selector.modelData.param) === index ? Theme.labelOrange : "transparent"
                        onClicked: Tracks.setParam(panel.track, selector.modelData.param, index / 2)
                    }
                }
            }
        }
    }

    // Where the input comes from: the interface, or another device (the mixer's input selector)
    Column {
        x: 670
        y: 120
        spacing: 6
        PanelLabel { text: "SOURCE" }
        Row {
            spacing: 6
            PanelButton {
                width: 34
                height: 50
                text: "Previous input source"
                faceText: "◂"
                onClicked: Rack.stepInputSource(panel.rackDevice, -1)
            }
            Rectangle {
                width: 120
                height: 50
                radius: 4
                color: Theme.lcdBg
                border.color: Theme.deckLine
                Text {
                    anchors.centerIn: parent
                    width: parent.width - 8
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                    text: Rack.mixerRevision >= 0 ? Rack.inputSourceName(panel.rackDevice) : ""
                    color: Theme.lcdInk
                    font.family: Theme.labelFont
                    font.pixelSize: 15
                    font.bold: true
                }
            }
            PanelButton {
                width: 34
                height: 50
                text: "Next input source"
                faceText: "▸"
                onClicked: Rack.stepInputSource(panel.rackDevice, 1)
            }
        }
    }

    // The input meter
    PanelLabel { x: 40; y: 216; text: "IN"; font.pixelSize: 12 }
    Rectangle {
        x: 70
        y: 216
        width: 790
        height: 14
        radius: 3
        color: Theme.ledOff
        Rectangle {
            width: parent.width * Math.max(0, Math.min(1, Tracks.frame >= 0 ? Tracks.inputLevel(panel.track) : 0))
            height: parent.height
            radius: 3
            color: width > parent.width * 0.9 ? Theme.ledOn : Theme.labelOrange
        }
    }

    // The active take's waveform
    PanelLabel { x: 40; y: 246; text: "TAKE"; font.pixelSize: 12 }
    Rectangle {
        id: wave
        x: 40
        y: 266
        width: 520
        height: 160
        radius: 4
        color: Theme.band
        border.color: Theme.deckLine

        Canvas {
            id: canvas
            anchors.fill: parent
            anchors.margins: 6
            readonly property var peaks: Tracks.revision >= 0 ? Tracks.peaks(panel.track) : []
            onPeaksChanged: requestPaint()
            onPaint: {
                const ctx = getContext("2d");
                ctx.clearRect(0, 0, width, height);
                const n = peaks.length;
                if (n === 0)
                    return;
                ctx.fillStyle = Theme.labelOrange;
                const mid = height / 2;
                for (let x = 0; x < width; ++x) {
                    const p = peaks[Math.min(n - 1, Math.floor(x * n / width))];
                    const h = Math.max(1, p * mid);
                    ctx.fillRect(x, mid - h, 1, 2 * h);
                }
            }
        }
        Text {
            anchors.centerIn: parent
            visible: canvas.peaks.length === 0
            width: parent.width - 40
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: panel.takes.length === 0 ? "No takes yet: ARM, press REC, then PLAY." : "No take playing: pick one from the list."
            color: Theme.inkInverse
            opacity: 0.7
            font.family: Theme.labelFont
            font.pixelSize: 15
        }
    }

    // The takes, newest last: click one to play it (again to play none)
    PanelLabel { x: 590; y: 246; text: "TAKES (" + panel.takes.length + ")"; font.pixelSize: 12 }
    Rectangle {
        x: 590
        y: 266
        width: 270
        height: 160
        radius: 4
        color: Theme.lcdBg
        border.color: Theme.deckLine
        clip: true

        ListView {
            id: takeList
            anchors.fill: parent
            anchors.margins: 4
            model: panel.takes
            spacing: 2
            onCountChanged: positionViewAtEnd()

            delegate: Rectangle {
                id: takeRow
                required property int index
                required property var modelData
                property bool confirming: false
                width: takeList.width
                height: 28
                radius: 3
                color: modelData.active ? Theme.accentUi : "transparent"

                MouseArea {
                    anchors.fill: parent
                    onClicked: Tracks.setActiveTake(panel.track, takeRow.modelData.active ? -1 : takeRow.index)
                }
                Text {
                    x: 8
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - 90
                    elide: Text.ElideRight
                    text: takeRow.modelData.name.replace(/^take-/, "")
                    color: takeRow.modelData.active ? Theme.inkInverse : Theme.lcdInk
                    font.family: "monospace"
                    font.pixelSize: 12
                }
                Text {
                    anchors.right: deleteButton.left
                    anchors.rightMargin: 6
                    anchors.verticalCenter: parent.verticalCenter
                    text: takeRow.modelData.seconds.toFixed(1) + " s" + (takeRow.modelData.mono ? " M" : "")
                    color: takeRow.modelData.active ? Theme.inkInverse : Theme.lcdInk
                    font.family: "monospace"
                    font.pixelSize: 12
                }
                Rectangle { // delete: twice, the file goes
                    id: deleteButton
                    anchors.right: parent.right
                    anchors.rightMargin: 2
                    anchors.verticalCenter: parent.verticalCenter
                    width: takeRow.confirming ? 52 : 24
                    height: 22
                    radius: 3
                    color: takeRow.confirming ? Theme.ledOn : Theme.buttonDark
                    Text {
                        anchors.centerIn: parent
                        text: takeRow.confirming ? "SURE?" : "✕"
                        color: Theme.inkInverse
                        font.family: Theme.labelFont
                        font.pixelSize: 11
                        font.bold: true
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            if (takeRow.confirming)
                                Tracks.deleteTake(panel.track, takeRow.index);
                            else
                                takeRow.confirming = true;
                        }
                    }
                    Timer {
                        running: takeRow.confirming
                        interval: 3000
                        onTriggered: takeRow.confirming = false
                    }
                    Accessible.role: Accessible.Button
                    Accessible.name: "Delete take " + takeRow.modelData.name
                }
            }
        }
    }
}

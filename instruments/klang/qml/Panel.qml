pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Dialogs
import Krate

// Klang's panel at design size 1600 x 1100: the 16 pads and their samples
// (load, slice), the edited pad's controls with its waveform, the patterns
// with the step grid and the trance gate's steps, Klang's own knobs.
Item {
    id: panel

    width: 1600
    height: 1100

    readonly property color chassis: "#17181A"
    readonly property color chassisEdge: "#060607"
    readonly property color deck: "#222326"
    readonly property color rust: "#C4522A"
    readonly property color hot: "#FF7A45"
    readonly property color silk: "#E6E2DA"
    readonly property color dim: "#8A8780"
    readonly property color cell: "#2E2F33"

    readonly property int pad: Klang.editedPad

    // ---- Controls bound to Klang's parameters by key; all MIDI-learnable.

    component ParamKnob: Knob {
        id: paramKnob
        required property string key
        name: Klang.label(key)
        detents: Klang.detents(key)
        value: Klang.revision >= 0 ? Klang.value(key) : 0
        defaultValue: Klang.defaultValue(key)
        displayText: Klang.displayText(key, value)
        onMoved: v => Klang.setValue(key, v)
        capColor: "#2A2B2F"
        capShadeColor: "#0E0F11"
        capBorderColor: "#050506"
        pointerColor: panel.hot
        Accessible.description: displayText

        LearnOverlay {
            anchors.fill: parent
            paramKey: paramKnob.key
        }
    }

    // A knob with its label above and its value under it.
    component LabeledKnob: Item {
        id: labeled
        required property string key
        required property string label
        property real size: 44
        width: 76
        height: size + 38
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: labeled.label
            color: panel.silk
            font.family: Theme.labelFont
            font.pixelSize: 11
            font.bold: true
        }
        ParamKnob {
            id: control
            key: labeled.key
            y: 16
            anchors.horizontalCenter: parent.horizontalCenter
            width: labeled.size
            height: labeled.size
        }
        Text {
            y: 18 + labeled.size
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            text: control.displayText
            color: panel.dim
            font.family: Theme.labelFont
            font.pixelSize: 10
        }
    }

    component SectionTitle: Text {
        color: panel.rust
        font.family: Theme.labelFont
        font.pixelSize: 13
        font.bold: true
        font.letterSpacing: 2
    }

    component SmallButton: PanelButton {
        width: 52
        height: 28
        dark: true
    }

    // Chassis
    Rectangle {
        anchors.fill: parent
        radius: 8
        color: panel.chassis
        border.color: panel.chassisEdge
        border.width: 3
    }

    // Branding and utilities
    Text {
        x: 40
        y: 26
        text: "Krate"
        color: panel.rust
        font.family: Theme.labelFont
        font.pixelSize: 34
        font.bold: true
        font.italic: true
    }
    PanelButton { // the manual
        x: 44
        y: 76
        width: 64
        height: 26
        dark: true
        faceText: "HELP"
        text: "Klang's manual: what every control does and how to use it"
        onClicked: manual.open()
    }
    ManualView {
        id: manual
        title: "Klang"
        source: "qrc:/qt/qml/Klang/docs/MANUAL.md"
    }
    Text {
        id: wordmark
        anchors.right: parent.right
        anchors.rightMargin: 40
        y: 14
        text: "KLANG"
        color: panel.silk
        font.family: Theme.labelFont
        font.pixelSize: 64
        font.weight: Font.Black
        font.letterSpacing: 14
    }
    Text {
        anchors.right: wordmark.right
        y: 86
        text: "SAMPLER"
        color: panel.dim
        font.family: Theme.labelFont
        font.pixelSize: 16
        font.letterSpacing: 8
    }
    Row {
        x: 560
        y: 34
        spacing: 12
        Repeater {
            model: ["UNMAP", "AUDIO", "LEARN"]

            PanelButton {
                id: utility
                required property string modelData
                width: 64
                height: 30
                dark: true
                faceText: modelData
                text: modelData === "AUDIO" ? "Audio: reconnect" : modelData === "LEARN" ? "MIDI Learn" : "Clear all MIDI mappings"
                hasLed: modelData !== "UNMAP"
                ledOn: modelData === "AUDIO" ? Host.audioConnected : MidiLearn.active
                onClicked: {
                    if (modelData === "AUDIO")
                        Host.connectAudio()
                    else if (modelData === "LEARN")
                        MidiLearn.active = !MidiLearn.active
                    else
                        MidiLearn.clearAll()
                }
            }
        }
        Column {
            spacing: 4
            Text {
                width: 220
                elide: Text.ElideRight
                text: Host.audioStatus
                color: Host.audioConnected ? panel.dim : Theme.ledOn
                font.family: Theme.labelFont
                font.pixelSize: 11
            }
            Rectangle {
                width: 220
                height: 8
                radius: 2
                color: Theme.ledOff
                Rectangle {
                    width: parent.width * Host.outputLevel
                    height: parent.height
                    radius: 2
                    color: Theme.ledOn
                }
            }
        }
    }

    // ---- The pads

    Rectangle {
        x: 24
        y: 120
        width: 500
        height: 500
        radius: 4
        color: panel.deck
    }
    SectionTitle { x: 40; y: 128; text: "PADS" }
    PanelButton {
        x: 404
        y: 124
        width: 104
        height: 22
        dark: true
        faceText: "STARTER KIT"
        text: "Put the starter kit (Klang's own sounds) back on the pads"
        onClicked: Pads.installStarterKit()
    }
    Grid {
        x: 40
        y: 152
        columns: 4
        spacing: 10
        Repeater {
            model: 16

            Rectangle {
                id: padButton
                required property int index
                // Pad 1 at the bottom left, as on a pad controller.
                readonly property int number: (3 - Math.floor(index / 4)) * 4 + index % 4
                readonly property bool lit: Klang.lights >= 0 && Klang.sounding(number)
                readonly property bool edited: panel.pad === number
                width: 107
                height: 92
                radius: 6
                color: lit ? Qt.lighter(panel.rust, 1.3) : pressArea.pressed ? "#3A3B40" : "#2B2C30"
                border.color: edited ? panel.hot : "#0B0B0C"
                border.width: edited ? 3 : 2

                Accessible.role: Accessible.Button
                Accessible.name: "Pad " + (number + 1) + (Pads.revision >= 0 && Pads.name(number) ? ": " + Pads.name(number) : "")

                Text {
                    x: 8
                    y: 6
                    text: padButton.number + 1
                    color: panel.silk
                    font.family: Theme.labelFont
                    font.pixelSize: 18
                    font.bold: true
                }
                Rectangle { // used in the pattern shown
                    visible: Patterns.revision >= 0 && Patterns.padUsed(padButton.number)
                    x: parent.width - 16
                    y: 10
                    width: 8
                    height: 8
                    radius: 4
                    color: panel.hot
                }
                Text {
                    x: 8
                    y: parent.height - 24
                    width: parent.width - 16
                    elide: Text.ElideMiddle
                    text: Pads.revision >= 0 ? (Pads.name(padButton.number) || "empty") : ""
                    color: Pads.revision >= 0 && Pads.name(padButton.number) ? panel.silk : panel.dim
                    font.family: Theme.labelFont
                    font.pixelSize: 11
                }
                MouseArea {
                    id: pressArea
                    anchors.fill: parent
                    onPressed: Klang.pressPad(padButton.number)
                    onReleased: Klang.releasePad(padButton.number)
                }
                DropArea { // a file dropped on a pad loads onto it
                    anchors.fill: parent
                    onDropped: drop => {
                        if (drop.hasUrls)
                            Pads.loadUrls(padButton.number, drop.urls)
                    }
                }
                LearnOverlay {
                    anchors.fill: parent
                    paramKey: "pad" + (padButton.number + 1)
                    round: false
                }
            }
        }
    }

    // Samples: load, step through the library, clear; slicing
    FileDialog {
        id: loadDialog
        title: "Load samples onto pad " + (panel.pad + 1) + " (and the pads after it)"
        fileMode: FileDialog.OpenFiles
        nameFilters: ["Audio files (*.wav *.WAV *.mp3 *.MP3)"]
        onAccepted: Pads.loadUrls(panel.pad, selectedFiles)
    }
    Row {
        x: 40
        y: 548
        spacing: 6
        SmallButton { width: 64; faceText: "LOAD"; text: "Load WAV or MP3 files onto the pad"; onClicked: loadDialog.open() }
        SmallButton { width: 30; faceText: "◂"; text: "Previous sample in the library"; onClicked: Pads.step(panel.pad, -1) }
        SmallButton { width: 30; faceText: "▸"; text: "Next sample in the library"; onClicked: Pads.step(panel.pad, 1) }
        SmallButton { width: 58; faceText: "CLEAR"; text: "No sample on the pad"; onClicked: Pads.clear(panel.pad) }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "SLICE"
            color: panel.rust
            font.family: Theme.labelFont
            font.pixelSize: 12
            font.bold: true
        }
        Repeater {
            model: [4, 8, 16]
            SmallButton {
                required property int modelData
                width: 36
                faceText: String(modelData)
                text: "Slice the pad's sample into " + modelData + " pads"
                onClicked: Pads.slice(panel.pad, modelData)
            }
        }
        SmallButton { width: 52; faceText: "HITS"; text: "Slice the pad's sample at its hits"; onClicked: Pads.sliceAtHits(panel.pad) }
    }
    Text {
        x: 40
        y: 586
        width: 470
        elide: Text.ElideRight
        text: Pads.message
        color: panel.dim
        font.family: Theme.labelFont
        font.pixelSize: 11
    }
    DropArea { // files dropped elsewhere on the panel: onto the edited pad
        x: 540
        y: 120
        width: 1036
        height: 500
        onDropped: drop => {
            if (drop.hasUrls)
                Pads.loadUrls(panel.pad, drop.urls)
        }
    }

    // ---- The edited pad

    Rectangle {
        x: 540
        y: 120
        width: 1036
        height: 500
        radius: 4
        color: panel.deck
    }
    SectionTitle {
        x: 556
        y: 128
        text: "PAD " + (panel.pad + 1) + (Pads.revision >= 0 && Pads.name(panel.pad) ? "  ·  " + Pads.name(panel.pad) : "  ·  empty")
                  + (Pads.revision >= 0 && Pads.seconds(panel.pad) > 0 ? "  (" + Pads.seconds(panel.pad).toFixed(2) + " s)" : "")
    }
    Rectangle { // the waveform, the region played lit
        id: wave
        x: 556
        y: 152
        width: 1004
        height: 96
        radius: 3
        color: "#0E0F11"
        readonly property var peaks: Pads.revision >= 0 ? Pads.waveform(panel.pad, 250) : []
        readonly property real start: Klang.revision >= 0 ? Klang.value("start") : 0
        readonly property real end: Klang.revision >= 0 ? Klang.value("end") : 1

        Rectangle {
            x: wave.width * Math.min(wave.start, wave.end)
            width: wave.width * Math.max(0, wave.end - wave.start)
            height: parent.height
            color: "#2A1A14"
        }
        Repeater {
            model: wave.peaks.length
            Rectangle {
                required property int index
                readonly property real at: index / wave.peaks.length
                x: index * wave.width / wave.peaks.length
                width: Math.max(1, wave.width / wave.peaks.length - 1)
                height: Math.max(1, wave.peaks[index] * (wave.height - 8))
                y: (wave.height - height) / 2
                color: at >= wave.start && at < wave.end ? panel.rust : "#4A4B50"
            }
        }
        Text {
            visible: wave.peaks.length === 0
            anchors.centerIn: parent
            text: "Drop a WAV or MP3 here or on a pad, or press LOAD"
            color: panel.dim
            font.family: Theme.labelFont
            font.pixelSize: 13
        }
    }

    // Three rows of the edited pad's controls
    Repeater {
        model: [
            { title: "SAMPLE", y: 262, keys: [["pad_level", "LEVEL"], ["pad_pan", "PAN"], ["tune", "TUNE"], ["fine", "FINE"],
                                               ["start", "START"], ["end", "END"], ["attack", "ATTACK"], ["decay", "DECAY"],
                                               ["mode", "MODE"], ["sync", "SYNC"], ["reverse", "REVERSE"], ["choke", "CHOKE"]] },
            { title: "TONE", y: 380, keys: [["filter", "FILTER"], ["cutoff", "CUTOFF"], ["res", "RES"], ["filter_env", "ENV"],
                                             ["drive", "DRIVE"], ["bits", "BITS"], ["downsample", "RATE"], ["ring", "RING"],
                                             ["ring_freq", "FREQ"], ["gate", "GATE"], ["delay_send", "DELAY"], ["reverb_send", "REVERB"]] }
        ]

        Item {
            id: row
            required property var modelData
            x: 556
            y: modelData.y
            SectionTitle { y: 2; text: row.modelData.title; font.pixelSize: 11 }
            Row {
                x: 64
                spacing: 4
                Repeater {
                    model: row.modelData.keys
                    LabeledKnob {
                        required property var modelData
                        key: modelData[0]
                        label: modelData[1]
                    }
                }
            }
        }
    }
    Text {
        x: 556
        y: 500
        width: 1004
        wrapMode: Text.WordWrap
        text: "Click a pad to play it and edit it. MODE: One shot plays to the end; Gate plays while held; Loop loops until let go or stopped. "
              + "SYNC fits the region to that many beats. GATE: the trance gate below chops this pad. CHOKE: pads in a group cut each other. "
              + "Keys 36–51 (C2–D#3) play the pads; with KEYS on Chromatic every key plays the edited pad at its pitch."
        color: panel.dim
        font.family: Theme.labelFont
        font.pixelSize: 11
    }

    // ---- Patterns

    Rectangle {
        x: 24
        y: 636
        width: panel.width - 48
        height: panel.height - 636 - 24
        radius: 4
        color: panel.deck
    }
    SectionTitle { x: 40; y: 644; text: "PATTERN " + String.fromCharCode(65 + Patterns.current) }

    Grid { // A-P: choose (and cue) a pattern
        x: 40
        y: 670
        columns: 4
        spacing: 6
        Repeater {
            model: 16
            PanelButton {
                id: patternButton
                required property int index
                width: 58
                height: 32
                faceText: String.fromCharCode(65 + index)
                text: "Pattern " + faceText
                readonly property bool playingNow: Klang.playing && Patterns.playing === index
                tint: Patterns.current === index ? panel.hot : playingNow ? "#7A3A22"
                    : Patterns.revision >= 0 && Patterns.used(index) ? "#5A5B60" : "#3A3B40"
                onClicked: Patterns.current = index

                LearnOverlay {
                    anchors.fill: parent
                    paramKey: "pattern" + (patternButton.index + 1)
                    round: false
                }
            }
        }
    }
    Text {
        x: 40
        y: 820
        visible: Klang.canRun
        text: "START"
        color: panel.silk
        font.family: Theme.labelFont
        font.pixelSize: 11
        font.bold: true
    }
    PanelButton {
        id: startButton
        x: 40
        y: 836
        width: 120
        height: 36
        text: "Start or stop"
        visible: Klang.canRun
        hasLed: true
        ledOn: Klang.playing
        onClicked: Klang.playing = !Klang.playing

        LearnOverlay {
            anchors.fill: parent
            paramKey: "start"
            round: false
        }
    }
    Column {
        x: 176
        y: 830
        spacing: 2
        Text { text: "LENGTH"; color: panel.silk; font.family: Theme.labelFont; font.pixelSize: 11; font.bold: true }
        Row {
            spacing: 4
            SmallButton { width: 26; faceText: "−"; text: "Shorter"; onClicked: Patterns.length = Patterns.length - 1 }
            Text {
                width: 30
                horizontalAlignment: Text.AlignHCenter
                anchors.verticalCenter: parent.verticalCenter
                text: Patterns.length
                color: panel.hot
                font.family: "monospace"
                font.pixelSize: 16
                font.bold: true
            }
            SmallButton { width: 26; faceText: "+"; text: "Longer"; onClicked: Patterns.length = Patterns.length + 1 }
        }
    }
    Row { // pages of 16 steps
        x: 40
        y: 884
        spacing: 6
        Repeater {
            model: 4
            PanelButton {
                required property int index
                width: 58
                height: 28
                faceText: (index * 16 + 1) + "–" + (index * 16 + 16)
                text: "Steps " + faceText
                enabled: index * 16 < Patterns.length
                opacity: enabled ? 1.0 : 0.35
                tint: Patterns.page === index ? panel.hot : "#3A3B40"
                onClicked: Patterns.page = index
            }
        }
    }
    Grid { // what a click on a step does
        x: 40
        y: 924
        columns: 3
        spacing: 6
        Repeater {
            model: ["STEP", "ACCENT", "ROLL", "REVERSE", "PITCH +", "PITCH −"]
            PanelButton {
                required property int index
                required property string modelData
                width: 80
                height: 28
                faceText: modelData
                text: "Clicking a step: " + modelData
                tint: Patterns.tool === index ? panel.hot : "#3A3B40"
                onClicked: Patterns.tool = index
            }
        }
    }
    Row {
        x: 40
        y: 1000
        spacing: 6
        SmallButton { width: 80; faceText: "COPY"; text: "Copy this pattern"; onClicked: Patterns.copy() }
        SmallButton { width: 80; faceText: "PASTE"; text: "Paste over this pattern"; enabled: Patterns.hasCopy; onClicked: Patterns.paste() }
        SmallButton { width: 80; faceText: "CLEAR"; text: "Clear this pattern"; onClicked: Patterns.clear() }
    }
    Text {
        x: 40
        y: 1040
        width: 270
        wrapMode: Text.WordWrap
        text: "A pattern chosen while playing starts at the end of the pass."
        color: panel.dim
        font.family: Theme.labelFont
        font.pixelSize: 10
    }

    // The step grid: a row per pad (pad 16 at the top), the page's 16 steps; the gate's steps under it
    readonly property real gridX: 380
    readonly property real gridY: 668
    readonly property real cellW: 58
    readonly property real cellH: 21
    Repeater {
        model: 16
        Text {
            required property int index
            readonly property int number: 15 - index
            x: panel.gridX - 52
            y: panel.gridY + index * panel.cellH + 3
            width: 46
            horizontalAlignment: Text.AlignRight
            elide: Text.ElideRight
            text: (number + 1)
            color: panel.pad === number ? panel.hot : panel.silk
            font.family: Theme.labelFont
            font.pixelSize: 11
            font.bold: true
            MouseArea {
                anchors.fill: parent
                onClicked: Klang.editedPad = parent.number
            }
        }
    }
    Repeater {
        model: 16 * 16
        Rectangle {
            id: stepCell
            required property int index
            readonly property int pad: 15 - Math.floor(index / 16)
            readonly property int column: index % 16
            readonly property int step: Patterns.page * 16 + column
            readonly property bool inPattern: step < Patterns.length
            readonly property bool on: Patterns.revision >= 0 && Patterns.stepOn(pad, step)
            readonly property bool accent: on && Patterns.stepAccent(pad, step)
            readonly property int repeat: on ? Patterns.stepRepeat(pad, step) : 1
            readonly property bool reversed: on && Patterns.stepReverse(pad, step)
            readonly property int pitch: on ? Patterns.stepPitch(pad, step) : 0
            readonly property bool playingHere: Patterns.playingStep === step && Patterns.playing === Patterns.current
            x: panel.gridX + column * panel.cellW + Math.floor(column / 4) * 4
            y: panel.gridY + Math.floor(index / 16) * panel.cellH
            width: panel.cellW - 3
            height: panel.cellH - 3
            radius: 2
            opacity: inPattern ? 1.0 : 0.25
            color: accent ? panel.hot : on ? panel.rust : playingHere ? "#4A4B50" : panel.cell

            Text {
                anchors.centerIn: parent
                visible: stepCell.on
                text: (stepCell.reversed ? "◂ " : "") + (stepCell.repeat > 1 ? "×" + stepCell.repeat + " " : "")
                      + (stepCell.pitch !== 0 ? (stepCell.pitch > 0 ? "+" : "") + stepCell.pitch : "")
                color: "#111"
                font.family: Theme.labelFont
                font.pixelSize: 10
                font.bold: true
            }
            MouseArea {
                anchors.fill: parent
                enabled: stepCell.inPattern
                onClicked: Patterns.pressStep(stepCell.pad, stepCell.step)
            }
        }
    }
    Rectangle { // the playhead
        visible: Klang.playing && Patterns.playing === Patterns.current && Patterns.playingStep >= Patterns.page * 16
                 && Patterns.playingStep < Patterns.page * 16 + 16
        readonly property int column: Patterns.playingStep - Patterns.page * 16
        x: panel.gridX + column * panel.cellW + Math.floor(column / 4) * 4 - 2
        y: panel.gridY - 4
        width: panel.cellW + 1
        height: 16 * panel.cellH + 4
        color: "transparent"
        border.color: panel.silk
        border.width: 2
        radius: 3
    }
    Text {
        x: panel.gridX - 52
        y: panel.gridY + 16 * panel.cellH + 10
        width: 46
        horizontalAlignment: Text.AlignRight
        text: "GATE"
        color: panel.rust
        font.family: Theme.labelFont
        font.pixelSize: 11
        font.bold: true
    }
    Repeater { // the trance gate's 16 steps for this pattern
        model: 16
        Rectangle {
            id: gateCell
            required property int index
            readonly property bool open: Patterns.revision >= 0 && Patterns.gateStep(index)
            x: panel.gridX + index * panel.cellW + Math.floor(index / 4) * 4
            y: panel.gridY + 16 * panel.cellH + 8
            width: panel.cellW - 3
            height: 22
            radius: 2
            color: open ? "#D9C9A8" : panel.cell
            Accessible.role: Accessible.Button
            Accessible.name: "Gate step " + (index + 1) + (open ? " open" : " closed")
            MouseArea {
                anchors.fill: parent
                onClicked: Patterns.toggleGateStep(gateCell.index)
            }
        }
    }
    Text {
        x: panel.gridX
        y: panel.gridY + 16 * panel.cellH + 38
        width: 16 * panel.cellW
        text: "Click a step to do what the tool says. GATE: the steps the trance gate lets through, for the pads with GATE on."
        color: panel.dim
        font.family: Theme.labelFont
        font.pixelSize: 10
    }

    // ---- Klang's own: groove, gate, effects, output
    Grid {
        x: 1340
        y: 650
        columns: 3
        rowSpacing: 4
        columnSpacing: 0
        Repeater {
            model: [["swing", "SWING"], ["key_mode", "KEYS"], ["volume", "VOLUME"],
                    ["gate_rate", "GATE RATE"], ["gate_depth", "DEPTH"], ["gate_smooth", "SMOOTH"],
                    ["delay_level", "DELAY"], ["delay_time", "TIME"], ["delay_fb", "FEEDBACK"],
                    ["reverb_level", "REVERB"], ["reverb_size", "SIZE"], ["tempo", "TEMPO"]]
            LabeledKnob {
                required property var modelData
                key: modelData[0]
                label: modelData[1]
                size: 40
                visible: modelData[0] !== "tempo" || Klang.ownsTransport
            }
        }
    }

    // Keyboard: Space run/stop, L learn, Esc leaves learn, ← → the edited pad.
    Keys.onSpacePressed: Klang.playing = !Klang.playing
    Keys.onPressed: event => {
        if (event.modifiers !== Qt.NoModifier && event.modifiers !== Qt.KeypadModifier)
            return
        switch (event.key) {
        case Qt.Key_L: MidiLearn.active = !MidiLearn.active; break
        case Qt.Key_Left: Klang.editedPad = (Klang.editedPad + 15) % 16; break
        case Qt.Key_Right: Klang.editedPad = (Klang.editedPad + 1) % 16; break
        case Qt.Key_Escape:
            if (!MidiLearn.active)
                return
            MidiLearn.active = false
            break
        default:
            return
        }
        event.accepted = true
    }
}

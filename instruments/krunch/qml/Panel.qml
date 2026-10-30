pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Krate

// Krunch's panel at design size 1400 x 990: grille cloth with the logo and
// where the guitar comes from; below, the control plate: input, the four
// channel buttons, the power amp, level; then the playing channel's controls
// and its cabinet (IRs A and B from the IR library, and their mix). Under the
// amp, the pedalboard.
Item {
    id: panel

    width: 1400
    height: 990

    readonly property int input: Krunch.revision >= 0 && Krunch.value("input") >= 0.5 ? 1 : 0
    readonly property int channel: Krunch.revision >= 0 ? Math.round(Krunch.value("channel") * 3) : 0
    readonly property string inputPort: Host.inputPorts[input] ?? ""
    readonly property bool inputConnected: Host.inputsConnected[input] ?? false
    readonly property color cream: "#E9DFC4"
    readonly property var voiceNames: ["CLEAN", "CRUNCH", "HEAVY", "LEAD"]
    readonly property var channelColors: ["#5FB0E8", "#F2C230", "#F08A2B", "#E2412E"]

    function chKey(name) { return "ch" + (channel + 1) + "_" + name }
    function voiceOf(c) { return Krunch.revision >= 0 ? Math.round(Krunch.value("ch" + (c + 1) + "_voice") * 3) : 0 }

    component Meter: Rectangle { // vertical, -60..0 dB
        property real level: 0
        width: 16
        height: 120
        radius: 3
        color: "#0C0C0D"
        border.color: "#3A3B3E"
        Rectangle {
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 2
            x: 2
            width: parent.width - 4
            height: (parent.height - 4) * parent.level
            radius: 2
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#FF3A2A" }
                GradientStop { position: 0.15; color: "#F2C230" }
                GradientStop { position: 0.4; color: "#5FD35A" }
                GradientStop { position: 1.0; color: "#2F9A35" }
            }
        }
    }

    component Label: Text {
        color: panel.cream
        font.family: Theme.labelFont
        font.pixelSize: 14
        font.bold: true
        horizontalAlignment: Text.AlignHCenter
    }

    // A knob with its label above and its value under it, centred on cx.
    component AmpKnob: Item {
        id: ampKnob
        required property string key
        required property string label
        required property real cx
        property real size: 84
        property int detents: 0
        x: cx - 70
        width: 140
        height: size + 54

        Label { width: parent.width; text: ampKnob.label }
        Knob {
            x: (parent.width - width) / 2
            y: 22
            width: ampKnob.size
            height: ampKnob.size
            name: Krunch.label(ampKnob.key)
            detents: ampKnob.detents
            value: Krunch.revision >= 0 ? Krunch.value(ampKnob.key) : 0
            defaultValue: Krunch.defaultValue(ampKnob.key)
            displayText: Krunch.displayText(ampKnob.key, value)
            onMoved: v => Krunch.setValue(ampKnob.key, v)

            LearnOverlay { anchors.fill: parent; paramKey: ampKnob.key }
        }
        Label {
            y: ampKnob.size + 28
            width: parent.width
            text: Krunch.displayText(ampKnob.key, Krunch.revision >= 0 ? Krunch.value(ampKnob.key) : 0)
            font.pixelSize: 12
            font.bold: false
        }
    }

    Rectangle { // the cabinet: black tolex
        width: parent.width
        height: 700
        radius: 10
        color: "#161617"
        border.color: "#050505"
        border.width: 3
    }

    Rectangle { // grille cloth
        x: 24
        y: 18
        width: parent.width - 48
        height: 130
        radius: 6
        color: "#2B2A27"
        border.color: "#0A0A0A"
        border.width: 2
        clip: true

        Repeater { // the weave
            model: 80
            Rectangle {
                required property int index
                x: index * 19 - 60
                y: -20
                width: 3
                height: 180
                rotation: 30
                color: "#33322E"
            }
        }

        Text {
            x: 40
            anchors.verticalCenter: parent.verticalCenter
            text: "Krunch"
            color: panel.cream
            style: Text.Raised
            styleColor: "#000"
            font.family: Theme.labelFont
            font.pixelSize: 80
            font.bold: true
            font.italic: true
        }

        // The preset: its name (* when the amp has changed since), the list,
        // previous and next (learnable), SAVE over it, SAVE AS a new one.
        Row {
            x: 400
            anchors.verticalCenter: parent.verticalCenter
            spacing: 6

            Rectangle { // the name; a click opens the list
                width: 230
                height: 40
                radius: 4
                color: "#0F1410"
                border.color: "#3A3B3E"
                Text {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    verticalAlignment: Text.AlignVCenter
                    text: Presets.current === "" ? "NO PRESET" : Presets.current + (Presets.modified ? " *" : "")
                    color: Presets.current === "" ? "#6A7A6A" : "#9CFF9C"
                    font.family: "monospace"
                    font.pixelSize: 17
                    font.bold: true
                    elide: Text.ElideRight
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: presetList.open()
                }
                Accessible.role: Accessible.Button
                Accessible.name: "Preset: " + (Presets.current || "none") + (Presets.modified ? ", modified" : "") + ". Open the list"
            }
            Repeater {
                model: [{ face: "◂", by: -1, action: "presetPrevious", name: "Previous preset" },
                        { face: "▸", by: 1, action: "presetNext", name: "Next preset" }]
                PanelButton {
                    required property var modelData
                    width: 40
                    height: 40
                    dark: true
                    faceText: modelData.face
                    text: modelData.name
                    onClicked: Presets.step(modelData.by)
                    LearnOverlay { anchors.fill: parent; paramKey: parent.modelData.action; round: false }
                }
            }
            PanelButton {
                width: 64
                height: 40
                dark: true
                faceText: "SAVE"
                text: Presets.current === "" ? "Save the amp as a new preset" : "Save the amp over " + Presets.current
                tint: Presets.modified ? Theme.labelOrange : "transparent"
                onClicked: Presets.current === "" ? saveAsPopup.open() : Presets.save()
            }
            PanelButton {
                width: 84
                height: 40
                dark: true
                faceText: "SAVE AS…"
                text: "Save the amp as a new preset"
                onClicked: saveAsPopup.open()
            }
        }

        // Where the guitar comes from, and how long it takes to reach the speakers.
        Column {
            x: 900
            width: parent.width - 940
            anchors.verticalCenter: parent.verticalCenter
            spacing: 6

            Text {
                width: parent.width
                horizontalAlignment: Text.AlignRight
                text: panel.inputConnected ? "GUITAR  " + Host.portLabel(panel.inputPort)
                                           : panel.inputPort === "" ? "NO INPUT CHOSEN"
                                                                    : "WAITING FOR  " + Host.portLabel(panel.inputPort)
                color: panel.inputConnected ? panel.cream : Theme.labelOrange
                font.family: Theme.labelFont
                font.pixelSize: 17
                font.bold: true
                elide: Text.ElideLeft
            }
            Text {
                width: parent.width
                horizontalAlignment: Text.AlignRight
                visible: !panel.inputConnected
                text: panel.inputPort === ""
                      ? "Choose your audio interface's input under AUDIO in the top bar (as input " + (panel.input + 1) + ")."
                      : "Krunch connects as soon as that device appears. Is it plugged in?"
                color: "#B9B2A0"
                font.family: Theme.labelFont
                font.pixelSize: 13
                wrapMode: Text.WordWrap
            }
            Text {
                width: parent.width
                horizontalAlignment: Text.AlignRight
                text: Host.audioConnected
                      ? "DELAY  " + Host.latencyMs.toFixed(1) + " ms  ·  " + Host.bufferSize + " frames"
                        + (Host.latencyMs > 12 ? "  ·  set a smaller buffer under AUDIO" : "")
                      : "AUDIO OFFLINE"
                color: Host.latencyMs > 12 ? Theme.labelOrange : panel.cream
                font.family: Theme.labelFont
                font.pixelSize: 15
                font.bold: true
            }
        }
    }

    Rectangle { // control plate
        id: plate
        x: 24
        y: 164
        width: parent.width - 48
        height: 518
        radius: 6
        border.color: "#3A3B3E"
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#232325" }
            GradientStop { position: 1.0; color: "#0F0F10" }
        }

        // --- Top row: input, channels, power amp, level ---------------------------

        AmpKnob { y: 14; cx: 70; key: "input"; label: "INPUT"; size: 64; detents: 2 }

        Label { x: 128; y: 14; width: 40; text: "IN" }
        Meter { x: 140; y: 38; height: 96; level: Krunch.inputLevel }
        Led { x: 142; y: 142; width: 12; height: 12; on: Krunch.clipping }
        Label { x: 118; y: 158; width: 60; text: "CLIP"; font.pixelSize: 10 }

        Label { x: 210; y: 14; width: 440; text: "CHANNEL" }
        Repeater {
            model: 4
            Column {
                id: channelButton
                required property int index
                x: 210 + index * 112
                y: 40
                spacing: 6
                PanelButton {
                    width: 104
                    height: 56
                    dark: true
                    faceText: (channelButton.index + 1).toString()
                    text: "Channel " + (channelButton.index + 1)
                    hasLed: true
                    ledOn: panel.channel === channelButton.index
                    tint: panel.channel === channelButton.index ? panel.channelColors[channelButton.index] : "transparent"
                    onClicked: Krunch.setValue("channel", channelButton.index / 3)

                    LearnOverlay {
                        anchors.fill: parent
                        paramKey: "channel" + (channelButton.index + 1)
                        round: false
                    }
                }
                Label {
                    width: 104
                    text: panel.voiceNames[panel.voiceOf(channelButton.index)]
                    font.pixelSize: 12
                    color: panel.channel === channelButton.index ? panel.channelColors[channelButton.index] : "#8F897A"
                }
            }
        }

        Item { // the gate's light: shut while you're not playing
            x: 676
            y: 40
            width: 60
            height: 80
            Led { x: 24; y: 18; width: 12; height: 12; on: Krunch.gateClosed }
            Label { y: 40; width: parent.width; text: "GATE\nSHUT"; font.pixelSize: 10 }
        }

        Rectangle { // power amp section
            x: 760
            y: 8
            width: 410
            height: 176
            radius: 6
            color: "transparent"
            border.color: "#3A3B3E"
            Label { x: 10; y: 152; text: "POWER AMP"; font.pixelSize: 11; color: "#8F897A" }
        }
        AmpKnob { y: 14; cx: 830; key: "master"; label: "MASTER" }
        AmpKnob { y: 14; cx: 965; key: "sag"; label: "SAG" }
        AmpKnob { y: 14; cx: 1100; key: "resonance"; label: "RESONANCE" }

        AmpKnob { y: 14; cx: 1240; key: "level"; label: "LEVEL" }
        Label { x: 1296; y: 14; width: 40; text: "OUT" }
        Meter { x: 1308; y: 38; height: 116; level: Krunch.outputLevel }

        // --- The playing channel ----------------------------------------------------

        Rectangle {
            x: 14
            y: 196
            width: plate.width - 28
            height: 308
            radius: 6
            color: "#1A1A1C"
            border.color: panel.channelColors[panel.channel]
            border.width: 2

            Label {
                x: 16
                y: 10
                text: "CHANNEL " + (panel.channel + 1) + "  ·  " + panel.voiceNames[panel.voiceOf(panel.channel)]
                font.pixelSize: 12
                color: panel.channelColors[panel.channel]
            }
        }
        Repeater {
            model: [
                { name: "voice", label: "VOICE", detents: 4 },
                { name: "gain", label: "GAIN" },
                { name: "bass", label: "BASS" },
                { name: "mid", label: "MID" },
                { name: "treble", label: "TREBLE" },
                { name: "presence", label: "PRESENCE" },
                { name: "volume", label: "VOLUME" },
                { name: "gate", label: "GATE" }
            ]
            AmpKnob {
                required property var modelData
                required property int index
                y: 226
                cx: 96 + index * 142
                size: 96
                key: panel.chKey(modelData.name)
                label: modelData.label
                detents: modelData.detents ?? 0
            }
        }
        Column { // the channel's effects loop
            x: 1200
            y: 244
            spacing: 6
            PanelButton {
                width: 104
                height: 56
                dark: true
                faceText: "LOOP"
                text: "Effects loop for this channel"
                hasLed: true
                ledOn: Krunch.revision >= 0 && Krunch.value(panel.chKey("loop")) >= 0.5
                onClicked: Krunch.setValue(panel.chKey("loop"), ledOn ? 0 : 1)

                LearnOverlay { anchors.fill: parent; paramKey: "loop"; round: false }
            }
            Label { width: 104; text: "FX LOOP"; font.pixelSize: 12 }
            Label {
                width: 104
                readonly property int pedals: {
                    let n = 0
                    for (let i = 0; Pedals.revision >= 0 && i < Pedals.count; ++i)
                        if (Pedals.inLoop(i))
                            ++n
                    return n
                }
                text: pedals === 0 ? "no pedals in it" : pedals + (pedals === 1 ? " pedal" : " pedals")
                font.pixelSize: 10
                font.bold: false
                color: "#8F897A"
            }
        }

        // --- The playing channel's cabinet ------------------------------------------

        Label { x: 40; y: 400; text: "CABINET"; font.pixelSize: 13 }
        Repeater {
            model: 2
            Column {
                id: cabSlot
                required property int index
                readonly property bool isB: index === 1
                readonly property string irName: Cabinets.revision >= 0 ? Cabinets.irName(panel.channel, isB) : ""
                x: 40 + index * 470
                y: 422
                spacing: 4
                PanelButton {
                    width: 440
                    height: 40
                    dark: true
                    faceText: (cabSlot.isB ? "B   " : "A   ") + (cabSlot.irName !== "" ? cabSlot.irName : cabSlot.isB ? "—" : "Built-in speaker")
                    text: "Channel " + (panel.channel + 1) + " cabinet " + (cabSlot.isB ? "B" : "A") + ": choose an IR"
                    onClicked: {
                        irPicker.channel = panel.channel
                        irPicker.slotB = cabSlot.isB
                        irPicker.open()
                    }
                }
            }
        }
        AmpKnob { y: 380; cx: 1060; size: 64; key: panel.chKey("cab_mix"); label: "CAB MIX" }
        Text {
            x: 1140
            y: 420
            width: 190
            text: Cabinets.libraryCount + " IRs in the library"
            color: "#8F897A"
            font.family: Theme.labelFont
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }
    }

    Pedalboard {
        x: 0
        y: 712
        width: parent.width
        height: 278
    }

    // A new preset (or over one of that name): the whole amp, pedals and IRs included.
    Popup {
        id: saveAsPopup
        x: 400
        y: 110
        width: 420
        padding: 14
        modal: true
        onOpened: {
            nameField.text = Presets.current
            nameField.selectAll()
            nameField.forceActiveFocus()
        }
        background: Rectangle { radius: 6; color: "#232325"; border.color: "#5A5B5E" }
        Column {
            width: parent.width
            spacing: 10
            Label { text: "SAVE THE AMP AS"; horizontalAlignment: Text.AlignLeft }
            TextField {
                id: nameField
                width: parent.width
                placeholderText: "Preset name"
                onAccepted: if (Presets.saveAs(text)) saveAsPopup.close()
                Accessible.name: "Preset name"
            }
            Text {
                width: parent.width
                visible: Presets.names.indexOf(nameField.text.trim()) >= 0
                text: "There's a preset of that name: saving replaces it."
                color: Theme.labelOrange
                font.family: Theme.labelFont
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
            Row {
                spacing: 10
                PanelButton {
                    width: 90
                    height: 32
                    faceText: "SAVE"
                    text: "Save the preset"
                    onClicked: if (Presets.saveAs(nameField.text)) saveAsPopup.close()
                }
                PanelButton {
                    width: 90
                    height: 32
                    faceText: "CANCEL"
                    text: "Cancel"
                    onClicked: saveAsPopup.close()
                }
            }
        }
    }

    // The presets: a click loads one; rename or delete it.
    Popup {
        id: presetList
        x: 400
        y: 110
        width: 460
        height: Math.min(520, 90 + Math.max(1, Presets.names.length) * 44)
        padding: 14
        modal: true
        property string renaming: ""
        property string deleting: ""
        onClosed: {
            renaming = ""
            deleting = ""
        }
        background: Rectangle { radius: 6; color: "#232325"; border.color: "#5A5B5E" }
        Column {
            anchors.fill: parent
            spacing: 8
            Label { text: "PRESETS"; horizontalAlignment: Text.AlignLeft }
            Text {
                visible: Presets.names.length === 0
                width: parent.width
                text: "No presets yet: set the amp, the pedals and the cabinets up, then SAVE AS…"
                color: "#B9B2A0"
                font.family: Theme.labelFont
                font.pixelSize: 13
                wrapMode: Text.WordWrap
            }
            ListView {
                width: parent.width
                height: parent.height - 30
                clip: true
                model: Presets.names
                ScrollBar.vertical: ScrollBar {}
                delegate: Rectangle {
                    id: presetRow
                    required property string modelData
                    readonly property bool chosen: modelData === Presets.current
                    width: ListView.view.width - 12
                    height: 40
                    radius: 4
                    color: chosen ? "#3B4A5A" : (rowArea.containsMouse ? "#2E2F32" : "transparent")
                    MouseArea {
                        id: rowArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            Presets.load(presetRow.modelData)
                            presetList.close()
                        }
                    }
                    Text {
                        visible: presetList.renaming !== presetRow.modelData
                        x: 12
                        width: parent.width - 150
                        anchors.verticalCenter: parent.verticalCenter
                        text: presetRow.modelData
                        color: "#F2F2F2"
                        font.family: Theme.labelFont
                        font.pixelSize: 15
                        font.bold: presetRow.chosen
                        elide: Text.ElideRight
                    }
                    TextField {
                        visible: presetList.renaming === presetRow.modelData
                        x: 6
                        width: parent.width - 150
                        height: 32
                        anchors.verticalCenter: parent.verticalCenter
                        text: presetRow.modelData
                        onVisibleChanged: if (visible) { selectAll(); forceActiveFocus() }
                        onAccepted: {
                            if (Presets.rename(presetRow.modelData, text))
                                presetList.renaming = ""
                        }
                        Accessible.name: "New name for " + presetRow.modelData
                    }
                    Row {
                        anchors.right: parent.right
                        anchors.rightMargin: 6
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 6
                        PanelButton {
                            width: 70
                            height: 28
                            faceText: "RENAME"
                            text: "Rename " + presetRow.modelData
                            onClicked: presetList.renaming = presetRow.modelData
                        }
                        PanelButton {
                            width: 62
                            height: 28
                            faceText: presetList.deleting === presetRow.modelData ? "SURE?" : "DELETE"
                            text: "Delete " + presetRow.modelData
                            tint: presetList.deleting === presetRow.modelData ? Theme.labelOrange : "transparent"
                            onClicked: {
                                if (presetList.deleting === presetRow.modelData) {
                                    Presets.remove(presetRow.modelData)
                                    presetList.deleting = ""
                                } else {
                                    presetList.deleting = presetRow.modelData
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    IrPicker {
        id: irPicker
        x: (panel.width - width) / 2
        y: 40
    }

    DropArea { // IR files, folders or packs dropped on the amp go into the library
        anchors.fill: parent
        keys: ["text/uri-list"]
        onDropped: event => {
            Cabinets.importUrls(event.urls)
            event.accept()
        }
    }
}

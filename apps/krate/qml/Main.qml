pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Krate

// The Krate rack window: transport bar, instrument tabs (click = focus), the
// Mixer, Song and Seq tabs; below them the focused instrument's panel scaled
// to fit, the mixer screen, the song screen or the sequencer.
Window {
    id: root

    width: 1400
    height: 980
    minimumWidth: 900
    minimumHeight: 640
    visible: true
    title: "krate"
    color: Theme.desk

    Shortcut {
        sequence: "Space"
        onActivated: Rack.playing = !Rack.playing
    }
    Shortcut {
        sequence: "Tab"
        onActivated: Rack.focus = Rack.stepFocus(1)
    }
    Shortcut {
        sequence: "M"
        onActivated: Rack.mixerShown = !Rack.mixerShown
    }
    Shortcut {
        sequence: "S"
        onActivated: Rack.songShown = !Rack.songShown
    }
    Shortcut {
        sequence: "Q"
        onActivated: Rack.seqShown = !Rack.seqShown
    }
    Shortcut {
        sequence: "A"
        onActivated: Rack.arrangeShown = !Rack.arrangeShown
    }
    Shortcut {
        sequence: "L"
        onActivated: MidiLearn.active = !MidiLearn.active
    }
    Shortcut {
        sequence: "Escape"
        enabled: MidiLearn.active
        onActivated: MidiLearn.active = false
    }

    TransportBar {
        id: transportBar
        width: root.width
    }

    Row { // instrument tabs
        id: tabs
        y: transportBar.height + 10
        x: 16
        spacing: 8
        // Tabs fit the window: the screens' tabs keep their size, the instruments' share the rest.
        readonly property int screenTabWidth: instrumentTabs > 8 ? 100 : 112 // narrower with more in the rack
        // Only the instruments in the rack have tabs (docs/DAW.md, D1).
        readonly property int instrumentTabs: Math.max(1, Rack.panelComponents.filter((c, i) => c !== "" && Rack.present[i]).length)
        readonly property real tabWidth: Math.max(60, Math.min(150, (root.width - 32 - 3 * 40 - 16 - 4 * screenTabWidth
                                                                     - (instrumentTabs + 7) * spacing) / instrumentTabs))

        PanelButton { // focus previous (learnable)
            width: 40
            height: 40
            faceText: "◂"
            text: "Focus previous instrument"
            onClicked: Rack.focus = Rack.stepFocus(-1)
            LearnOverlay { anchors.fill: parent; paramKey: "rack:previous"; round: false }
        }

        Repeater {
            model: Rack.deviceNames

            Rectangle {
                id: tab
                required property int index
                required property string modelData
                readonly property bool focused: Rack.focus === index
                readonly property bool shown: focused && !Rack.mixerShown && !Rack.songShown && !Rack.seqShown && !Rack.arrangeShown

                // Devices without a panel (the backing track), or not in the rack, have no tab.
                visible: Rack.panelComponents[index] !== "" && Rack.present[index] === true
                width: tabs.tabWidth
                clip: true
                height: 40
                radius: 4
                color: shown ? Theme.silverLight : Theme.lowerDeck
                border.color: shown ? Theme.accentUi : Theme.deckLine
                border.width: shown ? 3 : 1

                Led {
                    x: 12
                    anchors.verticalCenter: parent.verticalCenter
                    on: tab.focused
                }
                Text {
                    x: 34
                    anchors.verticalCenter: parent.verticalCenter
                    text: tab.modelData
                    color: Theme.ink
                    font.family: Theme.labelFont
                    font.pixelSize: 18
                    font.bold: tab.focused
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        Rack.focus = tab.index
                        Rack.mixerShown = false
                    }
                }
                LearnOverlay { // a controller button can focus this instrument
                    anchors.fill: parent
                    paramKey: "rack:focus:" + tab.index
                    round: false
                }
                Accessible.role: Accessible.PageTab
                Accessible.name: modelData + (focused ? " (focused)" : "")
            }
        }

        PanelButton { // focus next (learnable)
            width: 40
            height: 40
            faceText: "▸"
            text: "Focus next instrument"
            onClicked: Rack.focus = Rack.stepFocus(1)
            LearnOverlay { anchors.fill: parent; paramKey: "rack:next"; round: false }
        }

        PanelButton { // add or remove instruments
            width: 40
            height: 40
            faceText: "+"
            text: "Add or remove instruments"
            onClicked: instrumentsPopup.open()
        }

        Item { width: 16; height: 1 }

        Rectangle { // the mixer screen (learnable, M)
            id: mixerTab
            width: tabs.screenTabWidth
            height: 40
            radius: 4
            color: Rack.mixerShown ? Theme.silverLight : Theme.lowerDeck
            border.color: Rack.mixerShown ? Theme.accentUi : Theme.deckLine
            border.width: Rack.mixerShown ? 3 : 1

            Led {
                x: 12
                anchors.verticalCenter: parent.verticalCenter
                on: Rack.mixerShown
            }
            Text {
                x: 34
                anchors.verticalCenter: parent.verticalCenter
                text: "Mixer"
                color: Theme.ink
                font.family: Theme.labelFont
                font.pixelSize: 18
                font.bold: Rack.mixerShown
            }
            MouseArea {
                anchors.fill: parent
                onClicked: Rack.mixerShown = !Rack.mixerShown
            }
            LearnOverlay {
                anchors.fill: parent
                paramKey: "rack:mixer"
                round: false
            }
            Accessible.role: Accessible.PageTab
            Accessible.name: "Mixer" + (Rack.mixerShown ? " (shown)" : "")
        }

        Rectangle { // the song screen (learnable, S)
            id: songTab
            width: tabs.screenTabWidth
            height: 40
            radius: 4
            color: Rack.songShown ? Theme.silverLight : Theme.lowerDeck
            border.color: Rack.songShown ? Theme.accentUi : Theme.deckLine
            border.width: Rack.songShown ? 3 : 1

            Led {
                x: 12
                anchors.verticalCenter: parent.verticalCenter
                on: Rack.songShown
            }
            Text {
                x: 34
                anchors.verticalCenter: parent.verticalCenter
                text: "Song"
                color: Theme.ink
                font.family: Theme.labelFont
                font.pixelSize: 18
                font.bold: Rack.songShown
            }
            MouseArea {
                anchors.fill: parent
                onClicked: Rack.songShown = !Rack.songShown
            }
            LearnOverlay {
                anchors.fill: parent
                paramKey: "rack:song"
                round: false
            }
            Accessible.role: Accessible.PageTab
            Accessible.name: "Song" + (Rack.songShown ? " (shown)" : "")
        }

        Rectangle { // the sequencer (learnable, Q)
            id: seqTab
            width: tabs.screenTabWidth
            height: 40
            radius: 4
            color: Rack.seqShown ? Theme.silverLight : Theme.lowerDeck
            border.color: Rack.seqShown ? Theme.accentUi : Theme.deckLine
            border.width: Rack.seqShown ? 3 : 1

            Led {
                x: 12
                anchors.verticalCenter: parent.verticalCenter
                on: Rack.seqShown
            }
            Text {
                x: 34
                anchors.verticalCenter: parent.verticalCenter
                text: "Seq"
                color: Theme.ink
                font.family: Theme.labelFont
                font.pixelSize: 18
                font.bold: Rack.seqShown
            }
            MouseArea {
                anchors.fill: parent
                onClicked: Rack.seqShown = !Rack.seqShown
            }
            LearnOverlay {
                anchors.fill: parent
                paramKey: "rack:seq"
                round: false
            }
            Accessible.role: Accessible.PageTab
            Accessible.name: "Sequencer" + (Rack.seqShown ? " (shown)" : "")
        }

        Rectangle { // the arranger (learnable, A)
            id: arrangeTab
            width: tabs.screenTabWidth
            height: 40
            radius: 4
            color: Rack.arrangeShown ? Theme.silverLight : Theme.lowerDeck
            border.color: Rack.arrangeShown ? Theme.accentUi : Theme.deckLine
            border.width: Rack.arrangeShown ? 3 : 1

            Led {
                x: 12
                anchors.verticalCenter: parent.verticalCenter
                on: Rack.arrangeShown
            }
            Text {
                x: 34
                anchors.verticalCenter: parent.verticalCenter
                text: "Arrange"
                color: Theme.ink
                font.family: Theme.labelFont
                font.pixelSize: 18
                font.bold: Rack.arrangeShown
            }
            MouseArea {
                anchors.fill: parent
                onClicked: Rack.arrangeShown = !Rack.arrangeShown
            }
            LearnOverlay {
                anchors.fill: parent
                paramKey: "rack:arrange"
                round: false
            }
            Accessible.role: Accessible.PageTab
            Accessible.name: "Arranger" + (Rack.arrangeShown ? " (shown)" : "")
        }
    }

    Item { // the focused instrument's panel, scaled to fit
        id: panelArea
        x: 16
        y: tabs.y + tabs.height + 10
        width: root.width - 32
        height: root.height - y - 16
        visible: !Rack.mixerShown && !Rack.songShown && !Rack.seqShown && !Rack.arrangeShown

        Repeater {
            model: Rack.deviceCount

            Item {
                id: slot
                required property int index
                anchors.fill: parent
                visible: Rack.focus === index

                Loader {
                    id: loader
                    // Only an instrument in the rack has its panel made.
                    active: Rack.panelComponents[slot.index] !== "" && Rack.present[slot.index] === true
                    sourceComponent: Qt.createComponent(Rack.panelModules[slot.index], Rack.panelComponents[slot.index])
                    readonly property Item panel: item as Item
                    // Fit, but don't blow small panels up past 1.25x.
                    readonly property real s: panel ? Math.min(slot.width / panel.width, slot.height / panel.height, 1.25) : 1
                    x: panel ? (slot.width - panel.width * s) / 2 : 0
                    y: panel ? (slot.height - panel.height * s) / 2 : 0
                    transform: Scale { xScale: loader.s; yScale: loader.s }
                    // A panel shared by several devices (the audio tracks) is told which it is.
                    onLoaded: if (item && item.hasOwnProperty("rackDevice")) item.rackDevice = slot.index
                }
            }
        }
    }

    Text { // an empty rack
        anchors.centerIn: panelArea
        visible: panelArea.visible && !Rack.present.some((p, i) => p && Rack.panelComponents[i] !== "")
        text: "No instruments in the rack: press + (next to the tabs) to add one."
        color: Theme.inkInverse
        font.family: Theme.labelFont
        font.pixelSize: 20
    }

    Item { // the mixer screen: the strips, taller and (room permitting) larger
        id: mixerArea
        x: 16
        y: panelArea.y
        width: root.width - 32
        height: panelArea.height
        visible: Rack.mixerShown

        Row {
            id: mixer
            // Grow up to 1.5x while the strips fit across; the strips fill the height.
            readonly property real s: Math.max(1, Math.min(1.5, mixerArea.width / (implicitWidth + 40)))
            readonly property real stripHeight: Math.min(mixerArea.height / s, 600)
            x: (mixerArea.width - implicitWidth * s) / 2
            y: (mixerArea.height - stripHeight * s) / 2
            spacing: 10
            transform: Scale { xScale: mixer.s; yScale: mixer.s }

            Repeater {
                model: Rack.deviceNames

                MixerStrip {
                    required property int index
                    required property string modelData
                    visible: Rack.present[index] === true // not in the rack: no strip
                    device: index
                    name: modelData
                    height: mixer.stripHeight
                }
            }

            Rectangle { width: 2; height: mixer.stripHeight; color: Theme.deckLine }

            MixerStrip {
                device: -1
                name: "Master"
                height: mixer.stripHeight
            }
        }
    }

    SongScreen {
        x: 16
        y: panelArea.y
        width: root.width - 32
        height: panelArea.height
        visible: Rack.songShown
    }

    SeqScreen {
        x: 16
        y: panelArea.y
        width: root.width - 32
        height: panelArea.height
        visible: Rack.seqShown
    }

    ArrangeScreen {
        x: 16
        y: panelArea.y
        width: root.width - 32
        height: panelArea.height
        visible: Rack.arrangeShown
    }

    // ---- Instruments in the rack: add or remove (docs/DAW.md, D1)
    Popup {
        id: instrumentsPopup
        x: (root.width - width) / 2
        y: tabs.y + tabs.height + 6
        width: 620
        padding: 16
        modal: true
        background: Rectangle { radius: 8; color: Theme.lowerDeck; border.color: Theme.deckLine; border.width: 2 }

        readonly property var about: ({
            korrode: "Acid bass: one voice, a squelching filter, a 16-step sequencer.",
            kessel: "Drum machine: synthesized drums and a sample voice, a step sequencer.",
            kalliope: "Polysynth: two oscillators, a ladder filter, an arpeggiator.",
            krimson: "Four sounds at once: leads, basses, pads, each on its own channel.",
            kobalt: "Monosynth: one oscillator, a sub, portamento, an arpeggiator.",
            klang: "Sampler: your WAV and MP3 files on pads, slices and patterns.",
            krunch: "Guitar amp: your guitar through amp channels, cabinets and pedals.",
            klick: "Metronome: a click on the rack's tempo."
        })

        Column {
            width: parent.width
            spacing: 8
            Text {
                width: parent.width
                wrapMode: Text.WordWrap
                text: "INSTRUMENTS IN THE RACK. Removing one keeps its sound, mixer settings and MIDI links for when you add it back; it isn't played or shown meanwhile."
                color: Theme.ink
                font.family: Theme.labelFont
                font.pixelSize: 13
                font.bold: true
            }
            Repeater {
                model: Rack.deviceNames
                Row {
                    id: instrumentRow
                    required property int index
                    required property string modelData
                    readonly property bool inRack: Rack.present[index] === true
                    visible: Rack.removable(index) && !Rack.deviceIds[index].startsWith("audio")
                             && !Rack.deviceIds[index].startsWith("plugin") // the tracks and plugin slots: below
                    spacing: 12
                    PanelButton {
                        width: 110
                        height: 32
                        faceText: instrumentRow.inRack ? "REMOVE" : "ADD"
                        text: (instrumentRow.inRack ? "Remove " : "Add ") + instrumentRow.modelData + (instrumentRow.inRack ? " from the rack" : " to the rack")
                        onClicked: Rack.setPresent(instrumentRow.index, !instrumentRow.inRack)
                    }
                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        Text {
                            text: instrumentRow.modelData
                            color: Theme.ink
                            font.family: Theme.labelFont
                            font.pixelSize: 16
                            font.bold: instrumentRow.inRack
                        }
                        Text {
                            width: 460
                            elide: Text.ElideRight
                            text: instrumentsPopup.about[Rack.deviceIds[instrumentRow.index]] ?? ""
                            color: Theme.ink
                            opacity: 0.7
                            font.family: Theme.labelFont
                            font.pixelSize: 12
                        }
                    }
                }
            }
            Text {
                topPadding: 8
                width: parent.width
                wrapMode: Text.WordWrap
                text: "AUDIO TRACKS. Each records your interface's inputs (or another instrument) and plays the take. Lit: in the rack."
                color: Theme.ink
                font.family: Theme.labelFont
                font.pixelSize: 13
                font.bold: true
            }
            Row {
                spacing: 8
                Repeater {
                    model: Rack.deviceIds.map((id, i) => id.startsWith("audio") ? i : -1).filter(i => i >= 0)
                    PanelButton {
                        id: trackButton
                        required property int modelData
                        readonly property bool inRack: Rack.present[modelData] === true
                        width: 64
                        height: 36
                        faceText: Rack.deviceNames[modelData].replace("Audio ", "")
                        text: (inRack ? "Remove " : "Add ") + Rack.deviceNames[modelData] + (inRack ? " from the rack" : " to the rack")
                        tint: inRack ? Theme.labelOrange : "transparent"
                        onClicked: Rack.setPresent(modelData, !inRack)
                    }
                }
            }
            Text {
                topPadding: 8
                width: parent.width
                wrapMode: Text.WordWrap
                text: "PLUGIN SLOTS. Each holds a CLAP plugin you pick on its panel: an instrument, or an effect fed by another device. Lit: in the rack."
                color: Theme.ink
                font.family: Theme.labelFont
                font.pixelSize: 13
                font.bold: true
            }
            Row {
                spacing: 8
                Repeater {
                    model: Rack.deviceIds.map((id, i) => id.startsWith("plugin") ? i : -1).filter(i => i >= 0)
                    PanelButton {
                        id: slotButton
                        required property int modelData
                        readonly property bool inRack: Rack.present[modelData] === true
                        width: 64
                        height: 36
                        faceText: Rack.deviceNames[modelData].replace("Plugin ", "")
                        text: (inRack ? "Remove " : "Add ") + Rack.deviceNames[modelData] + (inRack ? " from the rack" : " to the rack")
                        tint: inRack ? Theme.labelOrange : "transparent"
                        onClicked: Rack.setPresent(modelData, !inRack)
                    }
                }
            }
        }
    }
}

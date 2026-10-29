pragma ComponentBehavior: Bound

import QtQuick
import Krate

// The Krate rack window: transport bar, instrument tabs (click = focus), the
// Mixer and Song tabs; below them the focused instrument's panel scaled to
// fit, the mixer screen or the song screen.
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
        onActivated: Rack.focus = (Rack.focus + 1) % Rack.deviceCount
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

        PanelButton { // focus previous (learnable)
            width: 40
            height: 40
            faceText: "◂"
            text: "Focus previous instrument"
            onClicked: Rack.focus = (Rack.focus + Rack.deviceCount - 1) % Rack.deviceCount
            LearnOverlay { anchors.fill: parent; paramKey: "rack:previous"; round: false }
        }

        Repeater {
            model: Rack.deviceNames

            Rectangle {
                id: tab
                required property int index
                required property string modelData
                readonly property bool focused: Rack.focus === index
                readonly property bool shown: focused && !Rack.mixerShown && !Rack.songShown

                width: 150
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
            onClicked: Rack.focus = (Rack.focus + 1) % Rack.deviceCount
            LearnOverlay { anchors.fill: parent; paramKey: "rack:next"; round: false }
        }

        Item { width: 16; height: 1 }

        Rectangle { // the mixer screen (learnable, M)
            id: mixerTab
            width: 130
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
            width: 130
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
    }

    Item { // the focused instrument's panel, scaled to fit
        id: panelArea
        x: 16
        y: tabs.y + tabs.height + 10
        width: root.width - 32
        height: root.height - y - 16
        visible: !Rack.mixerShown && !Rack.songShown

        Repeater {
            model: Rack.deviceCount

            Item {
                id: slot
                required property int index
                anchors.fill: parent
                visible: Rack.focus === index

                Loader {
                    id: loader
                    sourceComponent: Qt.createComponent(Rack.panelModules[slot.index], Rack.panelComponents[slot.index])
                    readonly property Item panel: item as Item
                    // Fit, but don't blow small panels up past 1.25x.
                    readonly property real s: panel ? Math.min(slot.width / panel.width, slot.height / panel.height, 1.25) : 1
                    x: panel ? (slot.width - panel.width * s) / 2 : 0
                    y: panel ? (slot.height - panel.height * s) / 2 : 0
                    transform: Scale { xScale: loader.s; yScale: loader.s }
                }
            }
        }
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
}

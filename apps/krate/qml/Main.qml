pragma ComponentBehavior: Bound

import QtQuick
import Krate

// The Krate rack window: transport bar, instrument tabs (click = focus), the
// focused instrument's panel scaled to fit, and the mixer.
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

                width: 150
                height: 40
                radius: 4
                color: focused ? Theme.silverLight : Theme.lowerDeck
                border.color: focused ? Theme.accentUi : Theme.deckLine
                border.width: focused ? 3 : 1

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
                    onClicked: Rack.focus = tab.index
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
    }

    Item { // the focused instrument's panel, scaled to fit
        id: panelArea
        x: 16
        y: tabs.y + tabs.height + 10
        width: root.width - 32
        height: root.height - y - mixer.height - 26

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

    Row { // mixer
        id: mixer
        x: 16
        y: root.height - height - 16
        height: 230
        spacing: 10

        Repeater {
            model: Rack.deviceNames

            MixerStrip {
                required property int index
                required property string modelData
                device: index
                name: modelData
            }
        }

        Rectangle { width: 2; height: 230; color: Theme.deckLine }

        MixerStrip {
            device: -1
            name: "Master"
        }
    }
}

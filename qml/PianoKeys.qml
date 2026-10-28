pragma ComponentBehavior: Bound

import QtQuick

// The 13-key note pad (C..C): white keys in the bottom row, black keys raised
// between them, a red LED per key. litKey = semitone 0..12, or -1.
Item {
    id: keys

    property int litKey: -1

    signal keyClicked(int semitone)

    width: 710
    height: 210

    readonly property real whitePitch: width / 8
    readonly property var names: ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B", "C"]
    // semitone -> [isBlack, column]; a black key sits on the boundary after its white column
    readonly property var layout: [[false, 0], [true, 0], [false, 1], [true, 1], [false, 2],
                                   [false, 3], [true, 3], [false, 4], [true, 4], [false, 5],
                                   [true, 5], [false, 6], [false, 7]]

    function centerOf(semitone) {
        const k = layout[semitone]
        return k[0] ? whitePitch * (k[1] + 1) : whitePitch * (k[1] + 0.5)
    }

    Rectangle {
        anchors.fill: parent
        radius: 4
        color: Theme.keyBed
        border.color: Theme.deckLine
    }

    Repeater {
        model: 13

        Item {
            id: key

            required property int index
            readonly property bool black: keys.layout[index][0]
            readonly property real cx: keys.centerOf(index)

            PanelLabel {
                centerX: key.cx
                y: 6
                text: keys.names[key.index]
                font.pixelSize: 15
            }

            Led {
                x: key.cx - width / 2
                y: key.black ? 40 : 50
                on: keys.litKey === key.index
                z: 1
            }

            PanelButton {
                x: key.cx - width / 2
                y: key.black ? 30 : 100
                width: key.black ? 50 : 58
                height: key.black ? 62 : 80
                dark: key.black
                text: keys.names[key.index]
                onClicked: keys.keyClicked(key.index)
            }
        }
    }
}

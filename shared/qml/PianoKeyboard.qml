pragma ComponentBehavior: Bound

import QtQuick

// Two-octave keyboard (C to C) played with the mouse: press and glide across
// keys; keys held from MIDI light up too. `instrument` is the instrument's
// controller singleton, which provides noteOn(n), noteOff(n), noteHeld(n) and
// notesRevision (Kalliope, Krimson).
Item {
    id: keyboard

    required property var instrument
    property int baseNote: 48 // lowest C
    property color litColor: "#E2701F"
    readonly property int whiteCount: 15
    readonly property real whiteWidth: width / whiteCount
    readonly property real blackWidth: whiteWidth * 0.6
    readonly property real blackHeight: height * 0.62
    readonly property var whiteOffsets: [0, 2, 4, 5, 7, 9, 11]
    property int mouseNote: -1

    function whiteNote(i) { return baseNote + 12 * Math.floor(i / 7) + whiteOffsets[i % 7] }
    function hasBlackAfter(i) { return i < whiteCount - 1 && [0, 1, 3, 4, 5].indexOf(i % 7) >= 0 }
    function lit(note) { return note === mouseNote || (instrument.notesRevision >= 0 && instrument.noteHeld(note)) }

    // The key under a point: black keys first (they sit on top).
    function noteAt(px, py) {
        if (px < 0 || px >= width || py < 0 || py >= height)
            return -1
        const i = Math.floor(px / whiteWidth)
        if (py < blackHeight) {
            const left = px - i * whiteWidth
            if (left < blackWidth / 2 && i > 0 && hasBlackAfter(i - 1))
                return whiteNote(i - 1) + 1
            if (left > whiteWidth - blackWidth / 2 && hasBlackAfter(i))
                return whiteNote(i) + 1
        }
        return whiteNote(i)
    }

    function play(note) {
        if (note === mouseNote)
            return
        if (mouseNote >= 0)
            instrument.noteOff(mouseNote)
        mouseNote = note
        if (note >= 0)
            instrument.noteOn(note)
    }

    Accessible.role: Accessible.Grouping
    Accessible.name: "Keyboard"

    Repeater { // white keys
        model: keyboard.whiteCount

        Rectangle {
            id: whiteKey
            required property int index
            readonly property int note: keyboard.whiteNote(index)
            x: index * keyboard.whiteWidth
            width: keyboard.whiteWidth - 2
            height: keyboard.height
            radius: 3
            border.color: "#2A2B2D"
            gradient: Gradient {
                GradientStop { position: 0.0; color: keyboard.lit(whiteKey.note) ? Qt.lighter(keyboard.litColor, 1.5) : "#F4F4F2" }
                GradientStop { position: 1.0; color: keyboard.lit(whiteKey.note) ? keyboard.litColor : "#D4D4D0" }
            }
        }
    }
    Repeater { // black keys
        model: keyboard.whiteCount

        Rectangle {
            id: blackKey
            required property int index
            readonly property int note: keyboard.whiteNote(index) + 1
            visible: keyboard.hasBlackAfter(index)
            x: (index + 1) * keyboard.whiteWidth - keyboard.blackWidth / 2 - 1
            width: keyboard.blackWidth
            height: keyboard.blackHeight
            radius: 2
            border.color: "#000"
            gradient: Gradient {
                GradientStop { position: 0.0; color: keyboard.lit(blackKey.note) ? keyboard.litColor : "#2A2B2E" }
                GradientStop { position: 1.0; color: keyboard.lit(blackKey.note) ? Qt.darker(keyboard.litColor, 1.6) : "#0B0B0C" }
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        onPressed: mouse => keyboard.play(keyboard.noteAt(mouse.x, mouse.y))
        onPositionChanged: mouse => { if (pressed) keyboard.play(keyboard.noteAt(mouse.x, mouse.y)) }
        onReleased: keyboard.play(-1)
        onCanceled: keyboard.play(-1)
    }
}

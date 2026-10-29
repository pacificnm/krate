pragma ComponentBehavior: Bound

import QtQuick

// A 49-key keyboard (four octaves, C to C, like a 49-key controller: C2-C6,
// notes 36-84, at baseNote 36) played with the mouse: press and glide across
// keys; keys held from MIDI light up too. `instrument` is the instrument's
// controller singleton, which provides noteOn(n), noteOff(n), noteHeld(n) and
// notesRevision (Kalliope, Krimson).
//
// It follows the keys you play: a controller's own octave buttons move its
// notes without telling anyone, so when a held note falls outside the four
// octaves shown, the keyboard slides by octaves to show it (and says so in
// its corner). Changing baseNote (the instrument's Octave) puts it back.
Item {
    id: keyboard

    required property var instrument
    property int baseNote: 36 // lowest C
    property int octaves: 4   // 49 keys
    property bool followKeys: true
    property int followOctaves: 0 // how far it has slid to show played notes
    readonly property int shownBase: Math.max(0, Math.min(108, baseNote + 12 * followOctaves))
    onBaseNoteChanged: followOctaves = 0
    property color litColor: "#E2701F"
    readonly property int whiteCount: 7 * octaves + 1
    readonly property real whiteWidth: width / whiteCount
    readonly property real blackWidth: whiteWidth * 0.6
    readonly property real blackHeight: height * 0.62
    readonly property var whiteOffsets: [0, 2, 4, 5, 7, 9, 11]
    property int mouseNote: -1

    function whiteNote(i) { return shownBase + 12 * Math.floor(i / 7) + whiteOffsets[i % 7] }

    // Slide to show held notes: the lowest one stays in view when they span more than two octaves.
    function follow() {
        if (!followKeys || instrument.notesRevision < 0)
            return
        let low = 128, high = -1
        for (let n = 0; n < 128; ++n)
            if (instrument.noteHeld(n)) {
                low = Math.min(low, n)
                high = Math.max(high, n)
            }
        if (high < 0)
            return // nothing held: stay where it is
        const top = shownBase + 12 * octaves
        if (low < shownBase) {
            followOctaves -= Math.ceil((shownBase - low) / 12)
        } else if (high > top) {
            // Up far enough to show the highest note, but never past the lowest.
            const up = Math.ceil((high - top) / 12)
            followOctaves += Math.min(up, Math.floor((low - shownBase) / 12))
        }
    }
    Connections {
        target: keyboard.instrument
        function onNotesRevisionChanged() { keyboard.follow() }
    }
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

    Text { // where it has slid to, while following
        visible: keyboard.followOctaves !== 0
        x: 6
        y: 4
        z: 10
        text: "showing C" + (Math.floor(keyboard.shownBase / 12) - 1) + "–C" + (Math.floor(keyboard.shownBase / 12) - 1 + keyboard.octaves)
        color: "#2A2B2D"
        font.pixelSize: Math.max(10, keyboard.height * 0.05)
        font.bold: true
    }
}

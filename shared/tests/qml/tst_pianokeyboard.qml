import QtQuick
import QtTest
import "../../qml" // PianoKeyboard.qml (it needs nothing but QtQuick)

// The on-screen keyboard follows notes played outside the two octaves it
// shows (a controller's own octave buttons move its notes silently).
TestCase {
    id: test
    name: "PianoKeyboard"
    width: 600
    height: 200
    when: windowShown

    QtObject { // an instrument holding whichever notes the test sets
        id: fake
        property var held: ({})
        property int notesRevision: 0
        function noteHeld(n) { return held[n] === true }
        function noteOn(n) {}
        function noteOff(n) {}
        function hold(notes) {
            const h = {}
            for (const n of notes)
                h[n] = true
            held = h
            ++notesRevision
        }
    }

    PianoKeyboard {
        id: keys
        instrument: fake
        width: 600
        height: 200
        baseNote: 36
    }

    function init() {
        fake.hold([])
        keys.baseNote = 36
        keys.followOctaves = 0
        keys.followKeys = true
    }

    function test_fortyNineKeys() {
        compare(keys.whiteCount, 29) // C2..C6: 29 white keys, 49 in all
        compare(keys.whiteNote(0), 36)
        compare(keys.whiteNote(28), 84)
        compare(keys.noteAt(1, keys.height - 1), 36) // the leftmost key is the lowest C
    }

    function test_staysWhileNotesFit() {
        fake.hold([36, 60, 84]) // C2, middle C, C6: the whole range shown
        compare(keys.shownBase, 36)
    }

    function test_slidesUpAndDown() {
        fake.hold([88]) // E6: a controller shifted up an octave
        compare(keys.shownBase, 48)
        fake.hold([]) // let go: it stays
        compare(keys.shownBase, 48)
        fake.hold([100]) // E7
        compare(keys.shownBase, 60)
        fake.hold([30]) // F#1: down
        compare(keys.shownBase, 24)
    }

    function test_widestChordKeepsTheLowestNote() {
        fake.hold([38, 110]) // more than four octaves apart: the low one stays in view
        compare(keys.shownBase, 36)
        fake.hold([62, 110])
        compare(keys.shownBase, 60)
    }

    function test_octaveChangeResets() {
        fake.hold([90])
        verify(keys.followOctaves !== 0)
        keys.baseNote = 48 // the instrument's Octave moved
        compare(keys.followOctaves, 0)
        compare(keys.shownBase, 48)
    }

    function test_canBeOff() {
        keys.followKeys = false
        fake.hold([100])
        compare(keys.shownBase, 36)
    }
}

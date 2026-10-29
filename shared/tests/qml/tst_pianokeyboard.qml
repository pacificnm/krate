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
        baseNote: 48
    }

    function init() {
        fake.hold([])
        keys.baseNote = 48
        keys.followOctaves = 0
        keys.followKeys = true
    }

    function test_staysWhileNotesFit() {
        fake.hold([48, 60, 72]) // C3..C5: the whole range shown
        compare(keys.shownBase, 48)
    }

    function test_slidesUpAndDown() {
        fake.hold([76]) // E5: one octave above what's shown
        compare(keys.shownBase, 60)
        fake.hold([]) // let go: it stays
        compare(keys.shownBase, 60)
        fake.hold([88]) // E6
        compare(keys.shownBase, 72)
        fake.hold([40]) // E2: down
        compare(keys.shownBase, 36)
    }

    function test_widestChordKeepsTheLowestNote() {
        fake.hold([50, 90]) // three octaves apart: the low one stays in view
        compare(keys.shownBase, 48)
        fake.hold([62, 90])
        compare(keys.shownBase, 60)
    }

    function test_octaveChangeResets() {
        fake.hold([80])
        verify(keys.followOctaves !== 0)
        keys.baseNote = 60 // the instrument's Octave moved
        compare(keys.followOctaves, 0)
        compare(keys.shownBase, 60)
    }

    function test_canBeOff() {
        keys.followKeys = false
        fake.hold([90])
        compare(keys.shownBase, 48)
    }
}

import QtQuick
import QtTest
import "../../qml" // PianoKeyboard.qml (it needs nothing but QtQuick)

// The on-screen keyboard is your MIDI keyboard key for key: with 49 keys
// whose first C sends 35 at octave 0, key 0 is note 35 and lights for it.
TestCase {
    id: test
    name: "PianoKeyboard"
    width: 980
    height: 200
    when: windowShown

    QtObject { // an instrument holding whichever notes the test sets
        id: fake
        property var held: ({})
        property int notesRevision: 0
        property int lastOn: -1
        function noteHeld(n) { return held[n] === true }
        function noteOn(n) { lastOn = n }
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
        width: 980
        height: 200
        keyCount: 49
        firstNoteAtZero: 35
        octaveDown: 3
        octaveUp: 4
        onLowestKeyCaptured: note => { keys.firstNoteAtZero = note; keys.capturing = false }
    }

    function init() {
        fake.hold([])
        keys.keyCount = 49
        keys.firstNoteAtZero = 35
        keys.instrumentOctave = 0
        keys.controllerOctave = 0
        keys.capturing = false
    }

    function test_keyForKey() {
        compare(keys.whiteKeys.length, 29)         // 49 keys from a C: 29 white
        compare(keys.noteOf(0), 35)               // the first key
        compare(keys.noteOf(48), 83)              // the last key
        fake.hold([35])
        verify(keys.lit(0))                       // lights the leftmost key
        verify(!keys.lit(1))
        compare(keys.keyAt(2, keys.height - 2), 0) // the leftmost white key is key 0
        keys.play(keys.keyAt(2, keys.height - 2)) // a click there...
        compare(fake.lastOn, 35)                   // ...plays what the real key sends
        keys.play(-1)
        fake.hold([36])
        verify(keys.lit(1))                       // the next key is a black key on the screen, as on the controller
        verify(!keys.isWhite(1))
    }

    function test_followsTheControllerOctave() {
        fake.hold([35 - 36]) // impossible note below 0: ignored by the instrument; use a real one
        fake.hold([11])      // the first key at octave -2 (35 - 24)
        compare(keys.controllerOctave, -2)
        verify(keys.lit(0))
        fake.hold([])
        compare(keys.controllerOctave, -2) // stays
        fake.hold([95])     // above: back up
        verify(keys.lit(95 - keys.firstNote))
        fake.hold([0])      // far below: stops at -3
        compare(keys.controllerOctave, -3)
        fake.hold([127])    // far above: stops at +4
        compare(keys.controllerOctave, 4)
    }

    function test_instrumentOctave() {
        keys.instrumentOctave = 1 // the instrument shifts what it holds up an octave
        fake.hold([47])           // the first key (35) as the instrument holds it
        verify(keys.lit(0))
        compare(keys.controllerOctave, 0)
    }

    function test_setFromKey() {
        keys.capturing = true
        fake.hold([36])
        compare(keys.firstNoteAtZero, 36)
        verify(!keys.capturing)
        compare(keys.noteOf(0), 36)
    }

    function test_otherSizes() {
        keys.keyCount = 61
        compare(keys.whiteKeys.length, 36)
        keys.keyCount = 25
        compare(keys.whiteKeys.length, 15)
    }
}

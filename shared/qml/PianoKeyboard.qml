pragma ComponentBehavior: Bound

import QtQuick

// The MIDI keyboard you play, drawn key for key: `keyCount` keys starting on
// a C, where key k plays and shows note firstNoteAtZero + k (plus 12 for each
// octave of the instrument's own Octave and of the controller's octave
// buttons). Played with the mouse too. `instrument` is the instrument's
// controller singleton, which provides noteOn(n), noteOff(n), noteHeld(n) and
// notesRevision (Kalliope, Krimson).
//
// A controller's octave buttons send nothing, so when a held note lands
// outside the keys shown, the keyboard moves by whole octaves to it (never
// past octaveDown / octaveUp). Panels use ControllerPianoKeyboard, which
// takes these settings from ControllerKeyboard (your keyboard's settings).
Item {
    id: keyboard

    required property var instrument
    property int instrumentOctave: 0 // the instrument's own Octave
    property int keyCount: 49
    property int firstNoteAtZero: 35 // what the first key (a C) sends at octave 0
    property int octaveDown: 3
    property int octaveUp: 4
    property bool capturing: false   // the next key held sets firstNoteAtZero (SET FROM KEY)
    signal lowestKeyCaptured(int note)
    property color litColor: "#E2701F"

    property int controllerOctave: 0 // where the controller's octave buttons are, as far as we've seen
    readonly property int firstNote: firstNoteAtZero + 12 * (controllerOctave + instrumentOctave)
    readonly property var whiteKeys: {
        const keys = []
        for (let k = 0; k < keyCount; ++k)
            if (isWhite(k))
                keys.push(k)
        return keys
    }
    readonly property real whiteWidth: width / Math.max(1, whiteKeys.length)
    readonly property real blackWidth: whiteWidth * 0.6
    readonly property real blackHeight: height * 0.62
    property int mouseNote: -1

    onFirstNoteAtZeroChanged: controllerOctave = 0
    onKeyCountChanged: controllerOctave = 0

    function isWhite(k) { return [0, 2, 4, 5, 7, 9, 11].indexOf(k % 12) >= 0 }
    function noteOf(k) { return firstNote + k }
    function lit(k) {
        const n = noteOf(k)
        return n === mouseNote || (instrument.notesRevision >= 0 && instrument.noteHeld(n))
    }

    // The key under a point (black keys sit on top), or -1.
    function keyAt(px, py) {
        if (px < 0 || px >= width || py < 0 || py >= height || whiteKeys.length === 0)
            return -1
        const i = Math.min(whiteKeys.length - 1, Math.floor(px / whiteWidth))
        const k = whiteKeys[i]
        if (py < blackHeight) {
            const left = px - i * whiteWidth
            if (left < blackWidth / 2 && k > 0 && !isWhite(k - 1))
                return k - 1
            if (left > whiteWidth - blackWidth / 2 && k + 1 < keyCount && !isWhite(k + 1))
                return k + 1
        }
        return k
    }

    function play(k) { // key index, or -1
        const note = k >= 0 ? noteOf(k) : -1
        if (note === mouseNote)
            return
        if (mouseNote >= 0)
            instrument.noteOff(mouseNote)
        mouseNote = note
        if (note >= 0)
            instrument.noteOn(note)
    }

    // Keys held on the controller: SET FROM KEY, or follow its octave buttons.
    function notesMoved() {
        if (instrument.notesRevision < 0)
            return
        let low = 128, high = -1
        for (let n = 0; n < 128; ++n)
            if (instrument.noteHeld(n)) {
                low = Math.min(low, n)
                high = Math.max(high, n)
            }
        if (high < 0)
            return
        if (capturing) { // the key pressed is the first key, at octave 0
            controllerOctave = 0
            lowestKeyCaptured(low - 12 * instrumentOctave)
            return
        }
        const kLow = low - firstNote, kHigh = high - firstNote
        if (kLow < 0) {
            controllerOctave = Math.max(-octaveDown, controllerOctave - Math.ceil(-kLow / 12))
        } else if (kHigh > keyCount - 1) {
            const up = Math.min(Math.ceil((kHigh - keyCount + 1) / 12), Math.floor(kLow / 12)) // keep the lowest in view
            controllerOctave = Math.min(octaveUp, controllerOctave + up)
        }
    }
    Connections {
        target: keyboard.instrument
        function onNotesRevisionChanged() { keyboard.notesMoved() }
    }

    Accessible.role: Accessible.Grouping
    Accessible.name: "Keyboard"

    Repeater { // white keys
        model: keyboard.whiteKeys

        Rectangle {
            id: whiteKey
            required property int index
            required property int modelData // key index
            x: index * keyboard.whiteWidth
            width: keyboard.whiteWidth - 2
            height: keyboard.height
            radius: 3
            border.color: "#2A2B2D"
            gradient: Gradient {
                GradientStop { position: 0.0; color: keyboard.lit(whiteKey.modelData) ? Qt.lighter(keyboard.litColor, 1.5) : "#F4F4F2" }
                GradientStop { position: 1.0; color: keyboard.lit(whiteKey.modelData) ? keyboard.litColor : "#D4D4D0" }
            }
        }
    }
    Repeater { // black keys: after the white key before them
        model: keyboard.whiteKeys

        Rectangle {
            id: blackKey
            required property int index
            required property int modelData // the white key's index
            readonly property int key: modelData + 1
            visible: key < keyboard.keyCount && !keyboard.isWhite(key)
            x: (index + 1) * keyboard.whiteWidth - keyboard.blackWidth / 2 - 1
            width: keyboard.blackWidth
            height: keyboard.blackHeight
            radius: 2
            border.color: "#000"
            gradient: Gradient {
                GradientStop { position: 0.0; color: keyboard.lit(blackKey.key) ? keyboard.litColor : "#2A2B2E" }
                GradientStop { position: 1.0; color: keyboard.lit(blackKey.key) ? Qt.darker(keyboard.litColor, 1.6) : "#0B0B0C" }
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        onPressed: mouse => keyboard.play(keyboard.keyAt(mouse.x, mouse.y))
        onPositionChanged: mouse => { if (pressed) keyboard.play(keyboard.keyAt(mouse.x, mouse.y)) }
        onReleased: keyboard.play(-1)
        onCanceled: keyboard.play(-1)
    }
}

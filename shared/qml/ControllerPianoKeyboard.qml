import QtQuick

// The on-screen keyboard as your MIDI keyboard: PianoKeyboard with the
// settings of ControllerKeyboard (keys, first note at octave 0, octave range,
// SET FROM KEY).
PianoKeyboard {
    keyCount: ControllerKeyboard.keys
    firstNoteAtZero: ControllerKeyboard.firstNote
    octaveDown: ControllerKeyboard.octaveDown
    octaveUp: ControllerKeyboard.octaveUp
    capturing: ControllerKeyboard.capturing
    onLowestKeyCaptured: note => ControllerKeyboard.captured(note)
}

pragma ComponentBehavior: Bound

import QtQuick
import Krate
import KrateRack

// The open sequence (docs/SEQUENCER.md): its name (saved in the library, or
// not yet), NEW, SAVE, the library and CLOSE; on the Seq and Arrange screens.
// A sequence keeps its patterns and its arrangement, audio clips included.
Rectangle {
    id: box

    property bool stacked: true // the name above the buttons (or beside them)

    width: stacked ? 300 : nameText.width + buttons.width + 26
    height: stacked ? 76 : 36
    radius: 4
    color: Theme.lcdBg
    border.color: Theme.deckLine

    Text {
        id: nameText
        x: 10
        y: box.stacked ? 6 : Math.round((box.height - height) / 2)
        width: box.stacked ? box.width - 20 : 220
        text: (Sequencer.sequenceId > 0 ? Sequencer.sequenceName : "Not saved yet")
              + (Sequencer.sequenceId > 0 && Sequencer.modified ? "  •  changed" : "")
        color: Theme.lcdInk
        font.family: Theme.labelFont
        font.pixelSize: 15
        font.bold: true
        elide: Text.ElideRight
    }
    Row {
        id: buttons
        x: box.stacked ? 8 : nameText.x + nameText.width + 8
        y: box.stacked ? 36 : 3
        spacing: 5
        PanelButton { // NEW: asks once more when there are unsaved changes
            id: newButton
            property bool confirming: false
            width: confirming ? 150 : 56
            height: 30
            faceText: confirming ? "DROP CHANGES?" : "NEW"
            text: confirming ? "Press again to clear it all without saving" : "Start a new, empty sequence and arrangement"
            onClicked: {
                if (Sequencer.modified && !confirming) {
                    confirming = true
                    newConfirmTimer.restart()
                    return
                }
                confirming = false
                Sequencer.newSequence()
            }
            Timer {
                id: newConfirmTimer
                interval: 4000
                onTriggered: newButton.confirming = false
            }
        }
        PanelButton {
            visible: !newButton.confirming
            width: 62
            height: 30
            faceText: "SAVE"
            text: Sequencer.sequenceId > 0 ? "Save over " + Sequencer.sequenceName : "Save it in the library"
            onClicked: {
                if (Sequencer.sequenceId > 0)
                    Sequencer.saveSequence()
                else
                    library.openSaveAs()
            }
        }
        PanelButton {
            visible: !newButton.confirming
            width: 88
            height: 30
            faceText: "LIBRARY…"
            text: "Saved sequences: open, save as, rename, delete"
            onClicked: library.open()
        }
        PanelButton {
            width: 64
            height: 30
            visible: Sequencer.sequenceId > 0 && !newButton.confirming
            faceText: "CLOSE"
            text: "Close it: back to what was here before, sounds and mixer too"
            onClicked: Sequencer.closeSequence()
        }
    }

    SequenceLibrary {
        id: library
    }
}

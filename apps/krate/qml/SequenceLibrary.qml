pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Krate
import KrateRack

// The library's saved sequences (docs/SEQUENCER.md): open, rename, delete,
// and save what's on the rack as a new one. Each keeps its patterns, its
// arrangement (audio clips too), the instruments' sounds and the mixer.
Popup {
    id: library
    parent: Overlay.overlay
    x: Math.round((parent.width - width) / 2)
    y: Math.round((parent.height - height) / 2)
    width: 620
    height: Math.min(parent.height - 80, 640)
    modal: true
    padding: 20
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    property int confirmOpen: -1    // a sequence to open over unsaved changes
    property int confirmDelete: -1
    property int renaming: -1
    function openSaveAs() {
        saveAsName.text = Sequencer.sequenceId > 0 ? Sequencer.sequenceName + " 2" : ""
        open()
        saveAsName.forceActiveFocus()
    }
    onOpened: {
        confirmOpen = -1
        confirmDelete = -1
        renaming = -1
    }

    background: Rectangle {
        radius: 8
        color: Theme.silverLight
        border.color: Theme.deckLine
        border.width: 2
    }

    Item {
        anchors.fill: parent

        Text {
            id: libraryTitle
            text: "Saved sequences and arrangements"
            color: Theme.ink
            font.family: Theme.labelFont
            font.pixelSize: 20
            font.bold: true
        }
        Text {
            anchors.top: libraryTitle.bottom
            anchors.topMargin: 4
            width: parent.width
            text: "Each keeps its patterns, tracks, chain and range, its arrangement (audio clips too), and the instruments' sounds and the mixer as they were when saved."
            wrapMode: Text.WordWrap
            color: "#55575B"
            font.family: Theme.labelFont
            font.pixelSize: 12
        }

        ListView {
            id: sequenceList
            y: 84
            width: parent.width
            height: parent.height - y - saveAsRow.height - 16
            clip: true
            spacing: 4
            model: Sequencer.savedSequences
            ScrollBar.vertical: ScrollBar {}

            Text {
                visible: sequenceList.count === 0
                y: 8
                text: "None yet: give this one a name below and SAVE AS."
                color: "#55575B"
                font.family: Theme.labelFont
                font.pixelSize: 13
            }

            delegate: Rectangle {
                id: row
                required property var modelData
                readonly property bool isOpen: Sequencer.sequenceId === modelData.id
                width: sequenceList.width
                height: 46
                radius: 4
                color: isOpen ? "#F4E7D6" : "#D9DADC"
                border.color: isOpen ? Theme.labelOrange : Theme.deckLine

                Column {
                    x: 10
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - buttons.width - 30
                    visible: library.renaming !== row.modelData.id
                    Text {
                        width: parent.width
                        text: row.modelData.name + (row.isOpen ? "  (open)" : "")
                        color: Theme.ink
                        font.family: Theme.labelFont
                        font.pixelSize: 14
                        font.bold: row.isOpen
                        elide: Text.ElideRight
                    }
                    Text {
                        text: "saved " + row.modelData.saved
                        color: "#6A6C70"
                        font.family: Theme.labelFont
                        font.pixelSize: 10
                    }
                }
                TextField {
                    id: renameField
                    x: 10
                    width: parent.width - buttons.width - 30
                    anchors.verticalCenter: parent.verticalCenter
                    visible: library.renaming === row.modelData.id
                    onVisibleChanged: if (visible) { text = row.modelData.name; forceActiveFocus(); selectAll() }
                    onAccepted: {
                        Sequencer.renameSequence(row.modelData.id, text)
                        library.renaming = -1
                    }
                    Accessible.name: "New name"
                }

                Row {
                    id: buttons
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 6
                    PanelButton {
                        width: library.confirmOpen === row.modelData.id ? 170 : 66
                        height: 30
                        faceText: library.confirmOpen === row.modelData.id ? "OPEN, DROP CHANGES" : "OPEN"
                        text: "Open " + row.modelData.name
                        onClicked: {
                            const unsaved = Sequencer.modified
                            if (unsaved && library.confirmOpen !== row.modelData.id) {
                                library.confirmOpen = row.modelData.id
                                return
                            }
                            if (Sequencer.openSequence(row.modelData.id))
                                library.close()
                        }
                    }
                    PanelButton {
                        width: 80
                        height: 30
                        faceText: library.renaming === row.modelData.id ? "OK" : "RENAME"
                        text: "Rename " + row.modelData.name
                        onClicked: {
                            if (library.renaming === row.modelData.id) {
                                Sequencer.renameSequence(row.modelData.id, renameField.text)
                                library.renaming = -1
                            } else {
                                library.renaming = row.modelData.id
                            }
                        }
                    }
                    PanelButton {
                        width: library.confirmDelete === row.modelData.id ? 116 : 70
                        height: 30
                        faceText: library.confirmDelete === row.modelData.id ? "REALLY DELETE" : "DELETE"
                        text: "Delete " + row.modelData.name
                        onClicked: {
                            if (library.confirmDelete !== row.modelData.id) {
                                library.confirmDelete = row.modelData.id
                                return
                            }
                            Sequencer.deleteSequence(row.modelData.id)
                            library.confirmDelete = -1
                        }
                    }
                }
            }
        }

        Row {
            id: saveAsRow
            anchors.bottom: parent.bottom
            spacing: 8
            TextField {
                id: saveAsName
                width: 300
                placeholderText: "A name for what's in the sequencer now"
                onAccepted: if (Sequencer.saveSequenceAs(text)) library.close()
                Accessible.name: "Name to save as"
            }
            PanelButton {
                width: 96
                height: 34
                enabled: saveAsName.text.trim() !== ""
                opacity: enabled ? 1 : 0.5
                faceText: "SAVE AS"
                text: "Save what's in the sequencer as a new sequence"
                onClicked: if (Sequencer.saveSequenceAs(saveAsName.text)) library.close()
            }
            PanelButton {
                width: 80
                height: 34
                faceText: "CLOSE"
                text: "Close this list"
                onClicked: library.close()
            }
        }
    }
}

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KrateRemote

// The player's settings (docs/SETTINGS.md), as Krate's own Settings screen
// has them. Krate takes a change while it runs; a folder waits for its next
// start (RESTART KRATE), its files moved there first if asked.
ScrollView {
    id: page
    contentWidth: availableWidth
    clip: true

    component Heading: Label {
        font.pixelSize: 17
        font.bold: true
        topPadding: 10
    }
    component Note: Label {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: palette.placeholderText
    }
    component PathLabel: Label {
        Layout.fillWidth: true
        font.family: "monospace"
        wrapMode: Text.WrapAnywhere
    }

    ColumnLayout {
        x: 20
        width: page.availableWidth - 40
        spacing: 8

        // ---- Waiting for Krate's start
        Frame {
            Layout.fillWidth: true
            Layout.topMargin: 12
            visible: Remote.restartNeeded
            RowLayout {
                anchors.fill: parent
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: "Folders change when Krate starts again (their files moved first, if asked)."
                }
                Button {
                    text: "RESTART KRATE NOW"
                    highlighted: true
                    enabled: Remote.krateState === "kiosk"
                    onClicked: Remote.restartKrate()
                }
            }
        }

        Heading { text: "Folders" }
        Note { text: "Where Krate keeps things on the player. A new folder for the ones Krate has open applies at its next start; exports at once." }

        Repeater {
            model: Remote.folders
            delegate: Frame {
                id: row
                required property var modelData
                Layout.fillWidth: true
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 4
                    RowLayout {
                        Label {
                            text: row.modelData.label
                            font.bold: true
                        }
                        Label {
                            text: row.modelData.isDefault ? "(default)" : ""
                            color: palette.placeholderText
                        }
                        Item { Layout.fillWidth: true }
                        Button {
                            text: "CHANGE…"
                            onClicked: changeDialog.ask(row.modelData)
                        }
                        Button {
                            text: "DEFAULT"
                            visible: !row.modelData.isDefault
                            onClicked: changeDialog.ask(row.modelData, row.modelData.defaultPath)
                        }
                    }
                    Note { text: row.modelData.about }
                    PathLabel { text: row.modelData.path }
                    RowLayout {
                        visible: row.modelData.pending !== ""
                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WrapAnywhere
                            color: "#C0581A"
                            text: "From Krate's next start: " + row.modelData.pending
                                  + (row.modelData.pendingMove ? "  (its files moved there)" : "  (used as it is)")
                        }
                        Button {
                            text: "CANCEL"
                            onClicked: Remote.cancel(row.modelData.key)
                        }
                    }
                }
            }
        }

        Heading { text: "Plugin folders" }
        Note {
            text: "CLAP plugins are looked for in these folders, then the standard ones (~/.clap, /usr/lib/clap…). "
                  + "Krate looks again at once. Put plugins there from Files."
        }
        Repeater {
            model: Remote.pluginFolders
            delegate: RowLayout {
                id: pluginRow
                required property string modelData
                Layout.fillWidth: true
                PathLabel { text: pluginRow.modelData }
                Button {
                    text: "REMOVE"
                    onClicked: Remote.removePluginFolder(pluginRow.modelData)
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            TextField {
                id: pluginField
                Layout.fillWidth: true
                placeholderText: "A folder on the player: /home/" + Remote.user + "/clap"
                onAccepted: addPlugin.clicked()
            }
            Button {
                text: "USE FOLDER OPEN IN FILES"
                enabled: Files.path !== ""
                onClicked: pluginField.text = Files.path
            }
            Button {
                id: addPlugin
                text: "ADD"
                enabled: pluginField.text.trim().startsWith("/")
                onClicked: {
                    Remote.addPluginFolder(pluginField.text)
                    pluginField.text = ""
                }
            }
        }

        Heading { text: "MIDI" }
        Note {
            text: "MIDI inputs: empty connects every MIDI device plugged into the player (two keyboards play together, "
                  + "told apart by their channels); otherwise only those whose name has this in it."
        }
        RowLayout {
            Layout.fillWidth: true
            TextField {
                id: midiField
                Layout.fillWidth: true
                text: Remote.midiInputs
                placeholderText: "Every MIDI device"
                onAccepted: Remote.setMidiInputs(text)
            }
            Button {
                text: "APPLY"
                enabled: midiField.text !== Remote.midiInputs
                onClicked: Remote.setMidiInputs(midiField.text)
            }
        }
        Switch {
            text: "Program Change n focuses the nth instrument in the rack (off: it goes to the instrument as a program change)"
            checked: Remote.programChangeFocus
            onToggled: Remote.setProgramChangeFocus(checked)
        }

        Heading { text: "Updates" }
        Switch {
            text: "Krate checks for a new version by itself"
            checked: Remote.autoCheckUpdates
            onToggled: Remote.setAutoCheckUpdates(checked)
        }
        Note {
            text: "Installing an update is done on the player (⚙ ▸ Updates ▸ UPDATE TO): it needs whoever sits at it."
        }

        Heading { text: "The file" }
        PathLabel { text: Remote.settingsPath }
        Note {
            text: "Audio, your MIDI keyboard's keys and the instruments' own settings are set on the player."
        }
        Item { Layout.preferredHeight: 20 }
    }

    // A folder's new place: typed, or the one open in Files.
    Dialog {
        id: changeDialog
        property var row: ({})
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(640, parent.width - 40)
        modal: true
        title: (row.label || "") + ": a new folder"

        function ask(r, path) {
            row = r
            pathField.text = path !== undefined ? path : (r.pending || r.path)
            open()
            pathField.forceActiveFocus()
        }
        function apply(move) {
            const key = row.key
            const path = pathField.text.trim() === row.defaultPath ? "" : pathField.text.trim()
            Remote.setFolder(key, path, move)
            close()
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 8
            Label { text: "Now: " + (changeDialog.row.path || "") }
            RowLayout {
                Layout.fillWidth: true
                TextField {
                    id: pathField
                    Layout.fillWidth: true
                    font.family: "monospace"
                }
                Button {
                    text: "FOLDER OPEN IN FILES"
                    enabled: Files.path !== ""
                    onClicked: pathField.text = Files.path
                }
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: palette.placeholderText
                text: changeDialog.row.atOnce
                      ? "Exports change at once: nothing to move."
                      : "MOVE MY FILES THERE: at Krate's next start, what's in the old folder goes to the new one (only into a "
                        + "new or empty folder). USE WHAT'S THERE: Krate uses the new folder as it is; your files stay where they were."
            }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                Button {
                    text: "MOVE MY FILES THERE"
                    visible: !changeDialog.row.atOnce
                    enabled: pathField.text.trim().startsWith("/")
                    highlighted: true
                    onClicked: changeDialog.apply(true)
                }
                Button {
                    text: changeDialog.row.atOnce ? "USE IT" : "USE WHAT'S THERE"
                    enabled: pathField.text.trim().startsWith("/")
                    onClicked: changeDialog.apply(false)
                }
                Button {
                    text: "CANCEL"
                    onClicked: changeDialog.close()
                }
            }
        }
    }
}

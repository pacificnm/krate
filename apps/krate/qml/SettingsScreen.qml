pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import Krate
import KrateRack

// Settings (docs/SETTINGS.md): where Krate keeps things, audio, your MIDI
// keyboard and inputs, updates, crash reports, and what's where.
Item {
    id: screen

    property string section: "folders"
    readonly property var sections: [
        { key: "folders", label: "Folders" },
        { key: "audio", label: "Audio" },
        { key: "midi", label: "MIDI and keys" },
        { key: "updates", label: "Updates" + (Updates.available ? "  •" : "") },
        { key: "crashes", label: "Crash reports" },
        { key: "about", label: "About" },
    ]

    component Heading: Text {
        color: Theme.ink
        font.family: Theme.labelFont
        font.pixelSize: 15
        font.bold: true
    }
    component Note: Text {
        color: "#55575B"
        font.family: Theme.labelFont
        font.pixelSize: 12
        wrapMode: Text.WordWrap
    }
    component PathText: Text {
        color: Theme.ink
        font.family: "monospace"
        font.pixelSize: 12
        wrapMode: Text.WrapAnywhere
    }
    // A switch: a button with its LED, and what it does.
    component Switch: Row {
        id: sw
        property bool on
        property string label
        property string tip
        signal toggled()
        spacing: 10
        PanelButton {
            width: 46
            height: 28
            hasLed: true
            ledOn: sw.on
            text: sw.tip
            onClicked: sw.toggled()
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: sw.label
            color: Theme.ink
            font.family: Theme.labelFont
            font.pixelSize: 13
        }
    }

    Rectangle { // the deck
        anchors.fill: parent
        radius: 6
        color: Theme.silverLight
        border.color: Theme.deckLine
    }

    // ---- Waiting for a restart (folders), or what this start did with them
    Rectangle {
        id: restartBar
        x: 16
        y: 12
        width: parent.width - 32
        height: visible ? 46 : 0
        visible: Settings.restartNeeded
        radius: 6
        color: "#5A1E1A"
        Text {
            x: 14
            width: parent.width - 240
            anchors.verticalCenter: parent.verticalCenter
            text: "Folder changes wait for Krate to start again."
            color: "#F2E8E6"
            font.family: Theme.labelFont
            font.pixelSize: 14
            font.bold: true
            elide: Text.ElideRight
        }
        PanelButton {
            anchors.right: parent.right
            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            width: 200
            height: 30
            faceText: "RESTART KRATE NOW"
            text: Settings.kiosk ? "Krate quits and the player starts it again (a few seconds)" : "Krate quits and starts again"
            onClicked: Settings.restart()
        }
    }

    // ---- The sections
    Column {
        id: nav
        x: 16
        y: restartBar.y + restartBar.height + (restartBar.visible ? 12 : 4)
        width: 190
        spacing: 6
        Repeater {
            model: screen.sections
            Rectangle {
                id: navItem
                required property var modelData
                readonly property bool chosen: screen.section === modelData.key
                width: nav.width
                height: 40
                radius: 4
                color: chosen ? "#FFFFFF" : Theme.lowerDeck
                border.color: chosen ? Theme.accentUi : Theme.deckLine
                border.width: chosen ? 2 : 1
                Text {
                    x: 14
                    anchors.verticalCenter: parent.verticalCenter
                    text: navItem.modelData.label
                    color: Theme.ink
                    font.family: Theme.labelFont
                    font.pixelSize: 15
                    font.bold: navItem.chosen
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: screen.section = navItem.modelData.key
                }
            }
        }
    }

    Flickable {
        id: page
        x: nav.x + nav.width + 20
        y: nav.y
        width: parent.width - x - 16
        height: parent.height - y - 12
        contentWidth: width
        contentHeight: content.height + 20
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        Column {
            id: content
            width: page.width - 16
            spacing: 14

            // ================= Folders
            Column {
                visible: screen.section === "folders"
                width: parent.width
                spacing: 12
                Heading { text: "FOLDERS" }
                Note {
                    width: parent.width
                    text: "Where Krate keeps things. A new folder applies when Krate starts again (exports: at once); MOVE MY FILES THERE takes what's in the old one along first, USE WHAT'S THERE starts from what the new one holds. Settings are kept in " + Settings.settingsPath + "."
                }
                Repeater {
                    model: Settings.folders
                    Rectangle {
                        id: card
                        required property var modelData
                        width: parent.width
                        height: cardColumn.height + 20
                        radius: 4
                        color: "#E4E5E7"
                        border.color: Theme.deckLine
                        Column {
                            id: cardColumn
                            x: 12
                            y: 10
                            width: parent.width - 24
                            spacing: 6
                            Row {
                                width: parent.width
                                spacing: 10
                                Text {
                                    width: parent.width - 2 * 120 - 20
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: card.modelData.label + (card.modelData.isDefault ? "" : "  (yours)")
                                    color: Theme.ink
                                    font.family: Theme.labelFont
                                    font.pixelSize: 15
                                    font.bold: true
                                }
                                PanelButton {
                                    width: 120
                                    height: 30
                                    faceText: "CHOOSE…"
                                    text: "Pick another folder for " + card.modelData.label
                                    onClicked: folderDialog.pick(card.modelData)
                                }
                                PanelButton {
                                    width: 120
                                    height: 30
                                    enabled: !card.modelData.isDefault
                                    opacity: enabled ? 1 : 0.4
                                    faceText: "DEFAULT"
                                    text: "Back to " + card.modelData.defaultPath
                                    onClicked: moveChoice.ask(card.modelData, "")
                                }
                            }
                            Note { width: parent.width; text: card.modelData.about }
                            PathText { width: parent.width; text: card.modelData.path }
                            Row { // what the next start does
                                visible: card.modelData.pending !== ""
                                width: parent.width
                                spacing: 10
                                Note {
                                    width: parent.width - 110
                                    anchors.verticalCenter: parent.verticalCenter
                                    color: Theme.accentUi
                                    font.bold: true
                                    text: "From the next start: " + card.modelData.pending
                                          + (card.modelData.pendingMove ? " (its files moved there)" : " (what's there)")
                                }
                                PanelButton {
                                    width: 100
                                    height: 28
                                    faceText: "CANCEL"
                                    text: "Keep " + card.modelData.path
                                    onClicked: Settings.cancel(card.modelData.key)
                                }
                            }
                        }
                    }
                }
                Column { // what this start did
                    visible: Settings.startNotes.length > 0
                    width: parent.width
                    spacing: 4
                    Heading { text: "AT THIS START" }
                    Repeater {
                        model: Settings.startNotes
                        Note {
                            required property string modelData
                            width: parent.width
                            text: modelData
                        }
                    }
                }

                Heading { text: "PLUGIN FOLDERS" }
                Note {
                    width: parent.width
                    text: "Where CLAP plugins are looked for: yours first, then the standard places. A change looks again at once."
                }
                Repeater {
                    model: Settings.pluginFolders
                    Row {
                        id: pluginRow
                        required property string modelData
                        required property int index
                        spacing: 10
                        PathText { width: content.width - 120; anchors.verticalCenter: parent.verticalCenter; text: pluginRow.modelData }
                        PanelButton {
                            width: 90
                            height: 28
                            faceText: "REMOVE"
                            text: "Stop looking in " + pluginRow.modelData
                            onClicked: Settings.pluginFolders = Settings.pluginFolders.filter((f, i) => i !== pluginRow.index)
                        }
                    }
                }
                PanelButton {
                    width: 160
                    height: 30
                    faceText: "ADD A FOLDER…"
                    text: "Look for plugins in another folder too"
                    onClicked: folderDialog.pickPlugins()
                }
                Repeater {
                    model: Settings.standardPluginFolders
                    PathText {
                        required property string modelData
                        width: content.width
                        opacity: 0.6
                        text: modelData + "  (standard)"
                    }
                }
                Heading { text: "SOUND FOLDERS" }
                Note {
                    width: parent.width
                    text: "The folders your sound library looks through are on the Sounds screen (ADD FOLDER)."
                }
            }

            // ================= Audio
            Column {
                visible: screen.section === "audio"
                width: parent.width
                spacing: 12
                AudioSettings { width: Math.min(parent.width, 560) }
            }

            // ================= MIDI and keys
            Column {
                visible: screen.section === "midi"
                width: parent.width
                spacing: 12
                Heading { text: "MIDI INPUTS" }
                Note {
                    width: parent.width
                    text: "Which MIDI devices Krate listens to. Every device plugged in (two keyboards, pads, a pedal's keyboard) plays together; tell them apart by channel (a mixer strip's CH). " + (Host.midiConnected ? "Now: MIDI in." : "Now: no MIDI device connected.")
                }
                Switch {
                    on: Settings.midiInputs === ""
                    label: "Every MIDI device"
                    tip: "Connect every MIDI device plugged in"
                    onToggled: Settings.midiInputs = on ? (patternField.text !== "" ? patternField.text : "keyboard") : ""
                }
                Row {
                    spacing: 10
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Only those named"
                        color: Theme.ink
                        opacity: Settings.midiInputs === "" ? 0.5 : 1
                        font.family: Theme.labelFont
                        font.pixelSize: 13
                    }
                    TextField {
                        id: patternField
                        width: 300
                        height: 32
                        enabled: Settings.midiInputs !== ""
                        text: Settings.midiInputs
                        placeholderText: "part of the device's name"
                        onEditingFinished: if (Settings.midiInputs !== "" && text.trim() !== "") Settings.midiInputs = text.trim()
                    }
                }
                Note {
                    width: parent.width
                    text: "A part of the name as it shows in your system's MIDI list (a regular expression, case doesn't matter); any app's MIDI port can match too."
                }
                Switch {
                    on: Settings.programChangeFocus
                    label: "Program Change n focuses the nth instrument in the rack"
                    tip: "A Program Change message switches the instrument in focus (off: it goes to the instrument as a program change)"
                    onToggled: Settings.programChangeFocus = !Settings.programChangeFocus
                }
                KeysSettings { width: Math.min(parent.width, 420) }
            }

            // ================= Updates
            Column {
                visible: screen.section === "updates"
                width: parent.width
                spacing: 12
                Heading { text: "UPDATES" }
                Note {
                    width: parent.width
                    text: "Krate " + Updates.currentVersion + ". New releases come from GitHub; installing one keeps your sounds, songs and settings."
                }
                Switch {
                    on: Settings.autoCheckUpdates
                    label: "Check by itself (when Krate starts, then twice a day)"
                    tip: "Ask GitHub for a newer Krate without being asked"
                    onToggled: Settings.autoCheckUpdates = !Settings.autoCheckUpdates
                }
                Row {
                    spacing: 10
                    readonly property bool busy: Updates.state === "checking" || Updates.state === "installing"
                                                 || Updates.state === "downloading"
                    PanelButton {
                        width: 110
                        height: 32
                        enabled: !parent.busy
                        opacity: enabled ? 1 : 0.5
                        faceText: "CHECK NOW"
                        text: "Ask GitHub whether there's a newer Krate"
                        onClicked: Updates.check()
                    }
                    PanelButton {
                        width: 200
                        height: 32
                        visible: Updates.available && Updates.canInstall
                        enabled: !parent.busy
                        opacity: enabled ? 1 : 0.5
                        faceText: "UPDATE TO " + Updates.latestVersion
                        text: "Download, check and install Krate " + Updates.latestVersion + "; Krate starts again with it"
                        onClicked: Updates.install()
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: Updates.message
                        color: Updates.state === "failed" ? Theme.ledOn : Theme.ink
                        font.family: Theme.labelFont
                        font.pixelSize: 13
                    }
                }
                Note {
                    width: parent.width
                    visible: !Updates.canInstall
                    text: "Updates install with Krate's package (this Krate runs from a build)."
                }
                Text { // what's new
                    width: parent.width
                    visible: Updates.available && Updates.notes !== ""
                    text: Updates.notes
                    textFormat: Text.MarkdownText
                    wrapMode: Text.WordWrap
                    color: Theme.ink
                    font.family: Theme.labelFont
                    font.pixelSize: 12
                }
            }

            // ================= Crash reports
            Column {
                visible: screen.section === "crashes"
                width: parent.width
                spacing: 12
                Heading { text: "CRASH REPORTS" }
                Note {
                    width: parent.width
                    text: Crashes.reports.length === 0
                          ? "None: Krate hasn't crashed (or every report was deleted)."
                          : Crashes.reports.length + (Crashes.reports.length === 1 ? " report" : " reports") + ", the last "
                            + Crashes.reports[0].when + ": " + Crashes.reports[0].summary
                }
                PanelButton {
                    width: 180
                    height: 32
                    faceText: "VIEW REPORTS"
                    text: "What Krate wrote down when it crashed or didn't end properly"
                    onClicked: Crashes.viewing = true
                }
                Note { width: parent.width; text: "Kept in " + Crashes.folder + " (the last 30)." }
            }

            // ================= About
            Column {
                visible: screen.section === "about"
                width: parent.width
                spacing: 8
                Heading { text: "KRATE " + Updates.currentVersion }
                Note {
                    width: parent.width
                    text: Settings.kiosk ? "Running on the player: the kiosk starts Krate at boot and again if it stops." : "Running on a desktop."
                }
                Repeater {
                    model: [
                        { label: "Settings", path: Settings.settingsPath },
                        { label: "The rack's set-up (mixer, focus, MIDI learn, audio)", path: Settings.configFolder + "/rack.json" },
                        { label: "Plugins in their slots", path: Settings.configFolder + "/plugins.json" },
                        { label: "Each instrument's panel and MIDI map", path: Settings.configFolder.replace(/\/krate$/, "") + "/<instrument>" },
                        { label: "This session's log", path: Crashes.folder.replace(/\/crashes$/, "") + "/krate.log" },
                    ]
                    Column {
                        id: aboutRow
                        required property var modelData
                        width: content.width
                        spacing: 2
                        Note { text: aboutRow.modelData.label }
                        PathText { width: parent.width; text: aboutRow.modelData.path }
                    }
                }
            }
        }
    }

    // ---- Picking a folder
    FolderDialog {
        id: folderDialog
        property var info: null // the folder's entry, or null: a plugin folder
        function pick(entry) {
            info = entry
            currentFolder = "file://" + entry.path
            open()
        }
        function pickPlugins() {
            info = null
            open()
        }
        onAccepted: {
            const path = Settings.localPath(selectedFolder)
            if (info === null)
                Settings.pluginFolders = Settings.pluginFolders.concat([path])
            else
                moveChoice.ask(info, path)
        }
    }

    // ---- Move the files, or use what's there?
    Popup {
        id: moveChoice
        property var info: null
        property string path: "" // "" = the default
        readonly property string shown: path !== "" ? path : (info ? info.defaultPath : "")
        readonly property bool canMove: shown !== "" && Settings.emptyOrMissing(shown)
        function ask(entry, to) {
            info = entry
            path = to
            if (entry.atOnce) { // exports: nothing to move, at once
                if (to === "") Settings.resetFolder(entry.key, false)
                else Settings.setFolder(entry.key, to, false)
                return
            }
            open()
        }
        x: (screen.width - width) / 2
        y: 80
        width: Math.min(screen.width - 40, 640)
        padding: 18
        modal: true
        background: Rectangle { radius: 8; color: Theme.lowerDeck; border.color: Theme.deckLine; border.width: 2 }
        Column {
            width: moveChoice.availableWidth
            spacing: 12
            Heading { text: moveChoice.info ? moveChoice.info.label.toUpperCase() : "" }
            Note { width: parent.width; text: "From the next start, use:" }
            PathText { width: parent.width; text: moveChoice.shown }
            Note {
                width: parent.width
                text: moveChoice.canMove ? "It's empty (or new): your files can go there."
                                         : "It has files in it already: Krate can use them, but won't move yours on top of them."
            }
            Row {
                spacing: 10
                PanelButton {
                    width: 210
                    height: 32
                    enabled: moveChoice.canMove
                    opacity: enabled ? 1 : 0.4
                    faceText: "MOVE MY FILES THERE"
                    text: "At the next start, move what's in " + (moveChoice.info ? moveChoice.info.path : "") + " there, then use it"
                    onClicked: {
                        Settings.setFolder(moveChoice.info.key, moveChoice.path, true)
                        moveChoice.close()
                    }
                }
                PanelButton {
                    width: 190
                    height: 32
                    faceText: "USE WHAT'S THERE"
                    text: "At the next start, use that folder as it is (yours stay where they are)"
                    onClicked: {
                        Settings.setFolder(moveChoice.info.key, moveChoice.path, false)
                        moveChoice.close()
                    }
                }
                PanelButton {
                    width: 110
                    height: 32
                    faceText: "CANCEL"
                    text: "Leave it as it is"
                    onClicked: moveChoice.close()
                }
            }
        }
    }
}

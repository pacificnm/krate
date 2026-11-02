pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import KrateRemote

// The player's files (docs/REMOTE.md#files): a folder at a time, files sent
// there (UPLOAD, or dropped from the file manager) and back (DOWNLOAD).
Item {
    id: page

    property var selection: [] // names in the folder open
    readonly property string one: selection.length === 1 ? selection[0] : ""

    function isSelected(name) { return selection.indexOf(name) >= 0 }
    function select(name, mods, index) {
        if (mods & Qt.ControlModifier) {
            const s = selection.slice()
            const at = s.indexOf(name)
            if (at >= 0) s.splice(at, 1); else s.push(name)
            selection = s
        } else if ((mods & Qt.ShiftModifier) && anchorIndex >= 0) {
            const s = []
            for (let i = Math.min(anchorIndex, index); i <= Math.max(anchorIndex, index); ++i)
                s.push(Files.entries[i].name)
            selection = s
            return
        } else {
            selection = [name]
        }
        anchorIndex = index
    }
    property int anchorIndex: -1

    // Sending: names already there are asked about first.
    function send(urls) {
        const clashes = []
        for (const u of urls) {
            const name = decodeURIComponent(u.toString().replace(/\/$/, "").split("/").pop())
            if (Files.exists(name)) clashes.push(name)
        }
        if (clashes.length) {
            replaceDialog.urls = urls
            replaceDialog.names = clashes
            replaceDialog.open()
        } else {
            Files.upload(urls)
        }
    }

    Connections {
        target: Files
        function onListed() { page.selection = []; page.anchorIndex = -1 }
    }

    function sizeText(bytes) {
        if (bytes < 1024) return bytes + " B"
        if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(0) + " KB"
        if (bytes < 1024 * 1024 * 1024) return (bytes / 1024 / 1024).toFixed(1) + " MB"
        return (bytes / 1024 / 1024 / 1024).toFixed(2) + " GB"
    }

    SplitView {
        anchors.fill: parent

        // ---- Places
        ListView {
            id: places
            SplitView.preferredWidth: 230
            SplitView.minimumWidth: 150
            clip: true
            model: Remote.places
            header: Label {
                text: "PLACES"
                padding: 8
                font.bold: true
                color: palette.placeholderText
            }
            delegate: ItemDelegate {
                id: place
                required property var modelData
                width: ListView.view.width
                text: modelData.label
                highlighted: Files.path === modelData.path
                ToolTip.visible: hovered
                ToolTip.text: modelData.path
                ToolTip.delay: 600
                onClicked: Files.open(modelData.path)
            }
        }

        ColumnLayout {
            SplitView.fillWidth: true
            spacing: 4

            // ---- Where, and what to do
            RowLayout {
                Layout.fillWidth: true
                Layout.margins: 6
                ToolButton {
                    text: "↑"
                    font.pixelSize: 18
                    enabled: Files.path !== "/" && Files.path !== ""
                    ToolTip.visible: hovered
                    ToolTip.text: "The folder above"
                    onClicked: Files.up()
                }
                TextField {
                    id: pathField
                    Layout.fillWidth: true
                    font.family: "monospace"
                    text: Files.path
                    onAccepted: Files.open(text.trim())
                }
                ToolButton {
                    text: "⟳"
                    font.pixelSize: 18
                    ToolTip.visible: hovered
                    ToolTip.text: "Look again"
                    onClicked: Files.refresh()
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 6
                Layout.rightMargin: 6
                Button { text: "UPLOAD FILES…"; highlighted: true; onClicked: filesDialog.open() }
                Button { text: "UPLOAD FOLDER…"; onClicked: folderDialog.open() }
                Button {
                    text: "DOWNLOAD"
                    enabled: page.selection.length > 0
                    onClicked: Files.download(page.selection)
                }
                Item { Layout.fillWidth: true }
                Button { text: "NEW FOLDER"; onClicked: nameDialog.ask("", "") }
                Button {
                    text: "RENAME"
                    enabled: page.one !== ""
                    onClicked: nameDialog.ask(page.one, page.one)
                }
                Button {
                    text: "DELETE"
                    enabled: page.selection.length > 0
                    onClicked: deleteDialog.open()
                }
            }
            Label {
                Layout.fillWidth: true
                Layout.leftMargin: 8
                visible: Files.error !== ""
                text: Files.error
                color: "#C0392B"
                wrapMode: Text.WordWrap
            }

            // ---- The folder
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: palette.base
                border.color: drop.containsDrag ? palette.highlight : palette.mid
                border.width: drop.containsDrag ? 2 : 1

                ListView {
                    id: list
                    anchors.fill: parent
                    anchors.margins: 1
                    clip: true
                    model: Files.entries
                    ScrollBar.vertical: ScrollBar {}
                    delegate: Rectangle {
                        id: entry
                        required property var modelData
                        required property int index
                        readonly property bool chosen: page.isSelected(modelData.name)
                        width: ListView.view.width
                        height: 28
                        color: chosen ? palette.highlight : (index % 2 ? palette.alternateBase : palette.base)
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            spacing: 10
                            Label {
                                Layout.fillWidth: true
                                font.bold: entry.modelData.dir
                                text: entry.modelData.name + (entry.modelData.dir ? "/" : "") + (entry.modelData.link ? "  →" : "")
                                elide: Text.ElideMiddle
                                color: entry.chosen ? palette.highlightedText : palette.text
                            }
                            Label {
                                Layout.preferredWidth: 90
                                horizontalAlignment: Text.AlignRight
                                text: entry.modelData.dir ? "" : page.sizeText(entry.modelData.size)
                                color: entry.chosen ? palette.highlightedText : palette.placeholderText
                            }
                            Label {
                                Layout.preferredWidth: 140
                                text: Qt.formatDateTime(new Date(entry.modelData.modified), "yyyy-MM-dd hh:mm")
                                color: entry.chosen ? palette.highlightedText : palette.placeholderText
                            }
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: mouse => page.select(entry.modelData.name, mouse.modifiers, entry.index)
                            onDoubleClicked: {
                                if (entry.modelData.dir)
                                    Files.openEntry(entry.modelData.name)
                                else
                                    Files.download([entry.modelData.name])
                            }
                        }
                    }
                    Label {
                        anchors.centerIn: parent
                        visible: list.count === 0 && !Files.loading
                        text: Files.path === "" ? "" : "Empty. Drop files here to send them, or UPLOAD."
                        color: palette.placeholderText
                    }
                }
                BusyIndicator {
                    anchors.centerIn: parent
                    running: Files.loading
                }
                DropArea {
                    id: drop
                    anchors.fill: parent
                    keys: ["text/uri-list"]
                    onDropped: dropEvent => {
                        if (dropEvent.hasUrls) {
                            page.send(dropEvent.urls)
                            dropEvent.acceptProposedAction()
                        }
                    }
                }
            }

            // ---- Transfers
            Frame {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(220, 70 + transferList.contentHeight)
                visible: Files.transfers.length > 0
                ColumnLayout {
                    anchors.fill: parent
                    RowLayout {
                        Label {
                            font.bold: true
                            text: Files.transferring ? "Sending…" : "Transfers"
                        }
                        Label {
                            Layout.fillWidth: true
                            elide: Text.ElideMiddle
                            color: palette.placeholderText
                            text: "Downloads go to " + Files.downloadFolder
                        }
                        Button { text: "CHANGE…"; onClicked: downloadsDialog.open() }
                        Button { text: "OPEN"; onClicked: Qt.openUrlExternally("file://" + Files.downloadFolder) }
                        Button { text: "CANCEL ALL"; visible: Files.transferring; onClicked: Files.cancelTransfers() }
                        Button { text: "CLEAR"; visible: !Files.transferring; onClicked: Files.clearFinished() }
                    }
                    ListView {
                        id: transferList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: Files.transfers
                        ScrollBar.vertical: ScrollBar {}
                        delegate: RowLayout {
                            id: transfer
                            required property var modelData
                            width: ListView.view.width
                            Label {
                                Layout.preferredWidth: 18
                                text: transfer.modelData.upload ? "↑" : "↓"
                            }
                            Label {
                                Layout.fillWidth: true
                                elide: Text.ElideMiddle
                                text: transfer.modelData.name
                            }
                            ProgressBar {
                                Layout.preferredWidth: 160
                                value: transfer.modelData.total > 0 ? transfer.modelData.done / transfer.modelData.total
                                                                    : (transfer.modelData.state === "done" ? 1 : 0)
                            }
                            Label {
                                Layout.preferredWidth: 220
                                elide: Text.ElideRight
                                color: transfer.modelData.state === "failed" ? "#C0392B" : palette.placeholderText
                                text: transfer.modelData.state === "failed" ? transfer.modelData.error
                                      : transfer.modelData.state === "sending" ? page.sizeText(transfer.modelData.done) + " of " + page.sizeText(transfer.modelData.total)
                                      : transfer.modelData.state
                                ToolTip.visible: transferHover.containsMouse && truncated
                                ToolTip.text: text
                                MouseArea {
                                    id: transferHover
                                    anchors.fill: parent
                                    hoverEnabled: true
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    FileDialog {
        id: filesDialog
        title: "Files to send to " + Files.path
        fileMode: FileDialog.OpenFiles
        onAccepted: page.send(selectedFiles)
    }
    FolderDialog {
        id: folderDialog
        title: "A folder to send to " + Files.path + " (with all it holds)"
        onAccepted: page.send([selectedFolder])
    }
    FolderDialog {
        id: downloadsDialog
        title: "Where downloads go"
        onAccepted: Files.downloadFolder = Files.localPath(selectedFolder)
    }

    Dialog {
        id: nameDialog
        property string from
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        title: from === "" ? "New folder in " + Files.path : "Rename " + from
        standardButtons: Dialog.Ok | Dialog.Cancel
        function ask(f, name) {
            from = f
            nameField.text = name
            open()
            nameField.forceActiveFocus()
            nameField.selectAll()
        }
        TextField {
            id: nameField
            width: 360
            onAccepted: nameDialog.accept()
        }
        onAccepted: from === "" ? Files.makeFolder(nameField.text) : Files.rename(from, nameField.text)
    }

    Dialog {
        id: deleteDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        title: "Delete from the player?"
        width: 460
        standardButtons: Dialog.Yes | Dialog.Cancel
        Label {
            width: deleteDialog.availableWidth
            wrapMode: Text.WordWrap
            text: (page.selection.length === 1 ? page.selection[0] : page.selection.length + " things")
                  + " in " + Files.path + " (folders with all they hold). This can't be undone."
        }
        onAccepted: Files.remove(page.selection)
    }

    Dialog {
        id: replaceDialog
        property var urls: []
        property var names: []
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        title: "Replace what's there?"
        width: 460
        standardButtons: Dialog.Yes | Dialog.Cancel
        Label {
            width: replaceDialog.availableWidth
            wrapMode: Text.WordWrap
            text: replaceDialog.names.join(", ") + (replaceDialog.names.length === 1 ? " is" : " are")
                  + " in " + Files.path + " already: the ones sent replace them (a folder's files go into it)."
        }
        onAccepted: Files.upload(urls)
    }
}

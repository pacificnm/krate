pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Krate

// A plugin slot's panel (docs/DAW.md, D7) at design size 900 x 450: the CLAP
// plugin it holds (LOAD picks one from those on the system, UNLOAD empties
// it), its editor in a window of its own (EDITOR), its knobs, and for an
// effect where its sound comes from (SOURCE: another device's output).
Item {
    id: panel

    property int rackDevice: -1 // set by the rack when it loads the panel
    readonly property int slot: Plugins.slotOf(rackDevice)
    readonly property int rev: Plugins.revision
    readonly property string name: rev >= 0 ? Plugins.pluginName(slot) : ""
    readonly property bool loaded: name !== ""
    readonly property bool effect: rev >= 0 && Plugins.isEffect(slot)

    width: 900
    height: 450

    Rectangle {
        anchors.fill: parent
        radius: 6
        border.color: Theme.deckLine
        border.width: 2
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.silverLight }
            GradientStop { position: 1.0; color: Theme.silverDark }
        }
    }

    Text {
        x: 36
        y: 22
        text: "Plugin " + (panel.slot + 1)
        color: Theme.ink
        font.family: Theme.labelFont
        font.pixelSize: 44
        font.bold: true
        font.italic: true
    }
    PanelLabel {
        x: 40
        y: 80
        width: 380
        horizontalAlignment: Text.AlignLeft
        elide: Text.ElideRight
        text: panel.loaded ? (panel.name + (Plugins.pluginVendor(panel.slot) !== "" ? " · " + Plugins.pluginVendor(panel.slot) : "")
                              + (panel.effect ? "  (EFFECT)" : ""))
                           : "EMPTY: LOAD A CLAP PLUGIN"
        font.pixelSize: 14
    }

    Row {
        x: 440
        y: 34
        spacing: 8
        PanelButton {
            width: 120
            height: 44
            faceText: panel.loaded ? "CHANGE" : "LOAD"
            text: "Pick a CLAP plugin for this slot"
            onClicked: picker.open()
        }
        PanelButton {
            width: 100
            height: 44
            faceText: "UNLOAD"
            text: "Empty this slot"
            enabled: panel.loaded
            opacity: enabled ? 1 : 0.5
            onClicked: Plugins.unload(panel.slot)
        }
        PanelButton {
            width: 110
            height: 44
            faceText: "EDITOR"
            text: "Open the plugin's own window"
            enabled: panel.rev >= 0 && Plugins.hasEditor(panel.slot)
            opacity: enabled ? 1 : 0.5
            tint: panel.rev >= 0 && Plugins.editorOpen(panel.slot) ? Theme.labelOrange : "transparent"
            onClicked: Plugins.toggleEditor(panel.slot)
        }
    }

    Text { // why a load or an editor didn't work
        x: 440
        y: 86
        width: 430
        elide: Text.ElideRight
        visible: text !== ""
        text: panel.rev >= 0 ? Plugins.message(panel.slot) : ""
        color: Theme.ledOn
        font.family: Theme.labelFont
        font.pixelSize: 13
        font.bold: true
    }

    // An effect's source: the mixer's input selector.
    Row {
        x: 40
        y: 112
        spacing: 6
        visible: panel.effect
        PanelLabel { anchors.verticalCenter: parent.verticalCenter; text: "SOURCE" }
        PanelButton {
            width: 34
            height: 36
            text: "Previous input source"
            faceText: "◂"
            onClicked: Rack.stepInputSource(panel.rackDevice, -1)
        }
        Rectangle {
            width: 160
            height: 36
            radius: 4
            color: Theme.lcdBg
            border.color: Theme.deckLine
            Text {
                anchors.centerIn: parent
                text: Rack.mixerRevision >= 0 && panel.effect ? Rack.inputSourceName(panel.rackDevice) : ""
                color: Theme.lcdInk
                font.family: Theme.labelFont
                font.pixelSize: 14
                font.bold: true
            }
        }
        PanelButton {
            width: 34
            height: 36
            text: "Next input source"
            faceText: "▸"
            onClicked: Rack.stepInputSource(panel.rackDevice, 1)
        }
        PanelLabel {
            anchors.verticalCenter: parent.verticalCenter
            text: "(mute the source's own strip to hear only the effect)"
            font.pixelSize: 11
            font.bold: false
        }
    }

    // Its knobs: a bar each, dragged (double click: as it was).
    Rectangle {
        x: 40
        y: 158
        width: 820
        height: 270
        radius: 4
        color: Theme.lcdBg
        border.color: Theme.deckLine
        clip: true

        Text {
            anchors.centerIn: parent
            visible: knobs.count === 0
            text: panel.loaded ? "This plugin has no knobs to show." : "No plugin in this slot."
            color: Theme.lcdInk
            font.family: Theme.labelFont
            font.pixelSize: 15
        }
        GridView {
            id: knobs
            anchors.fill: parent
            anchors.margins: 8
            cellWidth: width / 3
            cellHeight: 40
            model: panel.loaded ? Plugins.paramCount(panel.slot) : 0
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
            delegate: Item {
                id: knob
                required property int index
                readonly property real value: panel.rev >= 0 ? Plugins.param(panel.slot, index) : 0
                width: knobs.cellWidth - 10
                height: 36
                Text {
                    width: parent.width - 44
                    elide: Text.ElideRight
                    text: panel.rev >= 0 ? Plugins.paramLabel(panel.slot, knob.index) : ""
                    color: Theme.lcdInk
                    font.family: Theme.labelFont
                    font.pixelSize: 12
                    font.bold: true
                }
                Text {
                    anchors.right: parent.right
                    text: Math.round(knob.value * 100) + "%"
                    color: Theme.lcdInk
                    font.family: "monospace"
                    font.pixelSize: 11
                }
                Rectangle {
                    y: 18
                    width: parent.width
                    height: 12
                    radius: 3
                    color: Theme.ledOff
                    Rectangle {
                        width: parent.width * knob.value
                        height: parent.height
                        radius: 3
                        color: Theme.labelOrange
                    }
                    MouseArea {
                        anchors.fill: parent
                        preventStealing: true
                        function set(x) { Plugins.setParam(panel.slot, knob.index, Math.max(0, Math.min(1, x / width))) }
                        onPressed: mouse => set(mouse.x)
                        onPositionChanged: mouse => set(mouse.x)
                    }
                    Accessible.role: Accessible.Slider
                    Accessible.name: Plugins.paramLabel(panel.slot, knob.index)
                }
            }
        }
    }

    // LOAD: the CLAP plugins found on the system.
    Popup {
        id: picker
        x: (panel.width - width) / 2
        y: 30
        width: 640
        height: 390
        padding: 14
        modal: true
        background: Rectangle { radius: 8; color: Theme.lowerDeck; border.color: Theme.deckLine; border.width: 2 }
        Column {
            anchors.fill: parent
            spacing: 8
            Row {
                spacing: 12
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "CLAP PLUGINS ON THIS COMPUTER"
                    color: Theme.ink
                    font.family: Theme.labelFont
                    font.pixelSize: 14
                    font.bold: true
                }
                PanelButton {
                    width: 90
                    height: 28
                    faceText: "RESCAN"
                    text: "Look for plugins again"
                    onClicked: Plugins.rescan()
                }
            }
            ListView {
                id: list
                width: parent.width
                height: parent.height - 40
                clip: true
                spacing: 2
                model: picker.opened ? Plugins.available : []
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                delegate: Rectangle {
                    id: row
                    required property var modelData
                    width: list.width - 12
                    height: 34
                    radius: 3
                    color: rowMouse.containsMouse ? Theme.silverLight : "transparent"
                    Text {
                        x: 8
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - 120
                        elide: Text.ElideRight
                        text: row.modelData.name + (row.modelData.vendor !== "" ? "  ·  " + row.modelData.vendor : "")
                        color: Theme.ink
                        font.family: Theme.labelFont
                        font.pixelSize: 14
                    }
                    Text {
                        anchors.right: parent.right
                        anchors.rightMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        text: row.modelData.kind.toUpperCase()
                        color: Theme.ink
                        opacity: 0.6
                        font.family: Theme.labelFont
                        font.pixelSize: 11
                        font.bold: true
                    }
                    MouseArea {
                        id: rowMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            Plugins.load(panel.slot, row.modelData.path, row.modelData.id)
                            picker.close()
                        }
                    }
                }
                Text {
                    anchors.centerIn: parent
                    width: parent.width - 40
                    visible: list.count === 0
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    text: "No CLAP plugins found in ~/.clap, /usr/lib/clap or $CLAP_PATH. Install some (Debian: dpf-plugins-clap, lsp-plugins-clap, dragonfly-reverb-clap), then RESCAN."
                    color: Theme.ink
                    font.family: Theme.labelFont
                    font.pixelSize: 14
                }
            }
        }
    }
}

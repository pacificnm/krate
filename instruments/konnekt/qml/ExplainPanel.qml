pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Krate

// The explanation a right-click opens (docs/MANUAL.md#explanations): what a
// module, knob, jack or button is, how it works, why you'd want it, where it
// goes and what to try (Konnekt.explain), with what it's doing on the rack now.
// It follows the rack live; links open other explanations, ◂ goes back.
Rectangle {
    id: panel

    property string path: ""
    property var history: []
    signal showScope(int slot, int jack)

    readonly property var signalColors: ["#E8873A", "#4FA3E8", "#6FCF5A", "#B57BE8"] // audio, pitch, gate, mod
    readonly property var card: path !== "" && Konnekt.revision + Konnekt.knobRevision >= 0 ? Konnekt.explain(path) : ({})
    readonly property color accent: card.signal !== undefined && card.signal >= 0 ? signalColors[card.signal] : "#F2B632"

    function show(p) {
        if (p === path)
            return
        if (path !== "")
            history = history.concat([path])
        path = p
        scroll.contentY = 0
    }
    function back() {
        path = history[history.length - 1]
        history = history.slice(0, -1)
        scroll.contentY = 0
    }
    function close() {
        path = ""
        history = []
    }

    visible: path !== ""
    width: 440
    color: "#202226"
    onCardChanged: if (path !== "" && card.title === undefined) close() // its module was removed

    Rectangle { // the edge, in the colour of what's explained
        width: 3
        height: parent.height
        color: panel.accent
    }
    MouseArea { anchors.fill: parent } // clicks stay here, off the rack

    // Header: kind, title, the module it's on; back and close
    Column {
        id: header
        x: 18
        y: 12
        width: parent.width - 100
        spacing: 2
        Text {
            width: parent.width
            elide: Text.ElideRight
            text: (panel.card.kind ?? "").toUpperCase()
            color: panel.accent
            font.family: Theme.labelFont
            font.pixelSize: 11
            font.bold: true
            font.letterSpacing: 1.5
        }
        Text {
            width: parent.width
            wrapMode: Text.WordWrap
            text: panel.card.title ?? ""
            color: "#F0EDE6"
            font.family: Theme.labelFont
            font.pixelSize: 22
            font.bold: true
        }
        Text {
            visible: panel.card.module !== undefined && panel.card.kind !== undefined && !panel.card.kind.startsWith("Module")
            text: "on " + (panel.card.module ?? "") + "  ›"
            color: moduleLink.containsMouse ? "#F0EDE6" : "#9A968E"
            font.family: Theme.labelFont
            font.pixelSize: 12
            font.underline: moduleLink.containsMouse
            MouseArea {
                id: moduleLink
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: panel.show(panel.card.moduleId)
            }
        }
    }
    Row {
        anchors.right: parent.right
        anchors.rightMargin: 12
        y: 12
        spacing: 6
        PanelButton { width: 30; height: 26; dark: true; faceText: "◂"; text: "Back"; enabled: panel.history.length > 0; onClicked: panel.back() }
        PanelButton { width: 30; height: 26; dark: true; faceText: "✕"; text: "Close (Esc)"; onClicked: panel.close() }
    }

    Flickable {
        id: scroll
        x: 18
        y: header.y + header.height + 12
        width: parent.width - 30
        height: parent.height - y - 8
        contentHeight: body.height + 16
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { }

        Column {
            id: body
            width: scroll.width - 12
            spacing: 14

            Text { // the one-line help, as in the help bar
                width: parent.width
                visible: text !== ""
                wrapMode: Text.WordWrap
                text: panel.card.lead ?? ""
                color: "#DAD5CB"
                font.family: Theme.labelFont
                font.pixelSize: 13
                font.italic: true
                lineHeight: 1.15
            }

            // On the rack now: its value, range, cables
            Rectangle {
                width: parent.width
                height: facts.height + 16
                visible: (panel.card.facts ?? []).length > 0
                radius: 4
                color: "#17181B"
                Column {
                    id: facts
                    x: 10
                    y: 8
                    width: parent.width - 20
                    spacing: 4
                    Repeater {
                        model: panel.card.facts ?? []
                        Row {
                            id: factRow
                            required property var modelData
                            spacing: 8
                            Text {
                                width: 92
                                text: factRow.modelData.label
                                color: "#9A968E"
                                font.family: Theme.labelFont
                                font.pixelSize: 12
                                font.bold: true
                            }
                            Text {
                                width: facts.width - 100
                                wrapMode: Text.WordWrap
                                text: factRow.modelData.text + (factRow.modelData.link !== "" ? "  ›" : "")
                                color: factRow.modelData.link !== "" ? (factLink.containsMouse ? "#F0EDE6" : "#5FD0FF") : "#F0EDE6"
                                font.family: Theme.labelFont
                                font.pixelSize: 12
                                font.underline: factLink.containsMouse
                                MouseArea {
                                    id: factLink
                                    anchors.fill: parent
                                    enabled: factRow.modelData.link !== ""
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: panel.show(factRow.modelData.link)
                                }
                            }
                        }
                    }
                }
            }

            PanelButton {
                visible: (panel.card.scopeSlot ?? -1) >= 0
                width: 160
                height: 30
                dark: true
                faceText: "SHOW ON SCOPE"
                text: "Watch this output on the scope (bottom right)"
                onClicked: panel.showScope(panel.card.scopeSlot, panel.card.scopeJack)
            }

            // The texts
            Repeater {
                model: panel.card.sections ?? []
                Column {
                    id: section
                    required property var modelData
                    width: body.width
                    spacing: 4
                    Text {
                        text: section.modelData.heading.toUpperCase()
                        color: panel.accent
                        font.family: Theme.labelFont
                        font.pixelSize: 11
                        font.bold: true
                        font.letterSpacing: 1.5
                    }
                    Text {
                        width: parent.width
                        wrapMode: Text.WordWrap
                        text: section.modelData.text
                        color: "#E4E0D8"
                        font.family: Theme.labelFont
                        font.pixelSize: 14
                        lineHeight: 1.2
                    }
                }
            }

            // Its parts, and what to read next
            Repeater {
                model: panel.card.groups ?? []
                Column {
                    id: group
                    required property var modelData
                    width: body.width
                    spacing: 6
                    Text {
                        text: group.modelData.heading.toUpperCase()
                        color: "#9A968E"
                        font.family: Theme.labelFont
                        font.pixelSize: 11
                        font.bold: true
                        font.letterSpacing: 1.5
                    }
                    Flow {
                        width: parent.width
                        spacing: 6
                        Repeater {
                            model: group.modelData.links
                            Rectangle {
                                id: chip
                                required property var modelData
                                width: chipText.implicitWidth + (modelData.signal >= 0 ? 30 : 18)
                                height: 26
                                radius: 13
                                color: chipArea.containsMouse ? "#3A3D44" : "#2C2E33"
                                border.color: "#3A3D44"
                                Rectangle {
                                    visible: chip.modelData.signal >= 0
                                    x: 9
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 8
                                    height: 8
                                    radius: 4
                                    color: chip.modelData.signal >= 0 ? panel.signalColors[chip.modelData.signal] : "transparent"
                                }
                                Text {
                                    id: chipText
                                    x: chip.modelData.signal >= 0 ? 21 : 9
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: chip.modelData.label
                                    color: "#DAD5CB"
                                    font.family: Theme.labelFont
                                    font.pixelSize: 12
                                }
                                MouseArea {
                                    id: chipArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: panel.show(chip.modelData.path)
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

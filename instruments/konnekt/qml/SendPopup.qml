pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Krate

// Send to an instrument (docs/MANUAL.md#send-to-an-instrument): the patch as
// a preset of Kalliope, Kobalt or Krimson. The synths are listed best fit
// first; the report says what comes across, what changes and what can't; the
// grid shows the instrument's 64 slots (what's there now) to choose one.
Popup {
    id: send

    property int target: 0
    property int slot: 16
    property string defaultName: "Konnekt"
    property string result: ""
    property var targets: []
    property var preview: ({})
    property var names: []

    function refresh() {
        targets = Konnekt.sendTargets()
        if (targets.length > 0 && result === "")
            target = targets[0].target
        load()
    }
    function load() {
        preview = Konnekt.sendPreview(target)
        names = Konnekt.instrumentSlots(target)
        slot = Konnekt.suggestedSlot(target)
    }

    modal: true
    padding: 16
    width: Math.min(parent.width - 40, 800)
    height: Math.min(parent.height - 80, body.implicitHeight + 2 * padding)
    x: (parent.width - width) / 2
    y: 60
    background: Rectangle { radius: 8; color: "#23252A"; border.color: "#3A3D44" }
    onOpened: { result = ""; nameField.text = defaultName; refresh() }

    Flickable {
        anchors.fill: parent
        contentHeight: body.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { }

        Column {
            id: body
            width: parent.width - 12
            spacing: 12

            Text {
                width: parent.width
                wrapMode: Text.WordWrap
                text: "SEND TO AN INSTRUMENT: the patch becomes one of its presets. The synths have a fixed shape, so the patch is fitted to it: below is what comes across."
                color: "#F2B632"
                font.family: Theme.labelFont
                font.pixelSize: 12
                font.bold: true
            }

            // The synths, best fit first
            Row {
                spacing: 8
                Repeater {
                    model: send.targets
                    PanelButton {
                        required property var modelData
                        required property int index
                        width: 220
                        height: 34
                        dark: true
                        faceText: modelData.name + "   " + modelData.score + " %" + (index === 0 ? "   ★" : "")
                        text: modelData.name + ": " + modelData.score + " % of the patch comes across" + (index === 0 ? " (the best fit)" : "")
                        tint: send.target === modelData.target ? "#8A6A10" : "transparent"
                        onClicked: { send.target = modelData.target; send.result = ""; send.load() }
                    }
                }
            }

            // The report
            Repeater {
                model: [{ key: "exact", title: "COMES ACROSS", color: "#7ED957" },
                        { key: "approx", title: "CHANGED TO FIT", color: "#F2B632" },
                        { key: "lost", title: "CAN'T COME ACROSS", color: "#FF6A55" }]
                Column {
                    id: section
                    required property var modelData
                    readonly property var lines: send.preview[modelData.key] ?? []
                    visible: lines.length > 0
                    width: body.width
                    spacing: 3
                    Text {
                        text: section.modelData.title
                        color: section.modelData.color
                        font.family: Theme.labelFont
                        font.pixelSize: 11
                        font.bold: true
                        font.letterSpacing: 1.5
                    }
                    Repeater {
                        model: section.lines
                        Text {
                            required property string modelData
                            width: body.width
                            wrapMode: Text.WordWrap
                            text: "•  " + modelData
                            color: "#DAD5CB"
                            font.family: Theme.labelFont
                            font.pixelSize: 12
                        }
                    }
                }
            }

            // Where it goes: the instrument's 64 slots, as they are now
            Text {
                text: "INTO " + (send.targets.find(t => t.target === send.target)?.name ?? "").toUpperCase()
                      + "  ·  BANK " + (Math.floor(send.slot / 8) + 1) + ", " + (send.target === 2 ? "PROGRAM " : "PATCH ") + (send.slot % 8 + 1)
                      + (send.names[send.slot] ? "  (replaces " + send.names[send.slot] + ")" : "")
                color: "#9A968E"
                font.family: Theme.labelFont
                font.pixelSize: 11
                font.bold: true
            }
            Grid {
                columns: 8
                spacing: 4
                Repeater {
                    model: 64
                    Rectangle {
                        id: cell
                        required property int index
                        readonly property string label: send.names[index] ?? ""
                        width: (body.width - 7 * 4) / 8
                        height: 24
                        radius: 3
                        color: send.slot === index ? "#F2B632" : cellArea.containsMouse ? "#3A3D44" : "#2C2E33"
                        border.color: "#3A3D44"
                        Text {
                            anchors.fill: parent
                            anchors.margins: 3
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                            text: (Math.floor(cell.index / 8) + 1) + "" + (cell.index % 8 + 1) + "  " + (cell.label === "Init" ? "" : cell.label)
                            color: send.slot === cell.index ? "#111" : cell.label === "Init" || cell.label === "" ? "#7A766E" : "#DAD5CB"
                            font.family: Theme.labelFont
                            font.pixelSize: 10
                        }
                        MouseArea {
                            id: cellArea
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: { send.slot = cell.index; send.result = "" }
                        }
                    }
                }
            }

            Row {
                spacing: 10
                TextField {
                    id: nameField
                    width: 260
                    maximumLength: 16
                    placeholderText: "a name (16 letters)"
                }
                PanelButton {
                    width: 120
                    height: 34
                    dark: true
                    faceText: "SEND"
                    text: "Send the patch into this slot (what's there is replaced)"
                    enabled: nameField.text.trim() !== ""
                    onClicked: {
                        const name = (send.targets.find(t => t.target === send.target)?.name ?? "")
                        const where = (Math.floor(send.slot / 8) + 1) + "" + (send.slot % 8 + 1)
                        send.result = Konnekt.sendTo(send.target, send.slot, nameField.text)
                            ? "Sent to " + name + " " + where + ". If " + name + " (or Krate) is open it's there now, selected; if not, it arrives the next time it starts."
                            : "Couldn't send it: see the log."
                        send.names = Konnekt.instrumentSlots(send.target)
                    }
                }
                PanelButton { width: 90; height: 34; dark: true; faceText: "CLOSE"; text: "Close"; onClicked: send.close() }
            }
            Text {
                visible: send.result !== ""
                width: parent.width
                wrapMode: Text.WordWrap
                text: send.result
                color: "#7ED957"
                font.family: Theme.labelFont
                font.pixelSize: 12
            }
        }
    }
}

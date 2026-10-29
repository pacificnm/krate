pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Krate

// The pedals in front of the amp, left to right as the guitar goes through
// them (docs/PEDALS.md). Drag a pedal by its body to move it; the footswitch
// turns it on and off (learnable: "pedal1".."pedal8" by position); × takes it
// off the board; + adds one.
Rectangle {
    id: board

    readonly property real slot: 164 // a pedal and the gap after it
    readonly property color cream: "#E9DFC4"

    radius: 8
    color: "#1B1A19"
    border.color: "#3A3B3E"

    // The board's carpet.
    Rectangle {
        anchors.fill: parent
        anchors.margins: 6
        radius: 6
        color: "#24221F"
    }

    Text {
        x: 16
        y: 8
        text: "PEDALS  ·  guitar → " + (Pedals.count > 0 ? Pedals.count + (Pedals.count === 1 ? " pedal" : " pedals") : "no pedals") + " → amp"
        color: "#8F897A"
        font.family: Theme.labelFont
        font.pixelSize: 12
        font.bold: true
    }

    Item {
        id: lane
        x: 16
        y: 30
        width: parent.width - 32
        height: parent.height - 40

        Repeater {
            model: Pedals.layoutRevision >= 0 ? Pedals.count : 0

            Rectangle {
                id: pedal
                required property int index
                readonly property var info: Pedals.layoutRevision >= 0 ? Pedals.pedals()[index] : ({})
                readonly property bool on: Pedals.revision >= 0 && Pedals.isOn(index)

                Binding on x { // where its place is, except while it's being dragged
                    when: !bodyDrag.drag.active
                    value: pedal.index * board.slot
                    restoreMode: Binding.RestoreNone
                }
                width: board.slot - 12
                height: lane.height
                z: bodyDrag.drag.active ? 10 : 0
                radius: 10
                border.color: Qt.darker(info.colour ?? "#444", 1.8)
                border.width: 2
                gradient: Gradient {
                    GradientStop { position: 0.0; color: Qt.lighter(pedal.info.colour ?? "#444", 1.15) }
                    GradientStop { position: 1.0; color: Qt.darker(pedal.info.colour ?? "#444", 1.25) }
                }
                Accessible.role: Accessible.Grouping
                Accessible.name: (info.name ?? "") + ", pedal " + (index + 1)

                // Drag by the body to move it along the board.
                MouseArea {
                    id: bodyDrag
                    anchors.fill: parent
                    drag.target: pedal
                    drag.axis: Drag.XAxis
                    drag.minimumX: 0
                    drag.maximumX: (Pedals.count - 1) * board.slot
                    cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                    onReleased: {
                        const to = Math.max(0, Math.min(Pedals.count - 1, Math.round(pedal.x / board.slot)))
                        if (to !== pedal.index)
                            Pedals.move(pedal.index, to)
                        else
                            pedal.x = pedal.index * board.slot
                    }
                }

                // Its knobs.
                Row {
                    x: (parent.width - width) / 2
                    y: 20
                    spacing: 2
                    Repeater {
                        model: pedal.info.knobs ?? []
                        Column {
                            id: knobColumn
                            required property var modelData
                            required property int index
                            spacing: 1
                            Knob {
                                width: 44
                                height: 44
                                name: (pedal.info.name ?? "") + " " + knobColumn.modelData.label
                                value: Pedals.revision >= 0 ? Pedals.knob(pedal.index, knobColumn.index) : 0
                                defaultValue: knobColumn.modelData.defaultValue
                                displayText: Pedals.knobText(pedal.index, knobColumn.index, value)
                                capColor: "#1E1E1E"
                                capShadeColor: "#000"
                                pointerColor: "#F2F2F2"
                                onMoved: v => Pedals.setKnob(pedal.index, knobColumn.index, v)
                            }
                            Text {
                                width: 44
                                horizontalAlignment: Text.AlignHCenter
                                text: knobColumn.modelData.label
                                color: "#101010"
                                font.family: Theme.labelFont
                                font.pixelSize: 9
                                font.bold: true
                            }
                        }
                    }
                }

                Led {
                    x: (parent.width - width) / 2
                    y: 88
                    width: 10
                    height: 10
                    on: pedal.on
                }

                Text { // the name, on a plate
                    x: 8
                    y: 106
                    width: parent.width - 16
                    horizontalAlignment: Text.AlignHCenter
                    text: pedal.info.name ?? ""
                    color: "#F4F1E6"
                    style: Text.Raised
                    styleColor: "#80000000"
                    font.family: Theme.labelFont
                    font.pixelSize: 19
                    font.bold: true
                    font.italic: true
                    wrapMode: Text.WordWrap
                }

                // The footswitch.
                Rectangle {
                    id: footswitch
                    x: (parent.width - width) / 2
                    y: parent.height - height - 26
                    width: 54
                    height: 54
                    radius: width / 2
                    border.color: "#555"
                    border.width: 2
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: footswitchArea.pressed ? "#9A9A9A" : "#E4E4E4" }
                        GradientStop { position: 1.0; color: "#7A7A7A" }
                    }
                    Accessible.role: Accessible.Button
                    Accessible.name: "Footswitch: " + (pedal.info.name ?? "") + (pedal.on ? ", on" : ", off")
                    MouseArea {
                        id: footswitchArea
                        anchors.fill: parent
                        onClicked: Pedals.toggle(pedal.index)
                    }
                    LearnOverlay {
                        anchors.fill: parent
                        paramKey: "pedal" + (pedal.index + 1)
                    }
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: parent.height - 22
                    text: (pedal.index + 1).toString()
                    color: "#101010"
                    font.family: Theme.labelFont
                    font.pixelSize: 11
                    font.bold: true
                }

                // Off the board.
                Text {
                    x: parent.width - 18
                    y: 0
                    text: "×"
                    color: removeArea.containsMouse ? "#FFFFFF" : "#B0000000"
                    font.family: Theme.labelFont
                    font.pixelSize: 18
                    font.bold: true
                    Accessible.role: Accessible.Button
                    Accessible.name: "Remove " + (pedal.info.name ?? "")
                    MouseArea {
                        id: removeArea
                        anchors.fill: parent
                        anchors.margins: -6
                        hoverEnabled: true
                        onClicked: Pedals.remove(pedal.index)
                    }
                }
            }
        }

        // + : another pedal at the end of the board.
        Rectangle {
            visible: Pedals.count < Pedals.maxPedals
            x: Pedals.count * board.slot
            width: 110
            height: lane.height
            radius: 10
            color: addArea.containsMouse ? "#2E2C29" : "transparent"
            border.color: "#5A5650"
            border.width: 2

            Column {
                anchors.centerIn: parent
                spacing: 6
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "+"
                    color: board.cream
                    font.pixelSize: 40
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "ADD PEDAL"
                    color: board.cream
                    font.family: Theme.labelFont
                    font.pixelSize: 12
                    font.bold: true
                }
            }
            MouseArea {
                id: addArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: addMenu.popup()
            }
            Accessible.role: Accessible.Button
            Accessible.name: "Add a pedal"

            Menu {
                id: addMenu
                Repeater {
                    model: Pedals.types
                    MenuItem {
                        required property var modelData
                        text: modelData.name
                        onTriggered: Pedals.add(modelData.type)
                    }
                }
            }
        }

        Text {
            visible: Pedals.count === 0
            x: 130
            anchors.verticalCenter: parent.verticalCenter
            text: "Add a pedal with +. Drag pedals to reorder them; the footswitch turns one on and off."
            color: "#8F897A"
            font.family: Theme.labelFont
            font.pixelSize: 14
        }
    }
}

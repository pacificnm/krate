pragma ComponentBehavior: Bound

import QtQuick

// Recessed LCD readout with up/down arrow buttons on the right.
Item {
    id: field

    property string text: ""
    property string name: ""   // accessible name, e.g. "Pattern"

    signal up()
    signal down()

    width: 340
    height: 40

    Accessible.role: Accessible.StaticText
    Accessible.name: name !== "" ? name + ": " + text : text

    Rectangle {
        width: field.width - 34
        height: field.height
        radius: 3
        color: Theme.lcdBg
        border.color: "#6E716B"
        border.width: 2

        Rectangle { // inner top shadow
            x: 2
            y: 2
            width: parent.width - 4
            height: 3
            color: "#30000000"
        }

        Text {
            x: 12
            anchors.verticalCenter: parent.verticalCenter
            text: field.text
            color: Theme.lcdInk
            font.family: "monospace"
            font.pixelSize: field.height * 0.5
        }
    }

    Column {
        x: field.width - 30
        width: 30

        Repeater {
            model: ["▲", "▼"]

            Rectangle {
                required property int index
                required property string modelData

                width: 30
                height: field.height / 2
                Accessible.role: Accessible.Button
                Accessible.name: (field.name !== "" ? field.name + " " : "") + (index === 0 ? "up" : "down")
                color: arrowMouse.pressed ? Theme.buttonLightShade : Theme.buttonLight
                border.color: Theme.buttonEdge

                Text {
                    anchors.centerIn: parent
                    text: parent.modelData
                    color: Theme.ink
                    font.pixelSize: 11
                }

                MouseArea {
                    id: arrowMouse
                    anchors.fill: parent
                    onClicked: parent.index === 0 ? field.up() : field.down()
                }
            }
        }
    }
}

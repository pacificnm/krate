pragma ComponentBehavior: Bound

import QtQuick

// The 16 step buttons with LEDs. Lit = selected step; playStep pulses.
Item {
    id: row

    property int selectedStep: 0
    property int playStep: -1

    signal stepClicked(int step)

    width: 710
    height: 84

    readonly property real pitch: width / 16

    Repeater {
        model: 16

        Item {
            id: cell

            required property int index

            x: index * row.pitch
            width: row.pitch
            height: row.height

            Led {
                anchors.horizontalCenter: parent.horizontalCenter
                y: 2
                on: cell.index === row.selectedStep || cell.index === row.playStep
            }

            PanelButton {
                anchors.horizontalCenter: parent.horizontalCenter
                y: 22
                width: 34
                height: 32
                text: "Step " + (cell.index + 1)
                onClicked: row.stepClicked(cell.index)
            }

            PanelLabel {
                centerX: cell.width / 2
                y: 60
                text: cell.index + 1
                font.pixelSize: 13
            }
        }
    }
}

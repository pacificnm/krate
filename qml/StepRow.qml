pragma ComponentBehavior: Bound

import QtQuick

// The 16 step buttons with LEDs. The selected step's LED is steady; the
// playhead's LED flashes bright as it enters a step and settles dimmer.
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
            readonly property bool isPlayhead: index === row.playStep

            x: index * row.pitch
            width: row.pitch
            height: row.height

            onIsPlayheadChanged: {
                if (isPlayhead)
                    flash.restart()
                else
                    led.opacity = 1.0
            }

            Led {
                id: led
                anchors.horizontalCenter: parent.horizontalCenter
                y: 2
                on: cell.index === row.selectedStep || cell.isPlayhead
            }

            OpacityAnimator { // runs on the render thread
                id: flash
                target: led
                from: 1.0
                to: 0.45
                duration: 110
                easing.type: Easing.OutQuad
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

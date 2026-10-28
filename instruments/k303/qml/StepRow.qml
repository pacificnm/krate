pragma ComponentBehavior: Bound

import QtQuick
import Krate

// The 16 step buttons with LEDs, one per row of the Pattern model. The selected
// step's LED is steady; the playhead's LED flashes bright as it enters a step
// and settles dimmer. Each button shows its step's note; steps past the
// pattern length are dimmed.
Item {
    id: row

    property int selectedStep: 0
    property int playStep: -1

    signal stepClicked(int step)

    width: 710
    height: 84

    readonly property real pitch: width / 16

    Repeater {
        model: Pattern

        Item {
            id: cell

            required property int index
            required property string label
            required property bool active
            required property bool accent
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
                width: 38
                height: 32
                opacity: cell.active ? 1.0 : 0.35
                faceText: cell.label
                text: "Step " + (cell.index + 1) + (cell.label ? " " + cell.label : " rest")
                      + (cell.accent ? " accent" : "")
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

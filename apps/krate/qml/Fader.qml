import QtQuick
import Krate

// Vertical mixer fader: drag the cap or use the wheel; double-click = unity.
// Like Knob, it reports moves (moved) and never assigns its own value.
Item {
    id: fader

    property real value: 0.8
    property real unity: 0.8
    property string displayText: ""
    signal moved(real value)

    width: 36
    height: 120
    Accessible.role: Accessible.Slider
    Accessible.name: displayText

    function setValue(v) {
        v = Math.max(0, Math.min(1, v))
        if (v !== value)
            moved(v)
    }

    Rectangle { // slot
        anchors.horizontalCenter: parent.horizontalCenter
        y: 6
        width: 6
        height: fader.height - 12
        radius: 3
        color: Theme.band
    }
    Rectangle { // unity mark
        anchors.horizontalCenter: parent.horizontalCenter
        y: 6 + (1 - fader.unity) * (fader.height - 12)
        width: 22
        height: 2
        color: Theme.labelOrange
    }
    Rectangle { // cap
        anchors.horizontalCenter: parent.horizontalCenter
        y: (1 - fader.value) * (fader.height - 12)
        width: 30
        height: 12
        radius: 3
        border.color: Theme.buttonEdge
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.knobCap }
            GradientStop { position: 1.0; color: Theme.knobCapShade }
        }
        Rectangle {
            anchors.centerIn: parent
            width: parent.width - 6
            height: 2
            color: Theme.ink
        }
    }

    MouseArea {
        anchors.fill: parent
        onPressed: mouse => fader.setValue(1 - (mouse.y - 6) / (fader.height - 12))
        onPositionChanged: mouse => fader.setValue(1 - (mouse.y - 6) / (fader.height - 12))
        onDoubleClicked: fader.setValue(fader.unity)
        onWheel: wheel => fader.setValue(fader.value + (wheel.angleDelta.y > 0 ? 0.02 : -0.02))
    }
}

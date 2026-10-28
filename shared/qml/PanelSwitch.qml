import QtQuick

// Small vertical toggle with 2 or 3 positions; the top is the highest value.
// Click the upper half to move up, the lower half to move down. Emits
// moved(v) with v normalized (0, 0.5, 1 or 0, 1); the owner writes the model.
Item {
    id: sw

    property string name: ""
    property int positions: 2
    property real value: 0
    property string displayText: ""

    signal moved(real value)

    readonly property int position: Math.round(value * (positions - 1))

    function setPosition(p) {
        p = Math.max(0, Math.min(positions - 1, p))
        if (p !== position)
            moved(p / (positions - 1))
    }

    width: 22
    height: 56
    activeFocusOnTab: true

    Accessible.role: Accessible.Slider
    Accessible.name: name
    Accessible.description: displayText

    Keys.onUpPressed: setPosition(position + 1)
    Keys.onDownPressed: setPosition(position - 1)

    Rectangle { // slot
        anchors.fill: parent
        radius: 3
        color: "#060606"
        border.color: "#4A4B4E"
    }

    Rectangle { // lever
        x: 3
        width: parent.width - 6
        height: (parent.height - 6) / sw.positions
        y: 3 + (sw.positions - 1 - sw.position) * height
        radius: 2
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#F2F2F2" }
            GradientStop { position: 1.0; color: "#A8A9AC" }
        }
        Behavior on y { NumberAnimation { duration: 60 } }
    }

    Rectangle { // focus ring
        visible: sw.activeFocus
        anchors.fill: parent
        anchors.margins: -3
        radius: 4
        color: "transparent"
        border.color: Theme.accentUi
        border.width: 2
    }

    MouseArea {
        anchors.fill: parent
        onClicked: mouse => {
            sw.forceActiveFocus()
            sw.setPosition(sw.position + (mouse.y < height / 2 ? 1 : -1))
        }
        onWheel: wheel => sw.setPosition(sw.position + (wheel.angleDelta.y > 0 ? 1 : -1))
    }
}

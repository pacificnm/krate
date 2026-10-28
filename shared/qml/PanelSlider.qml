pragma ComponentBehavior: Bound

import QtQuick

// Vertical slider with a 0-10 scale: a dark slot, a cap with a white line.
// Drag (Shift = fine), wheel steps, double-click resets, Up/Down keys.
// Like Knob, it never assigns its own value: it emits moved(v) and the owner
// writes the model, so a binding such as `value: Kalliope.value(key)` survives.
Item {
    id: slider

    property string name: ""
    property real value: 0.5
    property real defaultValue: 0.5
    property string displayText: ""  // shown in a bubble while dragging
    property bool showScale: true    // tick marks on the left
    property color capColor: "#D9DADC"
    property color lineColor: "#F4F4F4"

    signal moved(real value)

    readonly property real capHeight: 18
    readonly property real travel: height - capHeight

    function setValue(v) {
        v = Math.max(0, Math.min(1, v))
        if (v !== value)
            moved(v)
    }

    width: 34
    height: 170
    activeFocusOnTab: true

    Accessible.role: Accessible.Slider
    Accessible.name: name
    Accessible.description: displayText

    Keys.onUpPressed: setValue(value + 0.01)
    Keys.onDownPressed: setValue(value - 0.01)

    Repeater { // scale: 11 ticks, longer at 0, 5 and 10
        model: slider.showScale ? 11 : 0

        Rectangle {
            required property int index
            x: index % 5 === 0 ? -2 : 1
            y: slider.capHeight / 2 + slider.travel * (1 - index / 10) - height / 2
            width: index % 5 === 0 ? 8 : 5
            height: 1
            color: "#9A9CA0"
        }
    }

    Rectangle { // slot
        anchors.horizontalCenter: parent.horizontalCenter
        y: slider.capHeight / 2 - 2
        width: 6
        height: slider.travel + 4
        radius: 3
        color: "#050505"
        border.color: "#3C3D40"
    }

    Rectangle { // cap
        id: cap
        anchors.horizontalCenter: parent.horizontalCenter
        y: slider.travel * (1 - slider.value)
        width: slider.width - 8
        height: slider.capHeight
        radius: 2
        border.color: "#111"
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.lighter(slider.capColor, 1.1) }
            GradientStop { position: 0.5; color: slider.capColor }
            GradientStop { position: 1.0; color: Qt.darker(slider.capColor, 1.5) }
        }

        Rectangle {
            anchors.centerIn: parent
            width: parent.width - 6
            height: 2
            color: slider.lineColor
        }
        Rectangle {
            anchors.centerIn: parent
            width: parent.width - 6
            height: 1
            color: "#111"
            y: parent.height / 2 + 1
        }
    }

    Rectangle { // focus ring
        visible: slider.activeFocus
        anchors.fill: parent
        anchors.margins: -3
        radius: 3
        color: "transparent"
        border.color: Theme.accentUi
        border.width: 2
    }

    Rectangle { // value bubble
        visible: sliderMouse.pressed && slider.displayText !== ""
        anchors.horizontalCenter: parent.horizontalCenter
        y: cap.y - height - 6
        z: 10
        width: bubbleText.implicitWidth + 16
        height: bubbleText.implicitHeight + 8
        radius: 4
        color: Theme.band

        Text {
            id: bubbleText
            anchors.centerIn: parent
            text: slider.displayText
            color: Theme.inkInverse
            font.family: Theme.labelFont
            font.pixelSize: 14
            font.bold: true
        }
    }

    MouseArea {
        id: sliderMouse
        property real startY: 0
        property real startValue: 0

        anchors.fill: parent
        cursorShape: Qt.SizeVerCursor
        onPressed: mouse => {
            slider.forceActiveFocus()
            // A press away from the cap jumps there; on the cap it just grabs.
            const capTop = slider.travel * (1 - slider.value)
            let start = slider.value
            if (mouse.y < capTop || mouse.y > capTop + slider.capHeight) {
                start = Math.max(0, Math.min(1, 1 - (mouse.y - slider.capHeight / 2) / slider.travel))
                slider.setValue(start)
            }
            startY = mouse.y
            startValue = start
        }
        onPositionChanged: mouse => {
            const scale = (mouse.modifiers & Qt.ShiftModifier) ? 0.1 : 1.0
            slider.setValue(startValue + (startY - mouse.y) / slider.travel * scale)
        }
        onDoubleClicked: slider.setValue(slider.defaultValue)
        onWheel: wheel => slider.setValue(slider.value + (wheel.angleDelta.y > 0 ? 0.02 : -0.02))
    }
}

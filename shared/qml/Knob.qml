pragma ComponentBehavior: Bound

import QtQuick

// Rotary control: dark ribbed skirt, silver cap, black pointer.
// Only the rib and pointer layers rotate, so lighting stays fixed and nothing
// is re-tessellated while turning. Vertical drag: dragRange px = full travel,
// Shift = fine, wheel = small step, double-click = default.
//
// The knob never assigns its own value: it emits moved(v) and the owner writes
// the model, so a binding like `value: Synth.cutoff` survives. A knob with no
// model can use `onMoved: v => value = v`.
Item {
    id: knob

    property string name: ""
    property real value: 0.5
    property real defaultValue: 0.5
    property int detents: 0             // > 1: snaps to this many positions
    property real minAngle: -135
    property real maxAngle: 135
    property real dragRange: 200
    property string displayText: ""     // shown in a bubble while dragging

    // Style: Korrode's silver cap and black pointer by default; instruments may
    // restyle (Kessel: black cap, orange pointer).
    property color capColor: Theme.knobCap
    property color capShadeColor: Theme.knobCapShade
    property color capBorderColor: "#6A6C70"
    property color pointerColor: Theme.ink

    signal moved(real value)

    readonly property real angle: minAngle + (maxAngle - minAngle) * value

    function setValue(v) {
        v = Math.max(0, Math.min(1, v))
        if (detents > 1)
            v = Math.round(v * (detents - 1)) / (detents - 1)
        if (v !== value)
            moved(v)
    }

    function step(direction) {
        setValue(value + direction * (detents > 1 ? 1 / (detents - 1) : 0.01))
    }

    width: 70
    height: 70
    activeFocusOnTab: true

    Accessible.role: Accessible.Dial
    Accessible.name: name

    Keys.onUpPressed: step(1)
    Keys.onDownPressed: step(-1)

    Rectangle { // drop shadow
        x: 2
        y: 4
        width: knob.width
        height: knob.height
        radius: width / 2
        color: "#45000000"
    }

    Rectangle { // skirt
        anchors.fill: parent
        radius: width / 2
        border.color: "#0C0C0D"
        border.width: 1
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.knobSkirt }
            GradientStop { position: 1.0; color: Theme.knobSkirtShade }
        }
    }

    Item { // ribs
        anchors.fill: parent
        rotation: knob.angle

        Repeater {
            model: 32

            Rectangle {
                id: rib
                required property int index
                x: knob.width / 2 - 1
                y: 2
                width: 2
                height: knob.width * 0.11
                color: Theme.knobRib
                transform: Rotation {
                    origin.x: 1
                    origin.y: knob.height / 2 - 2
                    angle: rib.index * 360 / 32
                }
            }
        }
    }

    Rectangle { // cap
        anchors.centerIn: parent
        width: knob.width * 0.7
        height: width
        radius: width / 2
        border.color: knob.capBorderColor
        border.width: 1
        gradient: Gradient {
            GradientStop { position: 0.0; color: knob.capColor }
            GradientStop { position: 1.0; color: knob.capShadeColor }
        }
    }

    Item { // pointer
        anchors.fill: parent
        rotation: knob.angle

        Rectangle {
            width: Math.max(3, knob.width * 0.05)
            height: knob.height * 0.3
            x: (knob.width - width) / 2
            y: knob.height * 0.15 + 3
            radius: width / 2
            color: knob.pointerColor
        }
    }

    Rectangle { // focus ring
        visible: knob.activeFocus
        anchors.fill: parent
        anchors.margins: -4
        radius: width / 2
        color: "transparent"
        border.color: Theme.accentUi
        border.width: 2
    }

    Rectangle { // value bubble
        visible: knobMouse.pressed && knob.displayText !== ""
        anchors.horizontalCenter: parent.horizontalCenter
        y: -height - 6
        z: 10
        width: bubbleText.implicitWidth + 16
        height: bubbleText.implicitHeight + 8
        radius: 4
        color: Theme.band

        Text {
            id: bubbleText
            anchors.centerIn: parent
            text: knob.displayText
            color: Theme.inkInverse
            font.family: Theme.labelFont
            font.pixelSize: 14
            font.bold: true
        }
    }

    MouseArea {
        id: knobMouse
        property real startY: 0
        property real startValue: 0

        anchors.fill: parent
        cursorShape: Qt.SizeVerCursor
        onPressed: mouse => {
            knob.forceActiveFocus()
            startY = mouse.y
            startValue = knob.value
        }
        onPositionChanged: mouse => {
            const scale = (mouse.modifiers & Qt.ShiftModifier) ? 0.1 : 1.0
            knob.setValue(startValue + (startY - mouse.y) / knob.dragRange * scale)
        }
        onDoubleClicked: knob.setValue(knob.defaultValue)
        onWheel: wheel => knob.step(wheel.angleDelta.y > 0 ? 1 : -1)
    }
}

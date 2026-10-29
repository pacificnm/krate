import QtQuick
import QtQuick.Shapes

// A knob with an LED ring: a bright arc from the start to the value, and an
// optional second, dimmer arc for a morph range (from the value to
// morphValue; -1 = none). Wraps Knob, so dragging, the wheel, double-click
// reset, keys and accessibility all behave the same. Like Knob it never
// assigns its own value: it emits moved(v).
Item {
    id: ring

    property string name: ""
    property real value: 0.5
    property real defaultValue: 0.5
    property int detents: 0
    property string displayText: ""
    property real morphValue: -1
    property bool bipolar: false          // the arc grows from the top (centre) instead of the start
    property color ringColor: "#FF4A3D"
    property color morphColor: "#4FA3FF"
    property color trackColor: "#2E2F32"
    property color capColor: "#2C2D30"
    property color pointerColor: "#F0F0F0"

    signal moved(real value)

    readonly property real knobSize: Math.min(width, height) - 12
    readonly property real radius: Math.min(width, height) / 2 - 3
    readonly property real startAngle: 135          // Shape angles: 0 = 3 o'clock, clockwise
    readonly property real sweep: 270

    function arcFrom(v) { return startAngle + sweep * v }

    width: 48
    height: 48

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath { // track
            strokeColor: ring.trackColor
            strokeWidth: 3
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            PathAngleArc {
                centerX: ring.width / 2
                centerY: ring.height / 2
                radiusX: ring.radius
                radiusY: ring.radius
                startAngle: ring.startAngle
                sweepAngle: ring.sweep
            }
        }
        ShapePath { // morph range
            strokeColor: ring.morphValue >= 0 ? ring.morphColor : "transparent"
            strokeWidth: 3
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            PathAngleArc {
                centerX: ring.width / 2
                centerY: ring.height / 2
                radiusX: ring.radius
                radiusY: ring.radius
                startAngle: ring.arcFrom(Math.min(ring.value, Math.max(ring.morphValue, 0)))
                sweepAngle: ring.sweep * Math.abs(Math.max(ring.morphValue, 0) - ring.value)
            }
        }
        ShapePath { // value
            strokeColor: ring.ringColor
            strokeWidth: 3
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            PathAngleArc {
                centerX: ring.width / 2
                centerY: ring.height / 2
                radiusX: ring.radius
                radiusY: ring.radius
                startAngle: ring.bipolar ? ring.arcFrom(Math.min(ring.value, 0.5)) : ring.startAngle
                sweepAngle: ring.bipolar ? ring.sweep * Math.abs(ring.value - 0.5) : ring.sweep * ring.value
            }
        }
    }

    Knob {
        id: knob
        anchors.centerIn: parent
        width: ring.knobSize
        height: ring.knobSize
        name: ring.name
        value: ring.value
        defaultValue: ring.defaultValue
        detents: ring.detents
        displayText: ring.displayText
        capColor: ring.capColor
        capShadeColor: Qt.darker(ring.capColor, 2.2)
        capBorderColor: "#060607"
        pointerColor: ring.pointerColor
        Accessible.description: ring.displayText
        onMoved: v => ring.moved(v)
    }
}

pragma ComponentBehavior: Bound

import QtQuick

// A pedal's case (the board's pedals and the picker's): its colour, lighter at
// the top, and the pattern on it ("dots:#RRGGBB" polka dots,
// "pyramid:#RRGGBB" a stepped pyramid, "paisley:#RRGGBB" 60s paisleys), under
// whatever is put on it. Lettering that reads on it: dark on light cases,
// cream on dark ones.
Rectangle {
    id: pedalCase
    property color colour: "#444"
    property string pattern: ""
    // By perceived brightness: a saturated yellow is light, though its HSL lightness isn't.
    readonly property bool lightCase: 0.299 * colour.r + 0.587 * colour.g + 0.114 * colour.b > 0.6
    readonly property color lettering: lightCase ? "#1A1A1A" : "#F4F1E6"
    readonly property bool patterned: pattern !== ""
    // A pattern busy enough that the knobs want a dark panel behind them.
    readonly property bool knobPlate: pattern.startsWith("dots:") || pattern.startsWith("paisley:")

    radius: 10
    border.color: Qt.darker(colour, 1.8)
    border.width: 2
    gradient: Gradient {
        GradientStop { position: 0.0; color: Qt.lighter(pedalCase.colour, 1.15) }
        GradientStop { position: 1.0; color: Qt.darker(pedalCase.colour, 1.25) }
    }

    // A pattern on the case ("dots:#RRGGBB": polka dots), under everything else.
    Item {
        id: dots
        readonly property string pattern: pedalCase.pattern
        readonly property real spacing: 22
        visible: pattern.startsWith("dots:")
        anchors.fill: parent
        anchors.margins: 3
        clip: true
        Repeater {
            model: dots.visible ? Math.ceil(dots.height / dots.spacing) + 1 : 0
            Repeater {
                id: dotRow
                required property int index
                model: Math.ceil(dots.width / dots.spacing) + 1
                Rectangle {
                    required property int index
                    x: index * dots.spacing + (dotRow.index % 2 ? dots.spacing / 2 : 0) - 6
                    y: dotRow.index * dots.spacing - 4
                    width: 11
                    height: 11
                    radius: 5.5
                    color: dots.pattern.slice(5)
                }
            }
        }
    }

    // "pyramid:#RRGGBB": a stepped pyramid in that colour, under the controls.
    Canvas {
        id: pyramid
        readonly property string pattern: pedalCase.pattern
        visible: pattern.startsWith("pyramid:")
        anchors.fill: parent
        onPatternChanged: requestPaint()
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            if (!visible)
                return
            const colour = pattern.slice(8)
            const apexY = 84, baseY = height - 14
            const half = Math.min(width * 0.42, (baseY - apexY) * 0.95)
            const cx = width / 2
            // Two faces, the right one in shade.
            ctx.fillStyle = Qt.lighter(colour, 1.25)
            ctx.beginPath()
            ctx.moveTo(cx, apexY)
            ctx.lineTo(cx - half, baseY)
            ctx.lineTo(cx + half * 0.15, baseY)
            ctx.closePath()
            ctx.fill()
            ctx.fillStyle = colour
            ctx.beginPath()
            ctx.moveTo(cx, apexY)
            ctx.lineTo(cx + half * 0.15, baseY)
            ctx.lineTo(cx + half, baseY)
            ctx.closePath()
            ctx.fill()
            // Its courses of stone.
            ctx.strokeStyle = Qt.darker(colour, 1.5)
            ctx.lineWidth = 1
            const courses = 9
            for (let i = 1; i < courses; ++i) {
                const y = apexY + (baseY - apexY) * i / courses
                const w = half * (y - apexY) / (baseY - apexY)
                ctx.beginPath()
                ctx.moveTo(cx - w, y)
                ctx.lineTo(cx + w, y)
                ctx.stroke()
            }
        }
    }

    // "paisley:#RRGGBB": 60s paisleys in that colour, scattered over the case.
    Canvas {
        id: paisley
        readonly property string pattern: pedalCase.pattern
        visible: pattern.startsWith("paisley:")
        anchors.fill: parent
        anchors.margins: 3
        onPatternChanged: requestPaint()
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        // One paisley: a teardrop with a curled tail, an inner outline and a dot.
        function drawOne(ctx, x, y, size, angle, colour) {
            ctx.save()
            ctx.translate(x, y)
            ctx.rotate(angle)
            ctx.scale(size, size)
            const shape = scale => {
                ctx.beginPath()
                ctx.moveTo(0.35 * scale, -1.15 * scale)
                ctx.bezierCurveTo(1.1 * scale, -0.6 * scale, 1.0 * scale, 0.95 * scale, 0.0, 1.0 * scale)
                ctx.bezierCurveTo(-0.95 * scale, 1.0 * scale, -0.9 * scale, -0.2 * scale, -0.15 * scale, -0.45 * scale)
                ctx.bezierCurveTo(0.25 * scale, -0.6 * scale, 0.15 * scale, -0.95 * scale, 0.35 * scale, -1.15 * scale)
                ctx.closePath()
            }
            ctx.lineWidth = 0.12
            ctx.strokeStyle = colour
            shape(1.0)
            ctx.stroke()
            ctx.lineWidth = 0.08
            shape(0.6)
            ctx.stroke()
            ctx.fillStyle = colour
            ctx.beginPath()
            ctx.arc(0.0, 0.35, 0.16, 0, 2 * Math.PI)
            ctx.fill()
            ctx.restore()
        }
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            if (!visible)
                return
            const colour = pattern.slice(8)
            // A fixed scatter, so the case looks the same every time.
            let seed = 7
            const random = () => { seed = (seed * 16807) % 2147483647; return seed / 2147483647 }
            const step = 46
            for (let row = 0; row * step < height + step; ++row)
                for (let col = 0; col * step < width + step; ++col) {
                    const x = col * step + (row % 2 ? step / 2 : 0) + (random() - 0.5) * 12
                    const y = row * step + (random() - 0.5) * 12
                    drawOne(ctx, x, y, 13 + random() * 6, random() * 2 * Math.PI, colour)
                }
        }
    }
}

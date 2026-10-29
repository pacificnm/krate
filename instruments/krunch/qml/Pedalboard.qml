pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Krate

// The pedalboard (docs/PEDALS.md), left to right: FRONT pedals play in front
// of the amp, LOOP pedals in its effects loop (when the channel's LOOP is
// on), each group in the board's order. Drag a pedal by its body to move it;
// FRONT / LOOP places it; the footswitch turns it on and off (learnable:
// "pedal1".."pedal8" by position); × takes it off the board; + adds one.
Rectangle {
    id: board

    readonly property real slot: 164 // a standard pedal and the gap after it
    readonly property var layout: Pedals.layoutRevision >= 0 ? Pedals.pedals() : []
    // Each pedal's left edge, and the end of the last one (cases are 1 or more slots wide).
    readonly property var lefts: {
        const xs = []
        let x = 0
        for (const p of layout) {
            xs.push(x)
            x += p.width * slot
        }
        xs.push(x)
        return xs
    }
    // Where a pedal dragged to centre `c` would go.
    function placeFor(c) {
        for (let i = 0; i < layout.length; ++i)
            if (c < (lefts[i] + lefts[i + 1]) / 2)
                return i
        return layout.length - 1
    }
    readonly property color cream: "#E9DFC4"

    radius: 8
    color: "#1B1A19"
    border.color: "#3A3B3E"

    // The board's carpet.
    Rectangle {
        anchors.fill: parent
        anchors.margins: 6
        radius: 6
        color: "#24221F"
    }

    Text {
        x: 16
        y: 8
        readonly property var counts: {
            let front = 0, loop = 0
            for (let i = 0; Pedals.revision >= 0 && i < Pedals.count; ++i)
                Pedals.inLoop(i) ? ++loop : ++front
            return [front, loop]
        }
        text: "PEDALS  ·  guitar → " + counts[0] + " in front → preamp → " + counts[1] + " in the loop → power amp"
        color: "#8F897A"
        font.family: Theme.labelFont
        font.pixelSize: 12
        font.bold: true
    }

    Flickable { // scrolls sideways once the board is fuller than it is wide
        id: lane
        x: 16
        y: 30
        width: parent.width - 32
        height: parent.height - 40
        contentWidth: board.lefts[board.lefts.length - 1] + 120
        contentHeight: height
        flickableDirection: Flickable.HorizontalFlick
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        interactive: contentWidth > width
        ScrollBar.horizontal: ScrollBar { policy: lane.interactive ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff }

        Repeater {
            model: Pedals.layoutRevision >= 0 ? Pedals.count : 0

            Rectangle {
                id: pedal
                required property int index
                readonly property var info: board.layout[index] ?? ({})
                readonly property bool on: Pedals.revision >= 0 && Pedals.isOn(index)
                readonly property bool loop: Pedals.revision >= 0 && Pedals.inLoop(index)
                // Lettering that reads on its case: dark on light cases, cream on dark ones.
                // (by perceived brightness: a saturated yellow is light, though its HSL lightness isn't).
                readonly property color caseColour: info.colour ?? "#444"
                readonly property bool lightCase: 0.299 * caseColour.r + 0.587 * caseColour.g + 0.114 * caseColour.b > 0.6
                readonly property color lettering: lightCase ? "#1A1A1A" : "#F4F1E6"

                Binding on x { // where its place is, except while it's being dragged
                    when: !bodyDrag.drag.active
                    value: board.lefts[pedal.index] ?? 0
                    restoreMode: Binding.RestoreNone
                }
                width: (info.width ?? 1) * board.slot - 12
                height: lane.height
                z: bodyDrag.drag.active ? 10 : 0
                radius: 10
                border.color: Qt.darker(info.colour ?? "#444", 1.8)
                border.width: 2
                gradient: Gradient {
                    GradientStop { position: 0.0; color: Qt.lighter(pedal.info.colour ?? "#444", 1.15) }
                    GradientStop { position: 1.0; color: Qt.darker(pedal.info.colour ?? "#444", 1.25) }
                }
                Accessible.role: Accessible.Grouping
                Accessible.name: (info.name ?? "") + ", pedal " + (index + 1)

                // A pattern on the case ("dots:#RRGGBB": polka dots), under everything else.
                Item {
                    id: dots
                    readonly property string pattern: pedal.info.pattern ?? ""
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
                    readonly property string pattern: pedal.info.pattern ?? ""
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
                    readonly property string pattern: pedal.info.pattern ?? ""
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

                Rectangle { // on a patterned case, a panel behind the knobs and their labels
                    visible: (dots.visible || paisley.visible) && knobRow.visible
                    x: 5
                    y: 14
                    width: parent.width - 10
                    height: knobRow.size + 22
                    radius: 6
                    color: "#101010"
                }

                // Drag by the body to move it along the board.
                MouseArea {
                    id: bodyDrag
                    anchors.fill: parent
                    preventStealing: true // a pedal being dragged isn't the board scrolling
                    drag.target: pedal
                    drag.axis: Drag.XAxis
                    drag.minimumX: 0
                    drag.maximumX: board.lefts[board.lefts.length - 1] - pedal.width
                    cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                    onReleased: {
                        const to = board.placeFor(pedal.x + pedal.width / 2)
                        if (to !== pedal.index)
                            Pedals.move(pedal.index, to)
                        else
                            pedal.x = board.lefts[pedal.index]
                    }
                }

                // Its knobs.
                Row {
                    id: knobRow
                    visible: !(pedal.info.sliders ?? false)
                    readonly property real size: Math.min(44, (pedal.width - 12) / Math.max(1, (pedal.info.knobs ?? []).length) - 2)
                    x: (parent.width - width) / 2
                    y: 20
                    spacing: 2
                    Repeater {
                        model: knobRow.visible ? (pedal.info.knobs ?? []) : []
                        Column {
                            id: knobColumn
                            required property var modelData
                            required property int index
                            spacing: 1
                            Knob {
                                width: knobRow.size
                                height: knobRow.size
                                name: (pedal.info.name ?? "") + " " + knobColumn.modelData.label
                                value: Pedals.revision >= 0 ? Pedals.knob(pedal.index, knobColumn.index) : 0
                                defaultValue: knobColumn.modelData.defaultValue
                                detents: knobColumn.modelData.detents ?? 0
                                displayText: Pedals.knobText(pedal.index, knobColumn.index, value)
                                capColor: "#1E1E1E"
                                capShadeColor: "#000"
                                pointerColor: "#F2F2F2"
                                onMoved: v => Pedals.setKnob(pedal.index, knobColumn.index, v)
                            }
                            Text {
                                width: knobRow.size + 2
                                horizontalAlignment: Text.AlignHCenter
                                text: knobColumn.modelData.label
                                color: pedal.lettering
                                font.family: Theme.labelFont
                                font.pixelSize: 9
                                font.bold: true
                                fontSizeMode: Text.HorizontalFit // long names shrink rather than run together
                                minimumPixelSize: 6
                            }
                        }
                    }
                }

                // A graphic EQ's sliders, the frequency under each.
                Row {
                    id: sliderRow
                    visible: pedal.info.sliders ?? false
                    readonly property int n: (pedal.info.knobs ?? []).length
                    readonly property real each: (pedal.width - 16) / Math.max(1, n)
                    x: 8
                    y: 12
                    Repeater {
                        model: sliderRow.visible ? (pedal.info.knobs ?? []) : []
                        Column {
                            id: sliderColumn
                            required property var modelData
                            required property int index
                            width: sliderRow.each
                            spacing: 2
                            PanelSlider {
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: Math.min(26, sliderRow.each - 4)
                                height: 84
                                showScale: false
                                capColor: sliderColumn.index === sliderRow.n - 1 ? "#E0781E" : "#D9DADC"
                                name: (pedal.info.name ?? "") + " " + sliderColumn.modelData.label
                                value: Pedals.revision >= 0 ? Pedals.knob(pedal.index, sliderColumn.index) : 0.5
                                defaultValue: sliderColumn.modelData.defaultValue
                                displayText: Pedals.knobText(pedal.index, sliderColumn.index, value)
                                onMoved: v => Pedals.setKnob(pedal.index, sliderColumn.index, v)
                            }
                            Text {
                                width: parent.width
                                horizontalAlignment: Text.AlignHCenter
                                text: sliderColumn.modelData.label
                                color: pedal.lettering
                                font.family: Theme.labelFont
                                font.pixelSize: 9
                                font.bold: true
                                fontSizeMode: Text.HorizontalFit
                                minimumPixelSize: 6
                            }
                        }
                    }
                }

                Led {
                    x: (pedal.info.sliders ?? false) ? 12 : (parent.width - width) / 2
                    y: (pedal.info.sliders ?? false) ? 116 : 88
                    width: 10
                    height: 10
                    on: pedal.on
                }

                Rectangle { // on a patterned case, a plate behind the name so it reads
                    visible: dots.visible || pyramid.visible || paisley.visible
                    x: 14
                    y: (pedal.info.sliders ?? false) ? 106 : 98
                    width: parent.width - 28
                    height: 30
                    radius: 4
                    color: "#101010"
                    border.color: "#60FFFFFF"
                }
                Text { // the name, on a plate
                    x: 8
                    y: (pedal.info.sliders ?? false) ? 110 : 102
                    width: parent.width - 16
                    horizontalAlignment: Text.AlignHCenter
                    text: pedal.info.name ?? ""
                    color: dots.visible || pyramid.visible || paisley.visible ? "#F4F1E6" : pedal.lettering // on its plate, or on the case
                    style: pedal.lightCase ? Text.Normal : Text.Raised
                    styleColor: "#80000000"
                    font.family: Theme.labelFont
                    font.pixelSize: 19
                    font.bold: true
                    font.italic: true
                    wrapMode: Text.WordWrap
                }

                // In front of the amp, or in its loop.
                Rectangle {
                    id: placement
                    x: (parent.width - width) / 2
                    y: 136
                    width: 96
                    height: 20
                    radius: 4
                    color: "#30000000"
                    Row {
                        anchors.fill: parent
                        Repeater {
                            model: ["FRONT", "LOOP"]
                            Rectangle {
                                id: placeChoice
                                required property string modelData
                                required property int index
                                readonly property bool chosen: (index === 1) === pedal.loop
                                width: placement.width / 2
                                height: placement.height
                                radius: 4
                                color: chosen ? (pedal.lightCase ? "#1A1A1A" : "#F4F1E6") : "transparent"
                                Text {
                                    anchors.centerIn: parent
                                    text: placeChoice.modelData
                                    color: placeChoice.chosen === pedal.lightCase ? "#F4F1E6" : "#101010"
                                    font.family: Theme.labelFont
                                    font.pixelSize: 10
                                    font.bold: true
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: Pedals.setInLoop(pedal.index, placeChoice.index === 1)
                                }
                                Accessible.role: Accessible.RadioButton
                                Accessible.name: (pedal.info.name ?? "") + (index === 1 ? " in the effects loop" : " in front of the amp")
                                Accessible.checked: chosen
                            }
                        }
                    }
                }

                // The footswitch.
                Rectangle {
                    id: footswitch
                    x: (parent.width - width) / 2
                    y: parent.height - height - 26
                    width: 48
                    height: 48
                    radius: width / 2
                    border.color: "#555"
                    border.width: 2
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: footswitchArea.pressed ? "#9A9A9A" : "#E4E4E4" }
                        GradientStop { position: 1.0; color: "#7A7A7A" }
                    }
                    Accessible.role: Accessible.Button
                    Accessible.name: "Footswitch: " + (pedal.info.name ?? "") + (pedal.on ? ", on" : ", off")
                    MouseArea {
                        id: footswitchArea
                        anchors.fill: parent
                        onClicked: Pedals.toggle(pedal.index)
                    }
                    LearnOverlay {
                        anchors.fill: parent
                        paramKey: "pedal" + (pedal.index + 1)
                    }
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: parent.height - 22
                    text: (pedal.index + 1).toString()
                    color: pedal.lettering
                    font.family: Theme.labelFont
                    font.pixelSize: 11
                    font.bold: true
                }

                // Off the board.
                Text {
                    x: parent.width - 18
                    y: 0
                    text: "×"
                    color: removeArea.containsMouse ? (pedal.lightCase ? "#000000" : "#FFFFFF") : pedal.lettering
                    opacity: removeArea.containsMouse ? 1.0 : 0.65
                    font.family: Theme.labelFont
                    font.pixelSize: 18
                    font.bold: true
                    Accessible.role: Accessible.Button
                    Accessible.name: "Remove " + (pedal.info.name ?? "")
                    MouseArea {
                        id: removeArea
                        anchors.fill: parent
                        anchors.margins: -6
                        hoverEnabled: true
                        onClicked: Pedals.remove(pedal.index)
                    }
                }
            }
        }

        // + : another pedal at the end of the board.
        Rectangle {
            visible: Pedals.count < Pedals.maxPedals
            x: board.lefts[board.lefts.length - 1]
            width: 110
            height: lane.height
            radius: 10
            color: addArea.containsMouse ? "#2E2C29" : "transparent"
            border.color: "#5A5650"
            border.width: 2

            Column {
                anchors.centerIn: parent
                spacing: 6
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "+"
                    color: board.cream
                    font.pixelSize: 40
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "ADD PEDAL"
                    color: board.cream
                    font.family: Theme.labelFont
                    font.pixelSize: 12
                    font.bold: true
                }
            }
            MouseArea {
                id: addArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: addMenu.popup()
            }
            Accessible.role: Accessible.Button
            Accessible.name: "Add a pedal"

            Menu {
                id: addMenu
                Repeater {
                    model: Pedals.types
                    MenuItem {
                        required property var modelData
                        text: modelData.name
                        onTriggered: Pedals.add(modelData.type)
                    }
                }
            }
        }

        Text {
            visible: Pedals.count === 0
            x: 130
            anchors.verticalCenter: parent.verticalCenter
            text: "Add a pedal with +. Drag pedals to reorder them; FRONT or LOOP puts one in front of the amp or in its effects loop."
            color: "#8F897A"
            font.family: Theme.labelFont
            font.pixelSize: 14
        }
    }
}

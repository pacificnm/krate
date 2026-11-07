pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Krate

// The Parametric EQ pedal's editor (docs/PEDALS.md#parametric-eq): its curve,
// 20 Hz .. 20 kHz across and -18 .. +18 dB up, with a point per band: drag a
// point to move its frequency and gain, the wheel over it for its Q. Under
// the curve, each band's TYPE, FREQ, GAIN and Q, and the LEVEL.
Popup {
    id: editor
    required property Pedals pedals
    property int position: -1  // the pedal's place on the board
    property int selected: -1  // the band being dragged or last touched

    readonly property int bands: 6
    readonly property int levelKnob: bands * 4
    readonly property var colours: ["#E2412E", "#F08A2B", "#F2C230", "#5FD35A", "#5FB0E8", "#B07FE0"]

    // The knobs' defaults (a double-click puts one back), from the pedal's description.
    readonly property var defaults: {
        const layout = editor.pedals.layoutRevision >= 0 && position >= 0 ? editor.pedals.pedals() : []
        return position < layout.length ? layout[position].knobs.map(k => k.defaultValue) : []
    }
    function knobIndex(band, part) { return band * 4 + part } // part: 0 type, 1 freq, 2 gain, 3 q
    function value(index) { return editor.pedals.revision >= 0 && position >= 0 ? editor.pedals.knob(position, index) : 0 }
    function set(index, v) { editor.pedals.setKnob(position, index, Math.max(0, Math.min(1, v))) }
    function text(index) { return position >= 0 ? editor.pedals.knobText(position, index, value(index)) : "" }
    function typeOf(band) { return Math.round(value(knobIndex(band, 0)) * 5) } // 0 off, 1 peak, 2 low shelf, 3 high shelf, 4 low cut, 5 high cut
    function hasGain(band) { const t = typeOf(band); return t >= 1 && t <= 3 }

    parent: Overlay.overlay
    x: Math.round((parent.width - width) / 2)
    y: Math.round((parent.height - height) / 2)
    width: Math.min(940, parent.width - 40)
    height: Math.min(640, parent.height - 40)
    modal: true
    padding: 16
    background: Rectangle {
        radius: 8
        color: "#1C1D20"
        border.color: "#4A4C50"
        border.width: 2
    }

    Column {
        anchors.fill: parent
        spacing: 12

        Row {
            width: parent.width
            Text {
                width: parent.width - closeButton.width
                anchors.verticalCenter: parent.verticalCenter
                text: "PARAMETRIC EQ  ·  pedal " + (editor.position + 1)
                color: "#E9DFC4"
                font.family: Theme.labelFont
                font.pixelSize: 16
                font.bold: true
            }
            PanelButton {
                id: closeButton
                width: 80
                height: 30
                dark: true
                faceText: "CLOSE"
                text: "Close the editor"
                onClicked: editor.close()
            }
        }

        // The curve, and the bands' points.
        Item {
            id: graph
            width: parent.width
            height: 280
            readonly property var curve: editor.visible && editor.pedals.revision >= 0 && editor.position >= 0
                                         ? editor.pedals.response(editor.position, 400) : []
            function xOfPosition(p) { return p * width }
            function yOfDb(db) { return height / 2 - Math.max(-18, Math.min(18, db)) * (height / 2 - 6) / 18 }
            function dbOfY(y) { return (height / 2 - y) * 18 / (height / 2 - 6) }
            function nearest(mx, my) {
                let best = -1, dist = 18
                for (let b = 0; b < editor.bands; ++b) {
                    if (editor.typeOf(b) === 0)
                        continue
                    const px = xOfPosition(editor.value(editor.knobIndex(b, 1)))
                    const py = editor.hasGain(b) ? yOfDb((editor.value(editor.knobIndex(b, 2)) - 0.5) * 36) : height / 2
                    const d = Math.hypot(px - mx, py - my)
                    if (d < dist) {
                        dist = d
                        best = b
                    }
                }
                return best
            }

            Canvas {
                id: canvas
                anchors.fill: parent
                readonly property var curve: graph.curve
                readonly property int selected: editor.selected
                onCurveChanged: requestPaint()
                onSelectedChanged: requestPaint()
                onPaint: {
                    const ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    ctx.fillStyle = "#0E1418"
                    ctx.fillRect(0, 0, width, height)
                    ctx.font = "11px sans-serif"
                    // Frequencies across (log), dB up.
                    ctx.strokeStyle = "#22303A"
                    ctx.fillStyle = "#6E7F8A"
                    for (const hz of [50, 100, 200, 500, 1000, 2000, 5000, 10000]) {
                        const x = Math.log(hz / 20) / Math.log(1000) * width
                        ctx.beginPath()
                        ctx.moveTo(x, 0)
                        ctx.lineTo(x, height)
                        ctx.stroke()
                        ctx.fillText(hz >= 1000 ? (hz / 1000) + "k" : String(hz), x + 3, height - 4)
                    }
                    for (const db of [-18, -12, -6, 0, 6, 12, 18]) {
                        const y = graph.yOfDb(db)
                        ctx.strokeStyle = db === 0 ? "#3E5260" : "#22303A"
                        ctx.beginPath()
                        ctx.moveTo(0, y)
                        ctx.lineTo(width, y)
                        ctx.stroke()
                        ctx.fillText((db > 0 ? "+" : "") + db + " dB", 4, db === 18 ? y + 12 : y - 3)
                    }
                    if (curve.length > 1) {
                        ctx.strokeStyle = "#5FD3C8"
                        ctx.lineWidth = 2.5
                        ctx.beginPath()
                        for (let i = 0; i < curve.length; ++i) {
                            const x = i * width / (curve.length - 1)
                            const y = graph.yOfDb(curve[i])
                            i === 0 ? ctx.moveTo(x, y) : ctx.lineTo(x, y)
                        }
                        ctx.stroke()
                        ctx.lineWidth = 1
                    }
                    for (let b = 0; b < editor.bands; ++b) {
                        if (editor.typeOf(b) === 0)
                            continue
                        const x = graph.xOfPosition(editor.value(editor.knobIndex(b, 1)))
                        const y = editor.hasGain(b) ? graph.yOfDb((editor.value(editor.knobIndex(b, 2)) - 0.5) * 36) : height / 2
                        ctx.fillStyle = editor.colours[b]
                        ctx.beginPath()
                        ctx.arc(x, y, b === selected ? 10 : 8, 0, 2 * Math.PI)
                        ctx.fill()
                        ctx.fillStyle = "#101010"
                        ctx.fillText(String(b + 1), x - 3, y + 4)
                    }
                }
            }
            MouseArea {
                anchors.fill: parent
                property int dragging: -1
                onPressed: mouse => {
                    dragging = graph.nearest(mouse.x, mouse.y)
                    if (dragging >= 0)
                        editor.selected = dragging
                }
                onPositionChanged: mouse => {
                    if (dragging < 0)
                        return
                    editor.set(editor.knobIndex(dragging, 1), mouse.x / graph.width)
                    if (editor.hasGain(dragging))
                        editor.set(editor.knobIndex(dragging, 2), 0.5 + graph.dbOfY(mouse.y) / 36)
                }
                onReleased: dragging = -1
                onWheel: wheel => {
                    const b = graph.nearest(wheel.x, wheel.y) >= 0 ? graph.nearest(wheel.x, wheel.y) : editor.selected
                    if (b < 0)
                        return
                    editor.selected = b
                    editor.set(editor.knobIndex(b, 3), editor.value(editor.knobIndex(b, 3)) + (wheel.angleDelta.y > 0 ? 0.03 : -0.03))
                }
            }
        }

        Text {
            width: parent.width
            text: "Drag a point: frequency and gain. The wheel over it: Q (narrower or wider). Cuts have no gain: their Q is the corner's resonance."
            color: "#8F897A"
            font.family: Theme.labelFont
            font.pixelSize: 11
        }

        // Each band's controls, and the level.
        Row {
            spacing: 6
            Repeater {
                model: editor.bands
                Rectangle {
                    id: bandBox
                    required property int index
                    width: (editor.width - 32 - 96 - 6 * 6) / editor.bands
                    height: 220
                    radius: 6
                    color: editor.selected === index ? "#2A2C30" : "#232427"
                    border.color: editor.colours[index]
                    border.width: editor.selected === index ? 2 : 1
                    opacity: editor.typeOf(index) === 0 ? 0.55 : 1.0
                    MouseArea { anchors.fill: parent; onPressed: mouse => { editor.selected = bandBox.index; mouse.accepted = false } }
                    Column {
                        anchors.fill: parent
                        anchors.margins: 6
                        spacing: 4
                        Text {
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                            text: "BAND " + (bandBox.index + 1)
                            color: editor.colours[bandBox.index]
                            font.family: Theme.labelFont
                            font.pixelSize: 11
                            font.bold: true
                        }
                        Row { // TYPE: off, peak, shelves, cuts
                            width: parent.width
                            PanelButton {
                                width: 22
                                height: 22
                                dark: true
                                faceText: "◂"
                                text: "Band " + (bandBox.index + 1) + ": previous type"
                                onClicked: editor.set(editor.knobIndex(bandBox.index, 0), ((editor.typeOf(bandBox.index) + 5) % 6) / 5)
                            }
                            Text {
                                width: parent.width - 44
                                height: 22
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                text: editor.text(editor.knobIndex(bandBox.index, 0)).toUpperCase()
                                color: "#E9DFC4"
                                font.family: Theme.labelFont
                                font.pixelSize: 10
                                font.bold: true
                                fontSizeMode: Text.HorizontalFit
                                minimumPixelSize: 7
                            }
                            PanelButton {
                                width: 22
                                height: 22
                                dark: true
                                faceText: "▸"
                                text: "Band " + (bandBox.index + 1) + ": next type"
                                onClicked: editor.set(editor.knobIndex(bandBox.index, 0), ((editor.typeOf(bandBox.index) + 1) % 6) / 5)
                            }
                        }
                        Repeater {
                            model: [{ part: 1, label: "FREQ" }, { part: 2, label: "GAIN" }, { part: 3, label: "Q" }]
                            Row {
                                id: knobLine
                                required property var modelData
                                readonly property int knob: editor.knobIndex(bandBox.index, modelData.part)
                                width: parent.width
                                spacing: 4
                                opacity: modelData.part === 2 && !editor.hasGain(bandBox.index) ? 0.35 : 1.0
                                Knob {
                                    width: 40
                                    height: 40
                                    name: "Band " + (bandBox.index + 1) + " " + knobLine.modelData.label
                                    value: editor.value(knobLine.knob)
                                    defaultValue: editor.defaults[knobLine.knob] ?? 0.5
                                    displayText: editor.text(knobLine.knob)
                                    capColor: "#1E1E1E"
                                    capShadeColor: "#000"
                                    pointerColor: "#F2F2F2"
                                    onMoved: v => {
                                        editor.selected = bandBox.index
                                        editor.set(knobLine.knob, v)
                                    }
                                }
                                Column {
                                    anchors.verticalCenter: parent.verticalCenter
                                    Text {
                                        text: knobLine.modelData.label
                                        color: "#8F897A"
                                        font.family: Theme.labelFont
                                        font.pixelSize: 9
                                        font.bold: true
                                    }
                                    Text {
                                        text: editor.text(knobLine.knob)
                                        color: "#E9DFC4"
                                        font.family: Theme.labelFont
                                        font.pixelSize: 10
                                    }
                                }
                            }
                        }
                    }
                }
            }
            Rectangle { // the level
                width: 96
                height: 220
                radius: 6
                color: "#232427"
                border.color: "#4A4C50"
                Column {
                    anchors.centerIn: parent
                    spacing: 6
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "LEVEL"
                        color: "#E9DFC4"
                        font.family: Theme.labelFont
                        font.pixelSize: 11
                        font.bold: true
                    }
                    Knob {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 56
                        height: 56
                        name: "Parametric EQ level"
                        value: editor.value(editor.levelKnob)
                        defaultValue: 0.5
                        displayText: editor.text(editor.levelKnob)
                        capColor: "#1E1E1E"
                        capShadeColor: "#000"
                        pointerColor: "#F2F2F2"
                        onMoved: v => editor.set(editor.levelKnob, v)
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: editor.text(editor.levelKnob)
                        color: "#E9DFC4"
                        font.family: Theme.labelFont
                        font.pixelSize: 10
                    }
                }
            }
        }
    }
}

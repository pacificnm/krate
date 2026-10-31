pragma ComponentBehavior: Bound

import QtQuick
import Krate

// One module on the rack, drawn from its table (Konnekt.moduleInfo): a header
// to drag it by (and ✕ to remove it), its knobs, its input jacks, its output
// jacks with their live traces. Jacks are clicked to wire them (the rack does
// the wiring: jackClicked); jackPoint() says where a jack is, for the cables.
// Right-clicking anything asks for its explanation (explain).
Item {
    id: panel

    required property int slot
    required property string type
    property bool onScreen: true // the rack says; traces and the scope only redraw when seen
    readonly property var info: Konnekt.moduleInfo(type)

    signal jackClicked(int slot, bool output, int jack)
    signal jackScope(int slot, int jack)
    signal explain(string path)
    signal moved()

    readonly property real unit: 64
    readonly property real knobW: 56
    readonly property real knobH: 80
    readonly property real jackW: 56
    readonly property int knobColumns: Math.max(1, Math.floor((width - 8) / knobW))
    readonly property int jackColumns: Math.max(1, Math.floor((width - 8) / jackW))
    readonly property int knobRows: Math.ceil(info.knobs.length / knobColumns)
    readonly property real knobsTop: 32
    readonly property real inputsTop: knobsTop + knobRows * knobH + (info.knobs.length > 0 ? 6 : 0)
    readonly property real inputsHeight: info.inputs.length > 0 ? 18 + Math.ceil(info.inputs.length / jackColumns) * 46 : 0
    readonly property real outputsTop: inputsTop + inputsHeight
    readonly property real outputsHeight: info.outputs.length > 0 ? 18 + Math.ceil(info.outputs.length / jackColumns) * 64 : 0
    readonly property bool isScope: type === "scope"
    readonly property string path: "m/" + slot

    width: info.width * unit - 6
    height: outputsTop + outputsHeight + (isScope ? 150 : 0) + 8

    readonly property var signalColors: ["#E8873A", "#4FA3E8", "#6FCF5A", "#B57BE8"] // audio, pitch, gate, mod

    // Where a jack's centre is, in the panel.
    function jackPoint(output, jack) {
        const top = output ? outputsTop : inputsTop
        const col = jack % jackColumns, row = Math.floor(jack / jackColumns)
        return Qt.point(4 + col * jackW + jackW / 2, top + 18 + row * (output ? 64 : 46) + 11)
    }

    Rectangle {
        anchors.fill: parent
        radius: 6
        color: "#26282C"
        border.color: "#0D0E10"
        border.width: 2
    }

    // Header: name, drag to move, ✕ to remove
    Rectangle {
        width: parent.width
        height: 26
        radius: 6
        color: "#3A3D44"

        Text {
            x: 8
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - 34
            elide: Text.ElideRight
            text: panel.info.name
            color: "#F0EDE6"
            font.family: Theme.labelFont
            font.pixelSize: 12
            font.bold: true
        }
        Text {
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            text: "✕"
            color: removeArea.containsMouse ? "#FF6A55" : "#9A968E"
            font.pixelSize: 13
            MouseArea {
                id: removeArea
                anchors.fill: parent
                anchors.margins: -4
                hoverEnabled: true
                onClicked: Konnekt.removeModule(panel.slot)
                onEntered: Konnekt.help = "Remove " + panel.info.name + " (and its cables)."
            }
        }
        HoverHandler {
            onHoveredChanged: if (hovered) Konnekt.help = panel.info.name + ": " + panel.info.help
                                                          + (panel.info.perVoice ? "  It runs once per voice." : "  It runs once: per-voice cables into it are mixed.")
                                                          + "  (Drag here to move it. Right-click: what it is and why.)"
        }
        TapHandler {
            acceptedButtons: Qt.RightButton
            onTapped: panel.explain(panel.path)
        }
        DragHandler {
            target: panel
            onActiveChanged: if (!active) panel.moved()
        }
    }

    // Knobs
    Grid {
        x: 4
        y: panel.knobsTop
        columns: panel.knobColumns
        Repeater {
            model: panel.info.knobs

            Item {
                id: knobCell
                required property int index
                required property var modelData
                width: panel.knobW
                height: panel.knobH

                readonly property real position: Konnekt.knobRevision >= 0 ? Konnekt.knob(panel.slot, index) : 0
                readonly property int cc: Konnekt.learning ? Konnekt.learnedCc(panel.slot, index) : -1
                readonly property bool toggle: modelData.detents === 2 // two positions: a button

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                    text: knobCell.modelData.name.toUpperCase()
                    color: "#DAD5CB"
                    font.family: Theme.labelFont
                    font.pixelSize: 10
                    font.bold: true
                }
                // A two-position control is a button, anything else a knob: only
                // the one shown is made (a big rack holds hundreds of these).
                Loader {
                    active: knobCell.toggle
                    x: (parent.width - 46) / 2
                    y: 20
                    sourceComponent: Rectangle { // click to switch
                        width: 46
                        height: 28
                        radius: 4
                        color: knobCell.position > 0.5 ? "#F2B632" : "#33363C"
                        border.color: "#08090A"
                        Text {
                            anchors.centerIn: parent
                            text: Konnekt.knobText(panel.slot, knobCell.index, knobCell.position)
                            color: knobCell.position > 0.5 ? "#111" : "#DAD5CB"
                            font.family: Theme.labelFont
                            font.pixelSize: 10
                            font.bold: true
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: Konnekt.setKnob(panel.slot, knobCell.index, knobCell.position > 0.5 ? 0 : 1)
                        }
                    }
                }
                Loader {
                    id: knob
                    active: !knobCell.toggle
                    x: (parent.width - 38) / 2
                    y: 15
                    width: 38
                    height: 38
                    sourceComponent: Knob {
                        width: 38
                        height: 38
                        name: knobCell.modelData.name
                        detents: knobCell.modelData.detents
                        value: knobCell.position
                        defaultValue: Konnekt.knobDefault(panel.slot, knobCell.index)
                        displayText: Konnekt.knobText(panel.slot, knobCell.index, value)
                        onMoved: v => Konnekt.setKnob(panel.slot, knobCell.index, v)
                        capColor: "#33363C"
                        capShadeColor: "#15161A"
                        capBorderColor: "#08090A"
                        pointerColor: "#F2B632"
                    }
                }
                Text {
                    visible: !knobCell.toggle || knobCell.cc >= 0
                    y: 56
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                    text: knobCell.cc >= 0 ? "CC " + knobCell.cc : Konnekt.knobText(panel.slot, knobCell.index, knobCell.position)
                    color: knobCell.cc >= 0 ? "#5FD0FF" : "#9A968E"
                    font.family: Theme.labelFont
                    font.pixelSize: 10
                }
                HoverHandler {
                    onHoveredChanged: if (hovered) Konnekt.help = knobCell.modelData.name + ": " + knobCell.modelData.help
                                                                  + (Konnekt.learning ? "  (LEARN: click, then move a knob on your controller; right-click to forget.)"
                                                                                      : "  (Right-click: what it is and why.)")
                }
                TapHandler { // while learning, right-click forgets a link instead
                    enabled: !Konnekt.learning
                    acceptedButtons: Qt.RightButton
                    onTapped: panel.explain(panel.path + "/knob/" + knobCell.modelData.key)
                }
                Rectangle { // MIDI Learn: click to arm, right-click to forget
                    visible: Konnekt.learning
                    anchors.fill: knob
                    anchors.margins: -4
                    radius: width / 2
                    color: "transparent"
                    border.width: 2
                    border.color: Konnekt.armed(panel.slot, knobCell.index) ? "#FF6A55" : knobCell.cc >= 0 ? "#5FD0FF" : "#4A6A8A"
                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        onClicked: mouse => {
                            if (mouse.button === Qt.RightButton)
                                Konnekt.forget(panel.slot, knobCell.index)
                            else
                                Konnekt.armLearn(panel.slot, knobCell.index)
                        }
                    }
                }
            }
        }
    }

    // A row of jacks (inputs, or outputs with their traces).
    component JackLabel: Text {
        color: "#9A968E"
        font.family: Theme.labelFont
        font.pixelSize: 9
        font.bold: true
        font.letterSpacing: 1
    }

    JackLabel { x: 8; y: panel.inputsTop + 2; text: "IN"; visible: panel.info.inputs.length > 0 }
    Repeater {
        model: panel.info.inputs

        Item {
            id: inJack
            required property int index
            required property var modelData
            readonly property point at: panel.jackPoint(false, index)
            readonly property bool plugged: Konnekt.revision >= 0 && Konnekt.cableInto(panel.slot, index).fromSlot !== undefined
            x: at.x - panel.jackW / 2
            y: at.y - 11
            width: panel.jackW
            height: 44

            Rectangle {
                x: (parent.width - width) / 2
                width: 22
                height: 22
                radius: 11
                color: "#0D0E10"
                border.width: 3
                border.color: panel.signalColors[inJack.modelData.signal]
                Rectangle {
                    anchors.centerIn: parent
                    width: 8
                    height: 8
                    radius: 4
                    color: inJack.plugged ? "#F0EDE6" : "#2A2B2F"
                }
            }
            Text {
                y: 24
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
                text: inJack.modelData.name
                color: "#DAD5CB"
                font.family: Theme.labelFont
                font.pixelSize: 10
            }
            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onClicked: mouse => {
                    if (mouse.button === Qt.RightButton)
                        panel.explain(panel.path + "/in/" + inJack.modelData.key)
                    else
                        panel.jackClicked(panel.slot, false, inJack.index)
                }
                onEntered: Konnekt.help = "Input · " + inJack.modelData.name + ": " + inJack.modelData.help
                                          + (inJack.plugged ? "  (Click to pull its cable out and plug it elsewhere." : "  (Click, then click an output to connect.")
                                          + " Right-click: what it is and why.)"
            }
        }
    }

    JackLabel { x: 8; y: panel.outputsTop + 2; text: "OUT"; visible: panel.info.outputs.length > 0 }
    Repeater {
        model: panel.info.outputs

        Item {
            id: outJack
            required property int index
            required property var modelData
            readonly property point at: panel.jackPoint(true, index)
            x: at.x - panel.jackW / 2
            y: at.y - 11
            width: panel.jackW
            height: 62

            Rectangle {
                x: (parent.width - width) / 2
                width: 22
                height: 22
                radius: 4
                color: panel.signalColors[outJack.modelData.signal]
                Rectangle {
                    anchors.centerIn: parent
                    width: 10
                    height: 10
                    radius: 5
                    color: "#0D0E10"
                }
            }
            Text {
                y: 24
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
                text: outJack.modelData.name
                color: "#DAD5CB"
                font.family: Theme.labelFont
                font.pixelSize: 10
            }
            Canvas { // the live trace: the last 1.5 s, lowest and highest in each slice
                id: traceView
                x: 4
                y: 40
                width: parent.width - 8
                height: 18
                readonly property int frame: Konnekt.frame
                onFrameChanged: if (panel.onScreen) requestPaint()
                onPaint: {
                    const ctx = getContext("2d")
                    ctx.reset()
                    ctx.fillStyle = "#0D0E10"
                    ctx.fillRect(0, 0, width, height)
                    const t = Konnekt.trace(panel.slot, outJack.index)
                    const n = t.length / 2
                    let lo = 0, hi = 0
                    for (let i = 0; i < t.length; ++i) {
                        lo = Math.min(lo, t[i]); hi = Math.max(hi, t[i])
                    }
                    const span = Math.max(hi - lo, 1e-3)
                    const y = v => height - 1 - (v - lo) / span * (height - 2)
                    ctx.strokeStyle = panel.signalColors[outJack.modelData.signal]
                    ctx.beginPath()
                    for (let i = 0; i < n; ++i) { // a stroke from each slice's lowest to its highest
                        const px = i * width / (n - 1)
                        ctx.moveTo(px, y(t[i]) + 0.5)
                        ctx.lineTo(px, y(t[n + i]) - 0.5)
                    }
                    ctx.stroke()
                }
            }
            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onClicked: mouse => {
                    if (mouse.button === Qt.RightButton) {
                        panel.jackScope(panel.slot, outJack.index)
                        panel.explain(panel.path + "/out/" + outJack.modelData.key)
                    } else
                        panel.jackClicked(panel.slot, true, outJack.index)
                }
                onEntered: Konnekt.help = "Output · " + outJack.modelData.name + ": " + outJack.modelData.help
                                          + "  (Click, then click an input to connect. Right-click: what it is and why, and see it on the scope.)"
            }
        }
    }

    // The Scope module's own screen: what's plugged into it, live.
    Rectangle {
        visible: panel.isScope
        x: 6
        y: panel.outputsTop + panel.outputsHeight
        width: panel.width - 12
        height: 140
        radius: 4
        color: "#081008"
        border.color: "#2A3A2A"
        Canvas {
            id: screen
            anchors.fill: parent
            anchors.margins: 4
            readonly property int frame: Konnekt.frame
            onFrameChanged: if (panel.isScope && panel.onScreen && Konnekt.scopeSlot() === panel.slot) requestPaint()
            onPaint: {
                const ctx = getContext("2d")
                ctx.reset()
                ctx.strokeStyle = "#1E3A1E"
                ctx.beginPath(); ctx.moveTo(0, height / 2); ctx.lineTo(width, height / 2); ctx.stroke()
                const spans = [240, 960, 3840] // 5, 20, 80 ms at 48 kHz
                const n = spans[Math.round(Konnekt.knobRevision >= 0 ? Konnekt.knob(panel.slot, 0) * 2 : 1)]
                const s = Konnekt.scopeSamples(n * 2)
                // Start at a rising zero crossing in the first half: a steady picture.
                let start = 0
                for (let i = 1; i < n; ++i)
                    if (s[i - 1] < 0 && s[i] >= 0) { start = i; break }
                ctx.strokeStyle = "#6FE86F"
                ctx.lineWidth = 1.5
                ctx.beginPath()
                for (let i = 0; i < n; ++i) {
                    const px = i * width / (n - 1)
                    const py = height / 2 - Math.max(-1.2, Math.min(1.2, s[start + i])) * height / 2.6
                    if (i === 0) ctx.moveTo(px, py); else ctx.lineTo(px, py)
                }
                ctx.stroke()
            }
        }
        Text {
            visible: Konnekt.revision >= 0 && Konnekt.scopeSlot() !== panel.slot
            anchors.centerIn: parent
            text: "Click to show what's plugged in"
            color: "#4A7A4A"
            font.family: Theme.labelFont
            font.pixelSize: 11
        }
        MouseArea {
            anchors.fill: parent
            onClicked: panel.jackScope(panel.slot, 0)
        }
    }
}

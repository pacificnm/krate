pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Shapes
import Krate

// Konnekt's panel (its window standalone, a tab in the Krate rack): the top
// bar (add a module, voices, learn, new, audio),
// the rack (modules and their cables, scrolling), the scope, the help bar.
// Wiring: click a jack, then another (an output and an input); clicking a
// plugged input lifts its cable to plug it somewhere else; Esc or a click on
// the empty rack lets go. Right-clicking anything opens its explanation.
Item {
    id: panel

    width: 1500
    height: 920
    clip: true // cables stay inside (in the rack nothing else cuts them off)
    // Its controller: one per copy of Konnekt.
    required property Konnekt konnekt
    // In the rack: sized to the panel area rather than scaled (it lays itself out).
    readonly property bool fillsSlot: true
    // Its device in the rack (set by Krate), -1 standalone: the rack's top bar
    // shows the audio status and level, so they're left out here.
    property int rackDevice: -1

    Rectangle { anchors.fill: parent; color: "#141518" }

    property var panels: ({})   // slot -> its ModulePanel
    property var pending: null  // the jack clicked first: {slot, output, jack}
    property point mouse: Qt.point(0, 0)
    readonly property var cableColors: ["#E8C547", "#E85A4F", "#4FB0E8", "#7ED957", "#E87FD0", "#F2994A", "#9B8CF2", "#E8E8E8"]

    function register(slot, item) {
        const p = Object.assign({}, panels)
        if (item)
            p[slot] = item
        else
            delete p[slot]
        panels = p
    }
    // Right-click on a top-bar button: its explanation.
    component ExplainTap: TapHandler {
        property string path
        acceptedButtons: Qt.RightButton
        onTapped: explainPanel.show(path)
    }

    function jackAt(slot, output, jack) {
        const item = panels[slot]
        if (!item)
            return null
        const pt = item.jackPoint(output, jack)
        return Qt.point(item.x + pt.x, item.y + pt.y)
    }
    function jackClicked(slot, output, jack) {
        if (!pending) {
            if (!output) {
                const c = panel.konnekt.cableInto(slot, jack)
                if (c.fromSlot !== undefined) { // lift the cable: its other end stays in your hand
                    panel.konnekt.unplug(slot, jack)
                    pending = { slot: c.fromSlot, output: true, jack: c.fromJack }
                    return
                }
            }
            pending = { slot: slot, output: output, jack: jack }
            return
        }
        if (pending.output !== output) {
            const out = output ? { slot: slot, jack: jack } : pending
            const inp = output ? pending : { slot: slot, jack: jack }
            panel.konnekt.plug(out.slot, out.jack, inp.slot, inp.jack)
            pending = null
        } else {
            pending = { slot: slot, output: output, jack: jack }
        }
    }

    // ---- Top bar
    Rectangle {
        id: topBar
        width: parent.width
        height: 54
        color: "#1F2125"
        z: 20

        Text {
            x: 18
            anchors.verticalCenter: parent.verticalCenter
            text: "KONNEKT"
            color: "#F2B632"
            font.family: Theme.labelFont
            font.pixelSize: 26
            font.weight: Font.Black
            font.letterSpacing: 6
            ExplainTap { path: "app/explain" }
        }
        Row {
            x: 210
            anchors.verticalCenter: parent.verticalCenter
            spacing: 10

            PanelButton {
                width: 130
                height: 34
                dark: true
                faceText: "+ ADD MODULE"
                text: "Add a module to the rack"
                onClicked: browser.open()
                ExplainTap { path: "app/add" }
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "VOICES"
                ExplainTap { path: "app/voices" }
                color: "#DAD5CB"
                font.family: Theme.labelFont
                font.pixelSize: 12
                font.bold: true
            }
            PanelButton { width: 28; height: 30; dark: true; faceText: "◂"; text: "Fewer voices"; onClicked: panel.konnekt.voices = panel.konnekt.voices - 1; ExplainTap { path: "app/voices" } }
            Text {
                width: 22
                horizontalAlignment: Text.AlignHCenter
                anchors.verticalCenter: parent.verticalCenter
                text: panel.konnekt.voices
                color: "#F2B632"
                font.family: "monospace"
                font.pixelSize: 20
                font.bold: true
            }
            PanelButton { width: 28; height: 30; dark: true; faceText: "▸"; text: "More voices"; onClicked: panel.konnekt.voices = panel.konnekt.voices + 1; ExplainTap { path: "app/voices" } }
            Row { // a light per note held
                anchors.verticalCenter: parent.verticalCenter
                spacing: 3
                Repeater {
                    model: panel.konnekt.voices
                    Rectangle {
                        required property int index
                        width: 8
                        height: 8
                        radius: 4
                        color: index < panel.konnekt.notesHeld ? "#7ED957" : "#33363C"
                    }
                }
            }
            Item { width: 16; height: 1 }
            PanelButton {
                width: 80
                height: 34
                dark: true
                faceText: "LEARN"
                text: "MIDI Learn: click a knob, then move a knob on your controller"
                hasLed: true
                ledOn: panel.konnekt.learning
                onClicked: panel.konnekt.learning = !panel.konnekt.learning
                ExplainTap { path: "app/learn" }
            }
            PanelButton {
                width: 100
                height: 34
                dark: true
                faceText: "PATCHES"
                text: "Save the rack as a patch, or load one"
                onClicked: patchesPopup.open()
                ExplainTap { path: "app/patches" }
            }
            PanelButton {
                width: 100
                height: 34
                dark: true
                faceText: "LESSONS"
                text: "Patches that build a synth up a module at a time, with what to try"
                hasLed: true
                ledOn: panel.konnekt.lesson >= 0
                onClicked: lessonsPopup.open()
                ExplainTap { path: "app/lessons" }
            }
            PanelButton {
                width: 100
                height: 34
                dark: true
                faceText: "PRESETS"
                text: "Finished sounds to play and take apart: basses, leads, 80s leads, pads, drums, effects"
                hasLed: true
                ledOn: panel.konnekt.preset >= 0
                onClicked: presetsPopup.open()
                ExplainTap { path: "app/presets" }
            }
            PanelButton {
                width: 70
                height: 34
                dark: true
                faceText: "HELP"
                text: "Konnekt's manual"
                onClicked: manual.open()
                ExplainTap { path: "app/help" }
            }
            PanelButton {
                id: newButton
                property bool confirming: false
                width: 110
                height: 34
                dark: true
                faceText: confirming ? "CLEAR ALL?" : "NEW"
                text: "Start an empty rack"
                tint: confirming ? "#B8302A" : "transparent"
                onClicked: {
                    if (!confirming) { confirming = true; confirmTimer.restart(); return }
                    confirming = false
                    panel.konnekt.clearPatch()
                }
                Timer { id: confirmTimer; interval: 3000; onTriggered: newButton.confirming = false }
                ExplainTap { path: "app/new" }
            }
        }
        Column {
            anchors.right: parent.right
            anchors.rightMargin: 18
            anchors.verticalCenter: parent.verticalCenter
            spacing: 4
            Text {
                width: 260
                horizontalAlignment: Text.AlignRight
                elide: Text.ElideRight
                visible: panel.konnekt.patchName !== ""
                text: panel.konnekt.patchName
                color: "#F2B632"
                font.family: Theme.labelFont
                font.pixelSize: 12
                font.bold: true
            }
            Text {
                width: 260
                visible: panel.rackDevice < 0
                horizontalAlignment: Text.AlignRight
                elide: Text.ElideRight
                text: Host.audioStatus
                color: Host.audioConnected ? "#9A968E" : Theme.ledOn
                font.family: Theme.labelFont
                font.pixelSize: 11
            }
            Rectangle {
                width: 260
                height: 6
                visible: panel.rackDevice < 0
                radius: 2
                color: "#33363C"
                Rectangle {
                    width: parent.width * Host.outputLevel
                    height: parent.height
                    radius: 2
                    color: "#7ED957"
                }
            }
        }
    }

    // ---- The rack
    Flickable {
        id: flick
        y: topBar.height
        width: parent.width
        height: parent.height - topBar.height - helpBar.height
        contentWidth: 5200
        contentHeight: 3200
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        interactive: false // dragging moves modules; the scroll bars and the wheel move the view
        ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AlwaysOn }
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AlwaysOn }

        WheelHandler {
            onWheel: event => {
                const dx = event.modifiers & Qt.ShiftModifier ? event.angleDelta.y : event.angleDelta.x
                const dy = event.modifiers & Qt.ShiftModifier ? 0 : event.angleDelta.y
                flick.contentX = Math.max(0, Math.min(flick.contentWidth - flick.width, flick.contentX - dx))
                flick.contentY = Math.max(0, Math.min(flick.contentHeight - flick.height, flick.contentY - dy))
            }
        }

        Item {
            id: rack
            width: flick.contentWidth
            height: flick.contentHeight

            Rectangle { // background: click it to let go of a cable
                anchors.fill: parent
                color: "#17181B"
                MouseArea {
                    anchors.fill: parent
                    onClicked: panel.pending = null
                }
            }
            Repeater { // a faint grid
                model: Math.ceil(rack.width / 64)
                Rectangle {
                    required property int index
                    x: index * 64
                    width: 1
                    height: rack.height
                    color: "#1D1F23"
                }
            }

            Repeater {
                model: panel.konnekt.moduleRevision >= 0 ? panel.konnekt.modules() : []

                ModulePanel {
                    required property var modelData
                    konnekt: panel.konnekt
                    slot: modelData.slot
                    type: modelData.type
                    x: modelData.x
                    y: modelData.y
                    onScreen: x + width > flick.contentX && x < flick.contentX + flick.width
                              && y + height > flick.contentY && y < flick.contentY + flick.height
                    onJackClicked: (s, o, j) => panel.jackClicked(s, o, j)
                    onJackScope: (s, j) => panel.konnekt.scope(s, j)
                    onExplain: p => explainPanel.show(p)
                    onMoved: {
                        x = Math.max(0, Math.round(x / 8) * 8)
                        y = Math.max(0, Math.round(y / 8) * 8)
                        panel.konnekt.moveModule(slot, x, y)
                    }
                    Component.onCompleted: panel.register(slot, this)
                    Component.onDestruction: panel.register(slot, null)
                }
            }

            // The cables, over the modules (they don't take clicks).
            Repeater {
                model: panel.konnekt.revision >= 0 ? panel.konnekt.cables() : []

                Shape {
                    id: cable
                    required property var modelData
                    readonly property point a: panel.panels[modelData.fromSlot] ? panel.jackAt(modelData.fromSlot, true, modelData.fromJack) : Qt.point(0, 0)
                    readonly property point b: panel.panels[modelData.toSlot] ? panel.jackAt(modelData.toSlot, false, modelData.toJack) : Qt.point(0, 0)
                    readonly property real sag: 30 + 0.25 * Math.hypot(b.x - a.x, b.y - a.y)
                    visible: panel.panels[modelData.fromSlot] !== undefined && panel.panels[modelData.toSlot] !== undefined
                    anchors.fill: parent
                    z: 10
                    preferredRendererType: Shape.CurveRenderer
                    opacity: 0.85

                    ShapePath {
                        strokeColor: panel.cableColors[cable.modelData.color % panel.cableColors.length]
                        strokeWidth: 5
                        fillColor: "transparent"
                        capStyle: ShapePath.RoundCap
                        startX: cable.a.x
                        startY: cable.a.y
                        PathCubic {
                            x: cable.b.x
                            y: cable.b.y
                            control1X: cable.a.x
                            control1Y: cable.a.y + cable.sag
                            control2X: cable.b.x
                            control2Y: cable.b.y + cable.sag
                        }
                    }
                }
            }
            // The cable in your hand, to the mouse.
            Shape {
                id: holding
                readonly property point a: panel.pending ? panel.jackAt(panel.pending.slot, panel.pending.output, panel.pending.jack) : Qt.point(0, 0)
                visible: panel.pending !== null
                anchors.fill: parent
                z: 11
                preferredRendererType: Shape.CurveRenderer
                ShapePath {
                    strokeColor: "#F2B632"
                    strokeWidth: 3
                    strokeStyle: ShapePath.DashLine
                    fillColor: "transparent"
                    startX: holding.a.x
                    startY: holding.a.y
                    PathLine { x: panel.mouse.x; y: panel.mouse.y }
                }
            }
            HoverHandler {
                onPointChanged: panel.mouse = point.position
            }
        }
    }

    // ---- The scope: right-click an output to watch it here.
    Rectangle {
        id: scopeView
        visible: panel.konnekt.scopeName !== "" && !panel.konnekt.scopeName.startsWith("Scope ·")
        anchors.right: parent.right
        anchors.bottom: helpBar.top
        anchors.margins: 14
        anchors.rightMargin: explainPanel.visible ? explainPanel.width + 14 : 14
        width: 420
        height: 200
        radius: 6
        color: "#081008"
        border.color: "#2A3A2A"
        border.width: 2
        z: 30

        Text {
            x: 10
            y: 6
            text: "SCOPE  ·  " + panel.konnekt.scopeName
            color: "#6FE86F"
            font.family: Theme.labelFont
            font.pixelSize: 11
            font.bold: true
        }
        Text {
            anchors.right: parent.right
            anchors.rightMargin: 10
            y: 4
            text: "✕"
            color: "#6FE86F"
            font.pixelSize: 14
            MouseArea { anchors.fill: parent; anchors.margins: -4; onClicked: panel.konnekt.scope(-1, -1) }
        }
        Row {
            anchors.right: parent.right
            anchors.rightMargin: 34
            y: 4
            spacing: 4
            Repeater {
                model: [{ name: "5 ms", n: 240 }, { name: "20 ms", n: 960 }, { name: "80 ms", n: 3840 }]
                Text {
                    required property var modelData
                    text: modelData.name
                    color: scopeCanvas.span === modelData.n ? "#6FE86F" : "#3A6A3A"
                    font.family: Theme.labelFont
                    font.pixelSize: 11
                    MouseArea { anchors.fill: parent; onClicked: scopeCanvas.span = parent.modelData.n }
                }
            }
        }
        Canvas {
            id: scopeCanvas
            property int span: 960
            x: 8
            y: 24
            width: parent.width - 16
            height: parent.height - 32
            readonly property int frame: panel.konnekt.frame
            onFrameChanged: if (scopeView.visible) requestPaint()
            onPaint: {
                const ctx = getContext("2d")
                ctx.reset()
                ctx.strokeStyle = "#1E3A1E"
                for (let k = 1; k < 4; ++k) {
                    ctx.beginPath(); ctx.moveTo(0, k * height / 4); ctx.lineTo(width, k * height / 4); ctx.stroke()
                }
                const n = span
                const s = panel.konnekt.scopeSamples(n * 2)
                let lo = -1, hi = 1
                for (let i = 0; i < s.length; ++i) { lo = Math.min(lo, s[i]); hi = Math.max(hi, s[i]) }
                let start = 0 // a rising crossing of the middle: a steady picture
                const mid = (lo + hi) / 2
                for (let i = 1; i < n; ++i)
                    if (s[i - 1] < mid && s[i] >= mid) { start = i; break }
                ctx.strokeStyle = "#6FE86F"
                ctx.lineWidth = 1.5
                ctx.beginPath()
                for (let i = 0; i < n; ++i) {
                    const px = i * width / (n - 1)
                    const py = height - (s[start + i] - lo) / (hi - lo) * height
                    if (i === 0) ctx.moveTo(px, py); else ctx.lineTo(px, py)
                }
                ctx.stroke()
                ctx.fillStyle = "#3A6A3A"
                ctx.font = "10px sans-serif"
                ctx.fillText(hi.toFixed(2), 2, 10)
                ctx.fillText(lo.toFixed(2), 2, height - 2)
            }
        }
    }

    // ---- The help bar
    Rectangle {
        id: helpBar
        anchors.bottom: parent.bottom
        width: parent.width
        height: 46
        color: "#1F2125"
        z: 20
        Text {
            x: 18
            width: parent.width - 36
            anchors.verticalCenter: parent.verticalCenter
            wrapMode: Text.WordWrap
            maximumLineCount: 2
            elide: Text.ElideRight
            text: panel.pending ? "Holding a cable from " + (panel.pending.output ? "an output" : "an input")
                                 + ": click " + (panel.pending.output ? "an input" : "an output") + " to plug it in (Esc, or click the rack, to let go)."
                : panel.konnekt.help !== "" ? panel.konnekt.help
                : "Point at anything for a line about it; right-click it for the whole story. Click a jack, then another, to connect them."
            color: "#DAD5CB"
            font.family: Theme.labelFont
            font.pixelSize: 13
        }
    }

    // ---- Adding a module
    Popup {
        id: browser
        x: 200
        y: topBar.height + 4
        width: Math.min(panel.width - 220, 1100)
        padding: 14
        modal: true
        background: Rectangle { radius: 8; color: "#23252A"; border.color: "#3A3D44" }

        readonly property var categories: {
            const seen = []
            for (const m of panel.konnekt.moduleTypes)
                if (seen.indexOf(m.category) < 0)
                    seen.push(m.category)
            return seen
        }
        property int added: 0

        Flow {
            width: parent.width
            spacing: 16
            Repeater {
                model: browser.categories
                Column {
                    id: category
                    required property string modelData
                    spacing: 6
                    Text {
                        text: category.modelData.toUpperCase()
                        color: "#F2B632"
                        font.family: Theme.labelFont
                        font.pixelSize: 12
                        font.bold: true
                    }
                    Repeater {
                        model: panel.konnekt.moduleTypes.filter(m => m.category === category.modelData)
                        PanelButton {
                            required property var modelData
                            width: 170
                            height: 30
                            dark: true
                            faceText: modelData.name
                            text: modelData.help
                            onClicked: {
                                // In view, a little further along each time so they don't stack.
                                const step = 24 * (browser.added++ % 10)
                                const slot = panel.konnekt.addModule(modelData.type, flick.contentX + 40 + step, flick.contentY + 30 + step)
                                if (modelData.type === "scope" && slot >= 0)
                                    panel.konnekt.scope(slot, 0)
                                browser.close()
                            }
                            HoverHandler { onHoveredChanged: if (hovered) panel.konnekt.help = parent.modelData.name + ": " + parent.modelData.help + "  (Right-click: read about it first.)" }
                            TapHandler {
                                acceptedButtons: Qt.RightButton
                                onTapped: { explainPanel.show("t/" + parent.modelData.type); browser.close() }
                            }
                        }
                    }
                }
            }
        }
    }

    // ---- Patches: save the rack by name, load or delete one
    Popup {
        id: patchesPopup
        x: 560
        y: topBar.height + 4
        width: 340
        padding: 14
        modal: true
        background: Rectangle { radius: 8; color: "#23252A"; border.color: "#3A3D44" }
        onOpened: nameField.text = panel.konnekt.patchName

        Column {
            width: parent.width
            spacing: 8
            Text { text: "SAVE THE RACK AS"; color: "#F2B632"; font.family: Theme.labelFont; font.pixelSize: 12; font.bold: true }
            Row {
                spacing: 8
                TextField {
                    id: nameField
                    width: 220
                    placeholderText: "a name"
                    onAccepted: if (panel.konnekt.savePatch(text)) patchesPopup.close()
                }
                PanelButton {
                    width: 80
                    height: 32
                    dark: true
                    faceText: "SAVE"
                    text: "Save the rack under this name (the same name replaces it)"
                    onClicked: if (panel.konnekt.savePatch(nameField.text)) patchesPopup.close()
                }
            }
            PanelButton {
                width: 310
                height: 32
                dark: true
                faceText: "SEND TO AN INSTRUMENT…"
                text: "Make the rack a preset of Kalliope, Kobalt or Krimson"
                onClicked: { patchesPopup.close(); sendPopup.open() }
                ExplainTap { path: "app/send" }
            }
            Text {
                text: panel.konnekt.patchNames.length > 0 ? "YOUR PATCHES  (click to load)" : "No saved patches yet."
                color: "#F2B632"
                font.family: Theme.labelFont
                font.pixelSize: 12
                font.bold: true
            }
            Repeater {
                model: panel.konnekt.patchNames
                Row {
                    id: patchRow
                    required property string modelData
                    spacing: 8
                    PanelButton {
                        width: 270
                        height: 28
                        dark: true
                        faceText: patchRow.modelData
                        text: "Load " + patchRow.modelData + " (the rack as it is now is replaced: save it first)"
                        onClicked: { panel.konnekt.loadPatch(patchRow.modelData); patchesPopup.close() }
                    }
                    PanelButton {
                        width: 32
                        height: 28
                        dark: true
                        faceText: "✕"
                        text: "Delete " + patchRow.modelData
                        onClicked: panel.konnekt.deletePatch(patchRow.modelData)
                    }
                }
            }
        }
    }

    // ---- Presets: finished sounds, by category (six columns at full width; it scrolls when narrower)
    Popup {
        id: presetsPopup
        width: Math.min(panel.width - 40, 1360)
        height: Math.min(presetsBody.height + 2 * padding, panel.height - topBar.height - 24)
        x: (panel.width - width) / 2
        y: topBar.height + 4
        padding: 14
        modal: true
        background: Rectangle { radius: 8; color: "#23252A"; border.color: "#3A3D44" }

        readonly property var categories: {
            const seen = []
            for (const p of panel.konnekt.presets)
                if (seen.indexOf(p.category) < 0)
                    seen.push(p.category)
            return seen
        }

        Flickable {
            anchors.fill: parent
            contentHeight: presetsBody.height
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { }

            Column {
                id: presetsBody
                width: parent.width
                spacing: 10
                Text {
                    width: parent.width
                    wrapMode: Text.WordWrap
                    text: "PRESETS: finished sounds to play and take apart. Each replaces the rack (save yours first) and says how it's built and what to tweak."
                    color: "#F2B632"
                    font.family: Theme.labelFont
                    font.pixelSize: 12
                    font.bold: true
                }
                Flow {
                    width: parent.width
                    spacing: 12
                    Repeater {
                        model: presetsPopup.categories
                        Column {
                            id: presetCategory
                            required property string modelData
                            spacing: 6
                            Text {
                                text: presetCategory.modelData.toUpperCase()
                                color: "#9A968E"
                                font.family: Theme.labelFont
                                font.pixelSize: 11
                                font.bold: true
                            }
                            Repeater {
                                model: panel.konnekt.presets.filter(p => p.category === presetCategory.modelData)
                                PanelButton {
                                    required property var modelData
                                    width: 210
                                    height: 28
                                    dark: true
                                    faceText: modelData.title
                                    text: "Load " + modelData.title + " (the rack is replaced: save yours first)"
                                    onClicked: { panel.konnekt.loadPreset(modelData.index); presetsPopup.close() }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // ---- Lessons: a list to pick from, and the lesson's text while it's loaded
    Popup {
        id: lessonsPopup
        x: 560
        y: topBar.height + 4
        width: 668
        padding: 14
        modal: true
        background: Rectangle { radius: 8; color: "#23252A"; border.color: "#3A3D44" }
        Column {
            width: parent.width
            spacing: 8
            Text {
                width: parent.width
                wrapMode: Text.WordWrap
                text: "LESSONS: each loads a patch (replacing the rack: save yours first) and says what to try. The basics on the left, in order; then the advanced techniques."
                color: "#F2B632"
                font.family: Theme.labelFont
                font.pixelSize: 12
                font.bold: true
            }
            Row {
                spacing: 12
                Repeater {
                    model: [{ title: "BASICS", from: 0, to: 10 }, { title: "ADVANCED", from: 10, to: 20 }]
                    Column {
                        id: lessonColumn
                        required property var modelData
                        spacing: 6
                        Text {
                            text: lessonColumn.modelData.title
                            color: "#9A968E"
                            font.family: Theme.labelFont
                            font.pixelSize: 11
                            font.bold: true
                        }
                        Repeater {
                            model: panel.konnekt.lessons.slice(lessonColumn.modelData.from, lessonColumn.modelData.to)
                            PanelButton {
                                required property var modelData
                                width: 314
                                height: 28
                                dark: true
                                faceText: (modelData.index + 1) + " · " + modelData.title.replace(/^Advanced [0-9]+ · /, "")
                                text: "Lesson " + (modelData.index + 1) + ": " + modelData.title
                                onClicked: { panel.konnekt.loadLesson(modelData.index); lessonsPopup.close() }
                            }
                        }
                    }
                }
            }
        }
    }
    Rectangle {
        id: lessonPanel
        readonly property bool isPreset: panel.konnekt.preset >= 0
        readonly property int index: isPreset ? panel.konnekt.preset : panel.konnekt.lesson
        readonly property int count: isPreset ? panel.konnekt.presets.length : panel.konnekt.lessons.length
        function go(i) { if (isPreset) panel.konnekt.loadPreset(i); else panel.konnekt.loadLesson(i) }
        visible: index >= 0
        anchors.left: parent.left
        anchors.bottom: helpBar.top
        anchors.margins: 14
        width: 480
        height: 230
        radius: 6
        color: "#23252A"
        border.color: "#F2B632"
        border.width: 2
        z: 30

        Text {
            x: 12
            y: 8
            width: parent.width - 120
            elide: Text.ElideRight
            text: (lessonPanel.isPreset ? "PRESET " : "LESSON ") + panel.konnekt.sheetTitle
            color: "#F2B632"
            font.family: Theme.labelFont
            font.pixelSize: 13
            font.bold: true
        }
        Row {
            anchors.right: parent.right
            anchors.rightMargin: 10
            y: 4
            spacing: 6
            PanelButton { width: 28; height: 24; dark: true; faceText: "◂"; text: "The one before"; enabled: lessonPanel.index > 0; onClicked: lessonPanel.go(lessonPanel.index - 1) }
            PanelButton { width: 28; height: 24; dark: true; faceText: "▸"; text: "The next one"; enabled: lessonPanel.index + 1 < lessonPanel.count; onClicked: lessonPanel.go(lessonPanel.index + 1) }
            PanelButton { width: 28; height: 24; dark: true; faceText: "✕"; text: "Close the text (the patch stays)"; onClicked: panel.konnekt.closeSheet() }
        }
        Flickable {
            x: 12
            y: 34
            width: parent.width - 24
            height: parent.height - 44
            contentHeight: lessonText.height
            clip: true
            Text {
                id: lessonText
                width: parent.width
                wrapMode: Text.WordWrap
                text: panel.konnekt.sheetText
                color: "#DAD5CB"
                font.family: Theme.labelFont
                font.pixelSize: 13
                lineHeight: 1.15
            }
        }
    }

    // ---- Explanations: right-click anything
    ExplainPanel {
        id: explainPanel
        konnekt: panel.konnekt
        anchors.right: parent.right
        anchors.top: topBar.bottom
        anchors.bottom: helpBar.top
        z: 35
        onShowScope: (s, j) => panel.konnekt.scope(s, j)
    }

    SendPopup {
        id: sendPopup
        konnekt: panel.konnekt
        // A name to start from: the patch's, the preset's, or the lesson's.
        defaultName: (panel.konnekt.patchName !== "" ? panel.konnekt.patchName
                      : panel.konnekt.sheetTitle !== "" ? panel.konnekt.sheetTitle.replace(/^[0-9]+ · /, "") : "Konnekt").slice(0, 16)
    }

    ManualView {
        id: manual
        title: "Konnekt"
        source: "qrc:/qt/qml/Konnekt/docs/MANUAL.md"
    }

    Shortcut {
        sequence: "Esc"
        enabled: panel.visible // in the rack: the Konnekt on show
        onActivated: {
            panel.pending = null
            panel.konnekt.learning = false
            explainPanel.close()
        }
    }
}

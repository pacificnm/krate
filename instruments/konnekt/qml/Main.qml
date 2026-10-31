pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Shapes
import Krate

// The Konnekt window: the top bar (add a module, voices, learn, new, audio),
// the rack (modules and their cables, scrolling), the scope, the help bar.
// Wiring: click a jack, then another (an output and an input); clicking a
// plugged input lifts its cable to plug it somewhere else; Esc or a click on
// the empty rack lets go. Right-clicking anything opens its explanation.
ApplicationWindow {
    id: root

    width: 1500
    height: 920
    minimumWidth: 900
    minimumHeight: 600
    visible: true
    title: "Konnekt"
    color: "#141518"

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
                const c = Konnekt.cableInto(slot, jack)
                if (c.fromSlot !== undefined) { // lift the cable: its other end stays in your hand
                    Konnekt.unplug(slot, jack)
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
            Konnekt.plug(out.slot, out.jack, inp.slot, inp.jack)
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
            PanelButton { width: 28; height: 30; dark: true; faceText: "◂"; text: "Fewer voices"; onClicked: Konnekt.voices = Konnekt.voices - 1; ExplainTap { path: "app/voices" } }
            Text {
                width: 22
                horizontalAlignment: Text.AlignHCenter
                anchors.verticalCenter: parent.verticalCenter
                text: Konnekt.voices
                color: "#F2B632"
                font.family: "monospace"
                font.pixelSize: 20
                font.bold: true
            }
            PanelButton { width: 28; height: 30; dark: true; faceText: "▸"; text: "More voices"; onClicked: Konnekt.voices = Konnekt.voices + 1; ExplainTap { path: "app/voices" } }
            Row { // a light per note held
                anchors.verticalCenter: parent.verticalCenter
                spacing: 3
                Repeater {
                    model: Konnekt.voices
                    Rectangle {
                        required property int index
                        width: 8
                        height: 8
                        radius: 4
                        color: index < Konnekt.notesHeld ? "#7ED957" : "#33363C"
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
                ledOn: Konnekt.learning
                onClicked: Konnekt.learning = !Konnekt.learning
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
                ledOn: Konnekt.lesson >= 0
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
                ledOn: Konnekt.preset >= 0
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
                    Konnekt.clearPatch()
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
                visible: Konnekt.patchName !== ""
                text: Konnekt.patchName
                color: "#F2B632"
                font.family: Theme.labelFont
                font.pixelSize: 12
                font.bold: true
            }
            Text {
                width: 260
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
                    onClicked: root.pending = null
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
                model: Konnekt.moduleRevision >= 0 ? Konnekt.modules() : []

                ModulePanel {
                    required property var modelData
                    slot: modelData.slot
                    type: modelData.type
                    x: modelData.x
                    y: modelData.y
                    onJackClicked: (s, o, j) => root.jackClicked(s, o, j)
                    onJackScope: (s, j) => Konnekt.scope(s, j)
                    onExplain: p => explainPanel.show(p)
                    onMoved: {
                        x = Math.max(0, Math.round(x / 8) * 8)
                        y = Math.max(0, Math.round(y / 8) * 8)
                        Konnekt.moveModule(slot, x, y)
                    }
                    Component.onCompleted: root.register(slot, this)
                    Component.onDestruction: root.register(slot, null)
                }
            }

            // The cables, over the modules (they don't take clicks).
            Repeater {
                model: Konnekt.revision >= 0 ? Konnekt.cables() : []

                Shape {
                    id: cable
                    required property var modelData
                    readonly property point a: root.panels[modelData.fromSlot] ? root.jackAt(modelData.fromSlot, true, modelData.fromJack) : Qt.point(0, 0)
                    readonly property point b: root.panels[modelData.toSlot] ? root.jackAt(modelData.toSlot, false, modelData.toJack) : Qt.point(0, 0)
                    readonly property real sag: 30 + 0.25 * Math.hypot(b.x - a.x, b.y - a.y)
                    visible: root.panels[modelData.fromSlot] !== undefined && root.panels[modelData.toSlot] !== undefined
                    anchors.fill: parent
                    z: 10
                    preferredRendererType: Shape.CurveRenderer
                    opacity: 0.85

                    ShapePath {
                        strokeColor: root.cableColors[cable.modelData.color % root.cableColors.length]
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
                readonly property point a: root.pending ? root.jackAt(root.pending.slot, root.pending.output, root.pending.jack) : Qt.point(0, 0)
                visible: root.pending !== null
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
                    PathLine { x: root.mouse.x; y: root.mouse.y }
                }
            }
            HoverHandler {
                onPointChanged: root.mouse = point.position
            }
        }
    }

    // ---- The scope: right-click an output to watch it here.
    Rectangle {
        id: scopeView
        visible: Konnekt.scopeName !== "" && !Konnekt.scopeName.startsWith("Scope ·")
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
            text: "SCOPE  ·  " + Konnekt.scopeName
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
            MouseArea { anchors.fill: parent; anchors.margins: -4; onClicked: Konnekt.scope(-1, -1) }
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
            readonly property int frame: Konnekt.frame
            onFrameChanged: if (scopeView.visible) requestPaint()
            onPaint: {
                const ctx = getContext("2d")
                ctx.reset()
                ctx.strokeStyle = "#1E3A1E"
                for (let k = 1; k < 4; ++k) {
                    ctx.beginPath(); ctx.moveTo(0, k * height / 4); ctx.lineTo(width, k * height / 4); ctx.stroke()
                }
                const n = span
                const s = Konnekt.scopeSamples(n * 2)
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
            text: root.pending ? "Holding a cable from " + (root.pending.output ? "an output" : "an input")
                                 + ": click " + (root.pending.output ? "an input" : "an output") + " to plug it in (Esc, or click the rack, to let go)."
                : Konnekt.help !== "" ? Konnekt.help
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
        width: Math.min(root.width - 220, 1100)
        padding: 14
        modal: true
        background: Rectangle { radius: 8; color: "#23252A"; border.color: "#3A3D44" }

        readonly property var categories: {
            const seen = []
            for (const m of Konnekt.moduleTypes)
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
                        model: Konnekt.moduleTypes.filter(m => m.category === category.modelData)
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
                                const slot = Konnekt.addModule(modelData.type, flick.contentX + 40 + step, flick.contentY + 30 + step)
                                if (modelData.type === "scope" && slot >= 0)
                                    Konnekt.scope(slot, 0)
                                browser.close()
                            }
                            HoverHandler { onHoveredChanged: if (hovered) Konnekt.help = parent.modelData.name + ": " + parent.modelData.help + "  (Right-click: read about it first.)" }
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
        onOpened: nameField.text = Konnekt.patchName

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
                    onAccepted: if (Konnekt.savePatch(text)) patchesPopup.close()
                }
                PanelButton {
                    width: 80
                    height: 32
                    dark: true
                    faceText: "SAVE"
                    text: "Save the rack under this name (the same name replaces it)"
                    onClicked: if (Konnekt.savePatch(nameField.text)) patchesPopup.close()
                }
            }
            Text {
                text: Konnekt.patchNames.length > 0 ? "YOUR PATCHES  (click to load)" : "No saved patches yet."
                color: "#F2B632"
                font.family: Theme.labelFont
                font.pixelSize: 12
                font.bold: true
            }
            Repeater {
                model: Konnekt.patchNames
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
                        onClicked: { Konnekt.loadPatch(patchRow.modelData); patchesPopup.close() }
                    }
                    PanelButton {
                        width: 32
                        height: 28
                        dark: true
                        faceText: "✕"
                        text: "Delete " + patchRow.modelData
                        onClicked: Konnekt.deletePatch(patchRow.modelData)
                    }
                }
            }
        }
    }

    // ---- Presets: finished sounds, by category (six columns at full width; it scrolls when narrower)
    Popup {
        id: presetsPopup
        width: Math.min(root.width - 40, 1360)
        height: Math.min(presetsBody.height + 2 * padding, root.height - topBar.height - 24)
        x: (root.width - width) / 2
        y: topBar.height + 4
        padding: 14
        modal: true
        background: Rectangle { radius: 8; color: "#23252A"; border.color: "#3A3D44" }

        readonly property var categories: {
            const seen = []
            for (const p of Konnekt.presets)
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
                                model: Konnekt.presets.filter(p => p.category === presetCategory.modelData)
                                PanelButton {
                                    required property var modelData
                                    width: 210
                                    height: 28
                                    dark: true
                                    faceText: modelData.title
                                    text: "Load " + modelData.title + " (the rack is replaced: save yours first)"
                                    onClicked: { Konnekt.loadPreset(modelData.index); presetsPopup.close() }
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
        x: 660
        y: topBar.height + 4
        width: 340
        padding: 14
        modal: true
        background: Rectangle { radius: 8; color: "#23252A"; border.color: "#3A3D44" }
        Column {
            width: parent.width
            spacing: 6
            Text {
                width: parent.width
                wrapMode: Text.WordWrap
                text: "LESSONS: each loads a patch (replacing the rack: save yours first) and says what to try."
                color: "#F2B632"
                font.family: Theme.labelFont
                font.pixelSize: 12
                font.bold: true
            }
            Repeater {
                model: Konnekt.lessons
                PanelButton {
                    required property var modelData
                    width: 310
                    height: 28
                    dark: true
                    faceText: (modelData.index + 1) + " · " + modelData.title
                    text: "Lesson " + (modelData.index + 1) + ": " + modelData.title
                    onClicked: { Konnekt.loadLesson(modelData.index); lessonsPopup.close() }
                }
            }
        }
    }
    Rectangle {
        id: lessonPanel
        readonly property bool isPreset: Konnekt.preset >= 0
        readonly property int index: isPreset ? Konnekt.preset : Konnekt.lesson
        readonly property int count: isPreset ? Konnekt.presets.length : Konnekt.lessons.length
        function go(i) { if (isPreset) Konnekt.loadPreset(i); else Konnekt.loadLesson(i) }
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
            text: (lessonPanel.isPreset ? "PRESET " : "LESSON ") + Konnekt.sheetTitle
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
            PanelButton { width: 28; height: 24; dark: true; faceText: "✕"; text: "Close the text (the patch stays)"; onClicked: Konnekt.closeSheet() }
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
                text: Konnekt.sheetText
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
        anchors.right: parent.right
        anchors.top: topBar.bottom
        anchors.bottom: helpBar.top
        z: 35
        onShowScope: (s, j) => Konnekt.scope(s, j)
    }

    ManualView {
        id: manual
        title: "Konnekt"
        source: "qrc:/qt/qml/Konnekt/docs/MANUAL.md"
    }

    Shortcut {
        sequence: "Esc"
        onActivated: {
            root.pending = null
            Konnekt.learning = false
            explainPanel.close()
        }
    }
}

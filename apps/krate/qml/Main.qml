pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Krate

// The Krate rack window: transport bar, instrument tabs (click = focus), the
// Mixer, Song and Seq tabs; below them the focused instrument's panel scaled
// to fit, the mixer screen, the song screen or the sequencer.
Window {
    id: root

    width: 1400
    height: 980
    minimumWidth: 900
    minimumHeight: 640
    visible: true
    title: "krate"
    color: Theme.desk

    Shortcut {
        sequence: "Space"
        onActivated: Rack.playing = !Rack.playing
    }
    Shortcut {
        sequence: "Tab"
        onActivated: Rack.focus = Rack.stepFocus(1)
    }
    Shortcut {
        sequence: "M"
        onActivated: Rack.mixerShown = !Rack.mixerShown
    }
    Shortcut {
        sequence: "S"
        onActivated: Rack.songShown = !Rack.songShown
    }
    Shortcut {
        sequence: "Q"
        onActivated: Rack.seqShown = !Rack.seqShown
    }
    Shortcut {
        sequence: "A"
        onActivated: Rack.arrangeShown = !Rack.arrangeShown
    }
    Shortcut {
        sequence: "O"
        onActivated: Rack.soundsShown = !Rack.soundsShown
    }
    Shortcut {
        sequence: "Ctrl+,"
        onActivated: Rack.settingsShown = !Rack.settingsShown
    }
    Shortcut {
        sequence: "L"
        onActivated: MidiLearn.active = !MidiLearn.active
    }
    Shortcut {
        sequence: "Escape"
        enabled: MidiLearn.active
        onActivated: MidiLearn.active = false
    }

    TransportBar {
        id: transportBar
        width: root.width
    }

    Row { // instrument tabs
        id: tabs
        y: transportBar.height + 10
        x: 16
        spacing: 8
        // Tabs fit the window: the screens' tabs always show (narrower on a
        // narrower window), the instruments' share the rest, and scroll when
        // they don't fit (the focused one kept in view).
        readonly property int screenTabWidth: root.width >= 1500 ? 104 : root.width >= 1200 ? 92 : 80
        // Only the instruments in the rack have tabs (docs/DAW.md, D1).
        readonly property int instrumentTabs: Math.max(1, Rack.panelComponents.filter((c, i) => c !== "" && Rack.present[i]).length)
        // Beside the instruments: ◂ ▸ +, a gap, the five screen tabs and Settings, and the spaces between them all.
        readonly property real stripWidth: Math.max(80, root.width - 32 - 4 * 40 - 16 - 5 * screenTabWidth - 10 * spacing)
        // Wide enough to read "Kalliope 2"; more tabs than fit scroll.
        readonly property real tabWidth: Math.max(118, Math.min(150, (stripWidth - (instrumentTabs - 1) * spacing) / instrumentTabs))

        PanelButton { // focus previous (learnable)
            width: 40
            height: 40
            faceText: "◂"
            text: "Focus previous instrument"
            onClicked: Rack.focus = Rack.stepFocus(-1)
            LearnOverlay { anchors.fill: parent; paramKey: "rack:previous"; round: false }
        }

        Flickable { // the instruments' tabs: scrolled when they don't all fit
            id: tabStrip
            width: tabs.stripWidth
            height: 40
            contentWidth: tabRow.width
            contentHeight: 40
            clip: true
            flickableDirection: Flickable.HorizontalFlick
            boundsBehavior: Flickable.StopAtBounds
            // The focused instrument's tab in view, whenever it or the tabs change.
            property Item focusedTab: null
            function reveal() {
                let x = contentX
                if (focusedTab && focusedTab.visible) {
                    if (focusedTab.x < x)
                        x = focusedTab.x
                    else if (focusedTab.x + focusedTab.width > x + width)
                        x = focusedTab.x + focusedTab.width - width
                }
                contentX = Math.max(0, Math.min(x, contentWidth - width))
            }
            onFocusedTabChanged: Qt.callLater(reveal)
            onWidthChanged: Qt.callLater(reveal)
            onContentWidthChanged: Qt.callLater(reveal)

            Row {
                id: tabRow
                spacing: tabs.spacing
                Repeater {
                    model: Rack.deviceNames

                    Rectangle {
                        id: tab
                        required property int index
                        required property string modelData
                        readonly property bool focused: Rack.focus === index
                        readonly property bool shown: focused && !Rack.mixerShown && !Rack.songShown && !Rack.seqShown && !Rack.arrangeShown && !Rack.soundsShown && !Rack.settingsShown
                        onFocusedChanged: if (focused) tabStrip.focusedTab = tab
                        onXChanged: if (focused) Qt.callLater(tabStrip.reveal)
                        Component.onCompleted: if (focused) tabStrip.focusedTab = tab

                        // Devices without a panel (the backing track), or not in the rack, have no tab.
                        visible: Rack.panelComponents[index] !== "" && Rack.present[index] === true
                        width: tabs.tabWidth
                        clip: true
                        height: 40
                        radius: 4
                        color: shown ? Theme.silverLight : Theme.lowerDeck
                        border.color: shown ? Theme.accentUi : Theme.deckLine
                        border.width: shown ? 3 : 1

                        Led {
                            x: 12
                            anchors.verticalCenter: parent.verticalCenter
                            on: tab.focused
                        }
                        Text {
                            x: 34
                            width: tab.width - x - 6
                            height: tab.height
                            verticalAlignment: Text.AlignVCenter
                            text: tab.modelData
                            color: Theme.ink
                            font.family: Theme.labelFont
                            font.pixelSize: 18
                            font.bold: tab.focused
                            fontSizeMode: Text.HorizontalFit // "Korrode 2" whole: smaller, not cut
                            minimumPixelSize: 12
                            elide: Text.ElideMiddle
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                Rack.focus = tab.index
                                Rack.mixerShown = false
                            }
                        }
                        LearnOverlay { // a controller button can focus this instrument
                            anchors.fill: parent
                            paramKey: "rack:focus:" + tab.index
                            round: false
                        }
                        Accessible.role: Accessible.PageTab
                        Accessible.name: modelData + (focused ? " (focused)" : "")
                    }
                }
            }
        }

        PanelButton { // focus next (learnable)
            width: 40
            height: 40
            faceText: "▸"
            text: "Focus next instrument"
            onClicked: Rack.focus = Rack.stepFocus(1)
            LearnOverlay { anchors.fill: parent; paramKey: "rack:next"; round: false }
        }

        PanelButton { // add or remove instruments
            width: 40
            height: 40
            faceText: "+"
            text: "Add or remove instruments"
            onClicked: instrumentsPopup.open()
        }

        Item { width: 16; height: 1 }

        Rectangle { // the mixer screen (learnable, M)
            id: mixerTab
            width: tabs.screenTabWidth
            height: 40
            radius: 4
            color: Rack.mixerShown ? Theme.silverLight : Theme.lowerDeck
            border.color: Rack.mixerShown ? Theme.accentUi : Theme.deckLine
            border.width: Rack.mixerShown ? 3 : 1

            Led {
                x: 12
                anchors.verticalCenter: parent.verticalCenter
                visible: tabs.screenTabWidth >= 90 // narrow: the label alone, centred
                on: Rack.mixerShown
            }
            Text {
                x: tabs.screenTabWidth < 90 ? (parent.width - width) / 2 : tabs.screenTabWidth < 100 ? 30 : 34
                anchors.verticalCenter: parent.verticalCenter
                text: "Mixer"
                color: Theme.ink
                font.family: Theme.labelFont
                font.pixelSize: tabs.screenTabWidth < 90 ? 14 : tabs.screenTabWidth < 100 ? 15 : 18
                font.bold: Rack.mixerShown
            }
            MouseArea {
                anchors.fill: parent
                onClicked: Rack.mixerShown = !Rack.mixerShown
            }
            LearnOverlay {
                anchors.fill: parent
                paramKey: "rack:mixer"
                round: false
            }
            Accessible.role: Accessible.PageTab
            Accessible.name: "Mixer" + (Rack.mixerShown ? " (shown)" : "")
        }

        Rectangle { // the song screen (learnable, S)
            id: songTab
            width: tabs.screenTabWidth
            height: 40
            radius: 4
            color: Rack.songShown ? Theme.silverLight : Theme.lowerDeck
            border.color: Rack.songShown ? Theme.accentUi : Theme.deckLine
            border.width: Rack.songShown ? 3 : 1

            Led {
                x: 12
                anchors.verticalCenter: parent.verticalCenter
                visible: tabs.screenTabWidth >= 90 // narrow: the label alone, centred
                on: Rack.songShown
            }
            Text {
                x: tabs.screenTabWidth < 90 ? (parent.width - width) / 2 : tabs.screenTabWidth < 100 ? 30 : 34
                anchors.verticalCenter: parent.verticalCenter
                text: "Song"
                color: Theme.ink
                font.family: Theme.labelFont
                font.pixelSize: tabs.screenTabWidth < 90 ? 14 : tabs.screenTabWidth < 100 ? 15 : 18
                font.bold: Rack.songShown
            }
            MouseArea {
                anchors.fill: parent
                onClicked: Rack.songShown = !Rack.songShown
            }
            LearnOverlay {
                anchors.fill: parent
                paramKey: "rack:song"
                round: false
            }
            Accessible.role: Accessible.PageTab
            Accessible.name: "Song" + (Rack.songShown ? " (shown)" : "")
        }

        Rectangle { // the sequencer (learnable, Q)
            id: seqTab
            width: tabs.screenTabWidth
            height: 40
            radius: 4
            color: Rack.seqShown ? Theme.silverLight : Theme.lowerDeck
            border.color: Rack.seqShown ? Theme.accentUi : Theme.deckLine
            border.width: Rack.seqShown ? 3 : 1

            Led {
                x: 12
                anchors.verticalCenter: parent.verticalCenter
                visible: tabs.screenTabWidth >= 90 // narrow: the label alone, centred
                on: Rack.seqShown
            }
            Text {
                x: tabs.screenTabWidth < 90 ? (parent.width - width) / 2 : tabs.screenTabWidth < 100 ? 30 : 34
                anchors.verticalCenter: parent.verticalCenter
                text: "Seq"
                color: Theme.ink
                font.family: Theme.labelFont
                font.pixelSize: tabs.screenTabWidth < 90 ? 14 : tabs.screenTabWidth < 100 ? 15 : 18
                font.bold: Rack.seqShown
            }
            MouseArea {
                anchors.fill: parent
                onClicked: Rack.seqShown = !Rack.seqShown
            }
            LearnOverlay {
                anchors.fill: parent
                paramKey: "rack:seq"
                round: false
            }
            Accessible.role: Accessible.PageTab
            Accessible.name: "Sequencer" + (Rack.seqShown ? " (shown)" : "")
        }

        Rectangle { // the arranger (learnable, A)
            id: arrangeTab
            width: tabs.screenTabWidth
            height: 40
            radius: 4
            color: Rack.arrangeShown ? Theme.silverLight : Theme.lowerDeck
            border.color: Rack.arrangeShown ? Theme.accentUi : Theme.deckLine
            border.width: Rack.arrangeShown ? 3 : 1

            Led {
                x: 12
                anchors.verticalCenter: parent.verticalCenter
                visible: tabs.screenTabWidth >= 90 // narrow: the label alone, centred
                on: Rack.arrangeShown
            }
            Text {
                x: tabs.screenTabWidth < 90 ? (parent.width - width) / 2 : tabs.screenTabWidth < 100 ? 30 : 34
                anchors.verticalCenter: parent.verticalCenter
                text: "Arrange"
                color: Theme.ink
                font.family: Theme.labelFont
                font.pixelSize: tabs.screenTabWidth < 90 ? 14 : tabs.screenTabWidth < 100 ? 15 : 18
                font.bold: Rack.arrangeShown
            }
            MouseArea {
                anchors.fill: parent
                onClicked: Rack.arrangeShown = !Rack.arrangeShown
            }
            LearnOverlay {
                anchors.fill: parent
                paramKey: "rack:arrange"
                round: false
            }
            Accessible.role: Accessible.PageTab
            Accessible.name: "Arranger" + (Rack.arrangeShown ? " (shown)" : "")
        }

        Rectangle { // the sound library (learnable, O)
            width: tabs.screenTabWidth
            height: 40
            radius: 4
            color: Rack.soundsShown ? Theme.silverLight : Theme.lowerDeck
            border.color: Rack.soundsShown ? Theme.accentUi : Theme.deckLine
            border.width: Rack.soundsShown ? 3 : 1

            Led {
                x: 12
                anchors.verticalCenter: parent.verticalCenter
                visible: tabs.screenTabWidth >= 90 // narrow: the label alone, centred
                on: Rack.soundsShown
            }
            Text {
                x: tabs.screenTabWidth < 90 ? (parent.width - width) / 2 : tabs.screenTabWidth < 100 ? 30 : 34
                anchors.verticalCenter: parent.verticalCenter
                text: "Sounds"
                color: Theme.ink
                font.family: Theme.labelFont
                font.pixelSize: tabs.screenTabWidth < 90 ? 14 : tabs.screenTabWidth < 100 ? 15 : 18
                font.bold: Rack.soundsShown
            }
            MouseArea {
                anchors.fill: parent
                onClicked: Rack.soundsShown = !Rack.soundsShown
            }
            LearnOverlay {
                anchors.fill: parent
                paramKey: "rack:sounds"
                round: false
            }
            Accessible.role: Accessible.PageTab
            Accessible.name: "Sound library" + (Rack.soundsShown ? " (shown)" : "")
        }

        Rectangle { // Settings (learnable, Ctrl+,): folders, audio, MIDI, updates, crash reports
            width: 40
            height: 40
            radius: 4
            color: Rack.settingsShown ? Theme.silverLight : Theme.lowerDeck
            border.color: Rack.settingsShown ? Theme.accentUi : Theme.deckLine
            border.width: Rack.settingsShown ? 3 : 1
            Text {
                anchors.centerIn: parent
                text: "⚙"
                color: Theme.ink
                font.pixelSize: 24
            }
            Rectangle { // something waits: a restart, or an update
                x: parent.width - 12
                y: 4
                width: 8
                height: 8
                radius: 4
                visible: Settings.restartNeeded || Updates.available
                color: Theme.ledOn
            }
            MouseArea {
                anchors.fill: parent
                onClicked: Rack.settingsShown = !Rack.settingsShown
            }
            LearnOverlay {
                anchors.fill: parent
                paramKey: "rack:settings"
                round: false
            }
            Accessible.role: Accessible.PageTab
            Accessible.name: "Settings" + (Rack.settingsShown ? " (shown)" : "")
        }
    }

    Item { // the focused instrument's panel, scaled to fit
        id: panelArea
        x: 16
        y: tabs.y + tabs.height + 10
        width: root.width - 32
        height: root.height - y - 16
        visible: !Rack.mixerShown && !Rack.songShown && !Rack.seqShown && !Rack.arrangeShown && !Rack.soundsShown && !Rack.settingsShown

        Repeater {
            model: Rack.deviceCount

            Item {
                id: slot
                required property int index
                anchors.fill: parent
                visible: Rack.focus === index

                Loader {
                    id: loader
                    // Only an instrument in the rack has its panel made, with its own controllers.
                    readonly property bool wanted: Rack.panelComponents[slot.index] !== "" && Rack.present[slot.index] === true
                    onWantedChanged: load()
                    Component.onCompleted: load()
                    function load() {
                        if (!wanted)
                            source = ""
                        else if (!item)
                            setSource(Qt.createComponent(Rack.panelModules[slot.index], Rack.panelComponents[slot.index]).url,
                                      Rack.panelProperties(slot.index))
                    }
                    readonly property Item panel: item as Item
                    // Fit, but don't blow small panels up past 1.25x.
                    readonly property real s: panel ? Math.min(slot.width / panel.width, slot.height / panel.height, 1.25) : 1
                    x: panel ? (slot.width - panel.width * s) / 2 : 0
                    y: panel ? (slot.height - panel.height * s) / 2 : 0
                    transform: Scale { xScale: loader.s; yScale: loader.s }
                    onLoaded: {
                        // A panel shared by several devices (the audio tracks) is told which it is.
                        if (item && item.hasOwnProperty("rackDevice"))
                            item.rackDevice = slot.index
                        // One that lays itself out (Konnekt) gets the whole area, unscaled.
                        if (item && item.hasOwnProperty("fillsSlot")) {
                            item.width = Qt.binding(() => slot.width)
                            item.height = Qt.binding(() => slot.height)
                        }
                    }
                }
            }
        }
    }

    // The last session crashed (or never ended): say so, once (docs/CRASHES.md).
    Rectangle {
        id: crashBanner
        x: 16
        y: panelArea.y
        z: 50
        width: root.width - 32
        height: 46
        visible: Crashes.newReport !== ""
        radius: 6
        color: "#5A1E1A"
        border.color: Theme.ledOn
        Text {
            x: 14
            width: parent.width - x - 2 * 110 - 30
            anchors.verticalCenter: parent.verticalCenter
            text: "Krate started again after the last session went wrong. " + Crashes.newSummary
            elide: Text.ElideRight
            color: "#F2E8E6"
            font.family: Theme.labelFont
            font.pixelSize: 14
            font.bold: true
        }
        Row {
            anchors.right: parent.right
            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8
            PanelButton {
                width: 100
                height: 30
                faceText: "VIEW"
                text: "Read the crash report"
                onClicked: Crashes.viewing = true
            }
            PanelButton {
                width: 100
                height: 30
                faceText: "DISMISS"
                text: "Hide this (the report is kept: Settings, then Crash reports)"
                onClicked: Crashes.dismiss()
            }
        }
    }

    CrashReportsPopup {
        x: 24
        y: 24
        width: root.width - 48
        height: root.height - 48
    }

    Text { // an empty rack
        anchors.centerIn: panelArea
        visible: panelArea.visible && !Rack.present.some((p, i) => p && Rack.panelComponents[i] !== "")
        text: "No instruments in the rack: press + (next to the tabs) to add one."
        color: Theme.inkInverse
        font.family: Theme.labelFont
        font.pixelSize: 20
    }

    Item { // the mixer screen: the strips, taller and (room permitting) larger
        id: mixerArea
        x: 16
        y: panelArea.y
        width: root.width - 32
        height: panelArea.height
        visible: Rack.mixerShown

        // Grow up to 1.5x while the strips fit across; the strips fill the height.
        // More strips than fit scroll sideways (the master stays at the right).
        readonly property real s: Math.max(1, Math.min(1.5, width / (strips.implicitWidth + 132 + 40)))
        readonly property real masterWidth: (10 + 2 + 10 + 110) * s
        readonly property bool scrolling: strips.implicitWidth * s > width - masterWidth
        readonly property real stripHeight: Math.min((height - (scrolling ? 16 : 0)) / s, 600)

        Flickable {
            id: mixerScroll
            readonly property real stripsWidth: strips.implicitWidth * mixerArea.s
            width: Math.min(stripsWidth, mixerArea.width - mixerArea.masterWidth)
            x: (mixerArea.width - width - mixerArea.masterWidth) / 2
            y: (mixerArea.height - height) / 2
            height: mixerArea.stripHeight * mixerArea.s + (mixerArea.scrolling ? 16 : 0)
            contentWidth: stripsWidth
            contentHeight: height
            flickableDirection: Flickable.HorizontalFlick
            boundsBehavior: Flickable.StopAtBounds
            clip: true
            ScrollBar.horizontal: ScrollBar { policy: mixerArea.scrolling ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff }

            Row {
                id: strips
                spacing: 10
                transform: Scale { xScale: mixerArea.s; yScale: mixerArea.s }

                Repeater {
                    model: Rack.deviceNames

                    Loader { // a strip only for a device in the rack
                        id: stripSlot
                        required property int index
                        required property string modelData
                        active: Rack.present[index] === true
                        visible: active
                        sourceComponent: MixerStrip {
                            device: stripSlot.index
                            name: stripSlot.modelData
                            height: mixerArea.stripHeight
                        }
                    }
                }
            }
        }

        Row {
            x: mixerScroll.x + mixerScroll.width + 10 * mixerArea.s
            y: mixerScroll.y
            spacing: 10
            transform: Scale { xScale: mixerArea.s; yScale: mixerArea.s }
            Rectangle { width: 2; height: mixerArea.stripHeight; color: Theme.deckLine }
            MixerStrip {
                device: -1
                name: "Master"
                height: mixerArea.stripHeight
            }
        }
    }

    SongScreen {
        x: 16
        y: panelArea.y
        width: root.width - 32
        height: panelArea.height
        visible: Rack.songShown
    }

    SeqScreen {
        x: 16
        y: panelArea.y
        width: root.width - 32
        height: panelArea.height
        visible: Rack.seqShown
    }

    ArrangeScreen {
        x: 16
        y: panelArea.y
        width: root.width - 32
        height: panelArea.height
        visible: Rack.arrangeShown
    }

    SoundsScreen {
        x: 16
        y: panelArea.y
        width: root.width - 32
        height: panelArea.height
        visible: Rack.soundsShown
    }

    SettingsScreen {
        x: 16
        y: panelArea.y
        width: root.width - 32
        height: panelArea.height
        visible: Rack.settingsShown
    }

    // ---- Instruments in the rack: add or remove (docs/DAW.md, D1)
    Popup {
        id: instrumentsPopup
        x: (root.width - width) / 2
        y: tabs.y + tabs.height + 6
        width: Math.min(root.width - 32, 780)
        padding: 16
        modal: true
        background: Rectangle { radius: 8; color: Theme.lowerDeck; border.color: Theme.deckLine; border.width: 2 }

        readonly property var about: ({
            korrode: "Acid bass: one voice, a squelching filter, a 16-step sequencer.",
            kessel: "Drum machine: synthesized drums and a sample voice, a step sequencer.",
            kalliope: "Polysynth: two oscillators, a ladder filter, an arpeggiator.",
            krimson: "Four sounds at once: leads, basses, pads, each on its own channel.",
            kobalt: "Monosynth: one oscillator, a sub, portamento, an arpeggiator.",
            klang: "Sampler: your WAV and MP3 files on pads, slices and patterns.",
            konnekt: "Modular synth: modules wired with patch cables, lessons that build a synth up.",
            krunch: "Guitar amp: your guitar through amp channels, cabinets and pedals.",
            klick: "Metronome: a click on the rack's tempo."
        })

        Column {
            width: parent.width
            spacing: 8
            Text {
                width: parent.width
                wrapMode: Text.WordWrap
                text: "INSTRUMENTS IN THE RACK: up to 16 of each, numbered (2 and on are copies, each with its own sound). Lit: in the rack. Removing one keeps its sound, mixer settings and MIDI links for when you add it back; it isn't played or shown meanwhile."
                color: Theme.ink
                font.family: Theme.labelFont
                font.pixelSize: 13
                font.bold: true
            }
            // Each instrument, and its copies (docs/DAW.md#copies): 1 is the
            // instrument itself, 2.. "Korrode 2" and on. Lit: in the rack.
            Repeater {
                model: Rack.deviceKinds.filter((kind, i) => Rack.deviceCopies[i] === 1 && Rack.removable(i)
                                                            && !kind.startsWith("audio") && !kind.startsWith("plugin"))
                Row {
                    id: kindRow
                    required property string modelData
                    readonly property var devices: Rack.deviceKinds.map((kind, i) => kind === modelData ? i : -1).filter(i => i >= 0)
                    spacing: 12
                    Column {
                        width: instrumentsPopup.availableWidth - copyButtons.width - 12
                        anchors.verticalCenter: parent.verticalCenter
                        Text {
                            text: Rack.deviceNames[kindRow.devices[0]]
                            color: Theme.ink
                            font.family: Theme.labelFont
                            font.pixelSize: 16
                            font.bold: kindRow.devices.some(d => Rack.present[d] === true)
                        }
                        Text {
                            width: parent.width
                            elide: Text.ElideRight
                            text: instrumentsPopup.about[kindRow.modelData] ?? ""
                            color: Theme.ink
                            opacity: 0.7
                            font.family: Theme.labelFont
                            font.pixelSize: 12
                        }
                    }
                    Flow {
                        id: copyButtons
                        width: 8 * 34
                        spacing: 4
                        anchors.verticalCenter: parent.verticalCenter
                        Repeater {
                            model: kindRow.devices
                            PanelButton {
                                id: copyButton
                                required property int modelData
                                readonly property bool inRack: Rack.present[modelData] === true
                                width: 30
                                height: 28
                                faceText: Rack.deviceCopies[modelData]
                                text: (inRack ? "Remove " : "Add ") + Rack.deviceNames[modelData] + (inRack ? " from the rack" : " to the rack")
                                tint: inRack ? Theme.labelOrange : "transparent"
                                onClicked: Rack.setPresent(modelData, !inRack)
                            }
                        }
                    }
                }
            }
            Text {
                topPadding: 8
                width: parent.width
                wrapMode: Text.WordWrap
                text: "AUDIO TRACKS. Each records your interface's inputs (or another instrument) and plays the take. Lit: in the rack."
                color: Theme.ink
                font.family: Theme.labelFont
                font.pixelSize: 13
                font.bold: true
            }
            Flow {
                width: parent.width
                spacing: 6
                Repeater {
                    model: Rack.deviceIds.map((id, i) => id.startsWith("audio") ? i : -1).filter(i => i >= 0)
                    PanelButton {
                        id: trackButton
                        required property int modelData
                        readonly property bool inRack: Rack.present[modelData] === true
                        width: 40
                        height: 36
                        faceText: Rack.deviceNames[modelData].replace("Audio ", "")
                        text: (inRack ? "Remove " : "Add ") + Rack.deviceNames[modelData] + (inRack ? " from the rack" : " to the rack")
                        tint: inRack ? Theme.labelOrange : "transparent"
                        onClicked: Rack.setPresent(modelData, !inRack)
                    }
                }
            }
            Text {
                topPadding: 8
                width: parent.width
                wrapMode: Text.WordWrap
                text: "PLUGIN SLOTS. Each holds a CLAP plugin you pick on its panel: an instrument, or an effect fed by another device. Lit: in the rack."
                color: Theme.ink
                font.family: Theme.labelFont
                font.pixelSize: 13
                font.bold: true
            }
            Flow {
                width: parent.width
                spacing: 6
                Repeater {
                    model: Rack.deviceIds.map((id, i) => id.startsWith("plugin") ? i : -1).filter(i => i >= 0)
                    PanelButton {
                        id: slotButton
                        required property int modelData
                        readonly property bool inRack: Rack.present[modelData] === true
                        width: 40
                        height: 36
                        faceText: Rack.deviceNames[modelData].replace("Plugin ", "")
                        text: (inRack ? "Remove " : "Add ") + Rack.deviceNames[modelData] + (inRack ? " from the rack" : " to the rack")
                        tint: inRack ? Theme.labelOrange : "transparent"
                        onClicked: Rack.setPresent(modelData, !inRack)
                    }
                }
            }
        }
    }
}

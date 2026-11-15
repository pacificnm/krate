pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Krate

// The Krunch app's window (apps/krunch): a top bar with what the Krate rack
// gives Krunch around its panel (the audio status, MIDI LEARN, AUDIO: the
// interface's inputs, the outputs and the buffer; UPDATES: Krate's releases,
// and crash reports), and under it the amp at its design size, scaled to fit.
Window {
    id: root
    required property Krunch krunch
    required property Presets presets
    required property Cabinets cabinets
    required property Captures captures
    required property Pedals pedals
    required property Tuner tuner

    width: 1280
    height: 960
    minimumWidth: 900
    minimumHeight: 680
    visible: true
    title: "Krunch"
    color: "#0B0B0C"

    readonly property color cream: "#E9DFC4"
    readonly property real panelScale: Math.min(width / panel.width, (height - bar.height) / panel.height)

    Shortcut {
        sequence: "L"
        onActivated: MidiLearn.active = !MidiLearn.active
    }
    Shortcut {
        sequence: "Escape"
        enabled: MidiLearn.active
        onActivated: MidiLearn.active = false
    }

    component BarButton: Column {
        id: barButton
        property alias faceText: button.faceText
        property alias text: button.text
        property alias tint: button.tint
        property alias hasLed: button.hasLed
        property alias ledOn: button.ledOn
        property string label: ""
        signal clicked()
        spacing: 2
        PanelButton {
            id: button
            width: 64
            height: 30
            dark: true
            onClicked: barButton.clicked()
        }
        Text {
            width: button.width
            horizontalAlignment: Text.AlignHCenter
            text: barButton.label
            color: "#8F897A"
            font.family: Theme.labelFont
            font.pixelSize: 10
            font.bold: true
        }
    }

    // --- The top bar ------------------------------------------------------------
    Rectangle {
        id: bar
        width: root.width
        height: 56
        color: "#141415"
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: "#2E2F32" }

        Row { // the audio: connected (a click reconnects), its status
            x: 14
            anchors.verticalCenter: parent.verticalCenter
            spacing: 10
            PanelButton {
                anchors.verticalCenter: parent.verticalCenter
                width: 44
                height: 22
                dark: true
                text: "Audio: reconnect"
                hasLed: true
                ledOn: Host.audioConnected
                onClicked: Host.connectAudio()
            }
            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2
                Text {
                    text: Host.audioStatus
                    color: Host.audioConnected ? root.cream : Theme.ledOn
                    font.family: Theme.labelFont
                    font.pixelSize: 12
                    font.bold: true
                }
                Text {
                    visible: Host.audioConnected
                    text: Host.midiConnected ? "MIDI controller connected" : "No MIDI controller"
                    color: "#8F897A"
                    font.family: Theme.labelFont
                    font.pixelSize: 11
                }
            }
        }

        Text { // a newer Krate (Krunch comes in its package): UPDATES has the button
            anchors.centerIn: parent
            visible: Updates.available
            text: "Krate " + Updates.latestVersion + " is out: UPDATES ▸"
            color: Theme.labelOrange
            font.family: Theme.labelFont
            font.pixelSize: 14
            font.bold: true
            MouseArea {
                anchors.fill: parent
                anchors.margins: -6
                cursorShape: Qt.PointingHandCursor
                onClicked: updatesPopup.open()
            }
        }

        Row {
            anchors.right: parent.right
            anchors.rightMargin: 14
            anchors.verticalCenter: parent.verticalCenter
            spacing: 10
            BarButton {
                faceText: "LEARN"
                label: "MIDI"
                text: "MIDI Learn: click a control, then move a knob or press a button on your controller (L)"
                tint: MidiLearn.active ? Theme.accentUi : "transparent"
                onClicked: MidiLearn.active = !MidiLearn.active
            }
            BarButton {
                faceText: "AUDIO"
                label: "IN / OUT"
                text: "Audio: which input of your audio interface the guitar is on, where Krunch plays to, and the buffer size"
                tint: audioPopup.opened ? Theme.labelOrange : "transparent"
                onClicked: audioPopup.opened ? audioPopup.close() : audioPopup.open()
            }
            BarButton {
                faceText: "UPDATES"
                label: "v" + Updates.currentVersion
                text: Updates.available ? "Krate " + Updates.latestVersion + " is out: update Krunch"
                                        : "Updates and crash reports"
                tint: Updates.available ? Theme.labelOrange : updatesPopup.opened ? "#3B4A5A" : "transparent"
                onClicked: updatesPopup.opened ? updatesPopup.close() : updatesPopup.open()
            }
        }
    }

    // --- The amp ----------------------------------------------------------------
    Panel {
        id: panel
        krunch: root.krunch
        presets: root.presets
        cabinets: root.cabinets
        captures: root.captures
        pedals: root.pedals
        tuner: root.tuner
        focus: true
        x: (root.width - width * root.panelScale) / 2
        y: bar.height + (root.height - bar.height - height * root.panelScale) / 2
        transform: Scale {
            xScale: root.panelScale
            yScale: root.panelScale
        }
    }

    Rectangle { // MIDI Learn is on: say how
        anchors.horizontalCenter: parent.horizontalCenter
        y: bar.height + 8
        z: 60
        width: learnText.implicitWidth + 32
        height: 34
        radius: 6
        visible: MidiLearn.active
        color: "#1A2E4D"
        border.color: Theme.accentUi
        Text {
            id: learnText
            anchors.centerIn: parent
            text: MidiLearn.armedParam === "" ? "MIDI LEARN: click a knob, switch or footswitch, then move or press it on your controller (right-click clears; Esc ends)"
                                              : "Now move a knob or press a button on your controller…"
            color: "#DDE8FF"
            font.family: Theme.labelFont
            font.pixelSize: 13
            font.bold: true
        }
    }

    // The last session crashed (or never ended): say so, once (docs/CRASHES.md).
    Rectangle {
        x: 16
        y: bar.height + 8
        z: 50
        width: root.width - 32
        height: 46
        visible: Crashes.newReport !== "" && !MidiLearn.active
        radius: 6
        color: "#5A1E1A"
        border.color: Theme.ledOn
        Text {
            x: 14
            width: parent.width - x - 2 * 110 - 30
            anchors.verticalCenter: parent.verticalCenter
            text: "Krunch started again after the last session went wrong. " + Crashes.newSummary
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
                text: "Hide this (the report is kept: UPDATES, then CRASH REPORTS)"
                onClicked: Crashes.dismiss()
            }
        }
    }

    // --- AUDIO: inputs, outputs, buffer ------------------------------------------
    Popup {
        id: audioPopup
        x: root.width - width - 14
        y: bar.height + 4
        width: 560
        padding: 14
        background: Rectangle { radius: 6; color: Theme.silverLight; border.color: Theme.deckLine }

        AudioSettings {
            width: parent.width
        }
    }

    // --- UPDATES: Krate's releases (Krunch comes in its package), crash reports ---
    Popup {
        id: updatesPopup
        x: root.width - width - 14
        y: bar.height + 4
        width: 560
        padding: 16
        background: Rectangle { radius: 6; color: Theme.silverLight; border.color: Theme.deckLine }

        component Note: Text {
            color: "#55575B"
            font.family: Theme.labelFont
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }

        Column {
            width: parent.width
            spacing: 12
            Text {
                text: "KRUNCH " + Updates.currentVersion
                color: Theme.ink
                font.family: Theme.labelFont
                font.pixelSize: 15
                font.bold: true
            }
            Note {
                width: parent.width
                text: "Krunch comes with Krate: one package, one version. New releases come from GitHub; installing one keeps your presets, captures, IRs and settings."
            }
            Row {
                spacing: 10
                PanelButton {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 44
                    height: 24
                    text: "Check by itself: when Krunch starts, then twice a day"
                    hasLed: true
                    ledOn: Updates.autoCheck
                    onClicked: Updates.autoCheck = !Updates.autoCheck
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Check by itself (when Krunch starts, then twice a day)"
                    color: Theme.ink
                    font.family: Theme.labelFont
                    font.pixelSize: 13
                }
            }
            Row {
                spacing: 10
                readonly property bool busy: Updates.state === "checking" || Updates.state === "installing"
                                             || Updates.state === "downloading"
                PanelButton {
                    width: 110
                    height: 32
                    enabled: !parent.busy
                    opacity: enabled ? 1 : 0.5
                    faceText: "CHECK NOW"
                    text: "Ask GitHub whether there's a newer Krate"
                    onClicked: Updates.check()
                }
                PanelButton {
                    width: 200
                    height: 32
                    visible: Updates.available && Updates.canInstall
                    enabled: !parent.busy
                    opacity: enabled ? 1 : 0.5
                    faceText: "UPDATE TO " + Updates.latestVersion
                    text: "Download, check and install Krate " + Updates.latestVersion + " (Krunch with it)"
                    onClicked: Updates.install()
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 220
                    text: Updates.message
                    color: Updates.state === "failed" ? Theme.ledOn : Theme.ink
                    font.family: Theme.labelFont
                    font.pixelSize: 13
                    wrapMode: Text.WordWrap
                }
            }
            Note {
                width: parent.width
                visible: !Updates.canInstall
                text: "Updates install with Krate's package (this Krunch runs from a build)."
            }
            Note {
                width: parent.width
                visible: Updates.state === "done"
                text: "Quit Krunch and start it again to play the new version."
                color: Theme.ink
                font.bold: true
            }
            Flickable { // what's new
                width: parent.width
                height: Math.min(contentHeight, 260)
                visible: Updates.available && Updates.notes !== ""
                contentHeight: notes.implicitHeight
                clip: true
                Text {
                    id: notes
                    width: parent.width
                    text: Updates.notes
                    textFormat: Text.MarkdownText
                    wrapMode: Text.WordWrap
                    color: Theme.ink
                    font.family: Theme.labelFont
                    font.pixelSize: 12
                }
            }
            Rectangle { width: parent.width; height: 1; color: Theme.deckLine }
            Row {
                spacing: 10
                PanelButton {
                    width: 140
                    height: 32
                    faceText: "CRASH REPORTS"
                    text: "Read what went wrong when Krunch crashed"
                    onClicked: {
                        updatesPopup.close()
                        Crashes.viewing = true
                    }
                }
                Note {
                    anchors.verticalCenter: parent.verticalCenter
                    text: Crashes.reports.length === 0 ? "None: Krunch hasn't crashed."
                                                       : Crashes.reports.length + " kept, the newest " + Crashes.reports[0].when
                }
            }
        }
    }

    CrashReportsPopup {
        appName: "Krunch"
        x: 24
        y: 24
        width: root.width - 48
        height: root.height - 48
    }
}

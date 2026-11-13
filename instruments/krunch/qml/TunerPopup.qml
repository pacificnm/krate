pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Krate

// The tuner: the note heard (the nearest string of the chosen tuning, or in
// Chromatic the nearest note), a needle from 50 cents flat to 50 sharp, the
// tuning's strings, the tuning, A4's pitch, and MUTE. Open while
// tuner.open (the TUNER button, or its learned footswitch).
Popup {
    id: popup
    required property Tuner tuner

    readonly property color cream: "#E9DFC4"
    readonly property color inTuneGreen: "#5FD35A"
    readonly property real inTuneCents: 3
    readonly property bool inTune: tuner.hasPitch && Math.abs(tuner.cents) <= inTuneCents

    width: 760
    padding: 18
    modal: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    visible: tuner.open
    onClosed: tuner.open = false

    background: Rectangle { radius: 8; color: "#18181A"; border.color: "#5A5B5E"; border.width: 2 }

    component Caption: Text {
        color: "#8F897A"
        font.family: Theme.labelFont
        font.pixelSize: 12
        font.bold: true
    }

    Column {
        width: parent.width
        spacing: 16

        // --- Title, MUTE, CLOSE --------------------------------------------------
        Item {
            width: parent.width
            height: 40
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "TUNER"
                color: popup.cream
                font.family: Theme.labelFont
                font.pixelSize: 22
                font.bold: true
            }
            Row {
                anchors.right: parent.right
                spacing: 8
                PanelButton {
                    width: 110
                    height: 40
                    dark: true
                    faceText: popup.tuner.mute ? "MUTED" : "MUTE OFF"
                    text: "Mute the amp while tuning"
                    tint: popup.tuner.mute ? Theme.labelOrange : "transparent" // (too low for a LED over the word)
                    onClicked: popup.tuner.mute = !popup.tuner.mute
                }
                PanelButton {
                    width: 90
                    height: 40
                    dark: true
                    faceText: "CLOSE"
                    text: "Close the tuner"
                    onClicked: popup.tuner.open = false
                }
            }
        }

        // --- The note ------------------------------------------------------------
        Item {
            width: parent.width
            height: 150
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                Text {
                    text: popup.tuner.hasPitch ? popup.tuner.note : "–"
                    color: !popup.tuner.hasPitch ? "#4A4B4E" : popup.inTune ? popup.inTuneGreen : popup.cream
                    font.family: Theme.labelFont
                    font.pixelSize: 130
                    font.bold: true
                }
                Text {
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 22
                    visible: popup.tuner.hasPitch
                    text: popup.tuner.octave
                    color: "#8F897A"
                    font.family: Theme.labelFont
                    font.pixelSize: 34
                    font.bold: true
                }
            }
            Text { // which way to turn
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                width: 200
                visible: popup.tuner.hasPitch && popup.tuner.cents < -popup.inTuneCents
                text: "FLAT\ntune up ▲"
                color: Theme.labelOrange
                font.family: Theme.labelFont
                font.pixelSize: 20
                font.bold: true
            }
            Text {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                width: 200
                horizontalAlignment: Text.AlignRight
                visible: popup.tuner.hasPitch && popup.tuner.cents > popup.inTuneCents
                text: "SHARP\ntune down ▼"
                color: Theme.labelOrange
                font.family: Theme.labelFont
                font.pixelSize: 20
                font.bold: true
            }
            Text {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                visible: popup.inTune
                text: "IN TUNE"
                color: popup.inTuneGreen
                font.family: Theme.labelFont
                font.pixelSize: 20
                font.bold: true
            }
        }

        // --- The needle: -50 .. +50 cents ------------------------------------------
        Item {
            id: scale
            width: parent.width
            height: 70
            readonly property real span: width - 40
            function xOf(cents) { return 20 + span * (Math.max(-50, Math.min(50, cents)) + 50) / 100 }

            Rectangle { // the in-tune zone
                x: scale.xOf(-popup.inTuneCents)
                y: 6
                width: scale.xOf(popup.inTuneCents) - x
                height: 40
                radius: 3
                color: popup.inTune ? "#2F6A2C" : "#22301F"
            }
            Repeater {
                model: 11
                Rectangle {
                    required property int index
                    readonly property int cents: -50 + index * 10
                    x: scale.xOf(cents) - width / 2
                    y: cents === 0 ? 0 : 16
                    width: cents === 0 ? 3 : 2
                    height: cents === 0 ? 52 : 20
                    color: cents === 0 ? popup.cream : "#5A5B5E"
                }
            }
            Repeater {
                model: [-50, -25, 0, 25, 50]
                Caption {
                    required property int modelData
                    x: scale.xOf(modelData) - 20
                    y: 54
                    width: 40
                    horizontalAlignment: Text.AlignHCenter
                    text: modelData > 0 ? "+" + modelData : modelData.toString()
                }
            }
            Caption { x: 0; y: 54; text: "♭"; font.pixelSize: 16 }
            Caption { anchors.right: parent.right; y: 54; text: "♯"; font.pixelSize: 16 }
            Rectangle { // the needle
                visible: popup.tuner.hasPitch
                x: scale.xOf(popup.tuner.cents) - width / 2
                y: 0
                width: 6
                height: 52
                radius: 3
                color: popup.inTune ? popup.inTuneGreen : Math.abs(popup.tuner.cents) > 50 ? "#E2412E" : Theme.labelOrange
                Behavior on x { NumberAnimation { duration: 60 } }
            }
        }
        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: popup.tuner.hasPitch
                  ? (popup.tuner.cents >= 0 ? "+" : "") + popup.tuner.cents.toFixed(1) + " cents   ·   " + popup.tuner.frequency.toFixed(2) + " Hz"
                  : "Play one string, open"
            color: popup.tuner.hasPitch ? popup.cream : "#8F897A"
            font.family: "monospace"
            font.pixelSize: 16
        }

        // --- The tuning's strings ----------------------------------------------------
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 10
            visible: popup.tuner.strings.length > 0
            Repeater {
                model: popup.tuner.strings
                Rectangle {
                    id: peg
                    required property var modelData
                    required property int index
                    readonly property bool playing: popup.tuner.hasPitch && popup.tuner.string === index
                    width: 70
                    height: 64
                    radius: 6
                    color: playing ? (popup.inTune ? "#2F6A2C" : "#4A3418") : "#232325"
                    border.color: playing ? (popup.inTune ? popup.inTuneGreen : Theme.labelOrange) : "#3A3B3E"
                    border.width: playing ? 2 : 1
                    Row {
                        anchors.centerIn: parent
                        anchors.verticalCenterOffset: -6
                        Text {
                            text: peg.modelData.name
                            color: peg.playing ? "#FFFFFF" : popup.cream
                            font.family: Theme.labelFont
                            font.pixelSize: 24
                            font.bold: true
                        }
                        Text {
                            anchors.bottom: parent.bottom
                            text: peg.modelData.octave
                            color: "#8F897A"
                            font.family: Theme.labelFont
                            font.pixelSize: 13
                            font.bold: true
                        }
                    }
                    Caption {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 4
                        text: peg.modelData.hz.toFixed(1)
                        font.pixelSize: 10
                        font.bold: false
                    }
                }
            }
        }

        // --- Tuning and A4 ------------------------------------------------------------
        Row {
            width: parent.width
            spacing: 24
            Column {
                spacing: 6
                Caption { text: "TUNING" }
                ComboBox {
                    id: tuningBox
                    width: 290
                    height: 40
                    model: popup.tuner.tunings
                    textRole: "name"
                    valueRole: "id"
                    currentIndex: indexOfValue(popup.tuner.tuning)
                    onActivated: popup.tuner.tuning = currentValue
                    Accessible.name: "Tuning"
                    delegate: ItemDelegate {
                        id: tuningItem
                        required property var modelData
                        required property int index
                        width: tuningBox.width
                        highlighted: tuningBox.highlightedIndex === index
                        contentItem: Item {
                            implicitHeight: 22
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: tuningItem.modelData.name
                                font.pixelSize: 14
                                font.bold: tuningItem.modelData.id === popup.tuner.tuning
                                color: tuningItem.highlighted ? "#FFFFFF" : "#E8E8E8"
                            }
                            Text {
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                text: tuningItem.modelData.group
                                font.pixelSize: 11
                                color: "#8F897A"
                            }
                        }
                        background: Rectangle { color: tuningItem.highlighted ? "#3B4A5A" : "#232325" }
                    }
                }
            }
            Column {
                spacing: 6
                Caption { text: "A4 REFERENCE" }
                Row {
                    spacing: 6
                    PanelButton {
                        width: 40
                        height: 40
                        dark: true
                        faceText: "−"
                        text: "A4 one hertz lower"
                        onClicked: popup.tuner.referenceHz = Math.round(popup.tuner.referenceHz) - 1
                    }
                    TextField {
                        id: referenceField
                        width: 90
                        height: 40
                        horizontalAlignment: Text.AlignHCenter
                        text: popup.tuner.referenceHz.toFixed(popup.tuner.referenceHz % 1 === 0 ? 0 : 1) + " Hz"
                        validator: RegularExpressionValidator { regularExpression: /\s*\d{3}([.,]\d)?\s*(Hz)?\s*/i }
                        onEditingFinished: {
                            const hz = parseFloat(text.replace(",", "."))
                            if (!isNaN(hz))
                                popup.tuner.referenceHz = hz
                            text = Qt.binding(() => popup.tuner.referenceHz.toFixed(popup.tuner.referenceHz % 1 === 0 ? 0 : 1) + " Hz")
                        }
                        onActiveFocusChanged: if (activeFocus) selectAll()
                        Accessible.name: "A4 reference pitch in hertz, " + popup.tuner.minReferenceHz + " to " + popup.tuner.maxReferenceHz
                    }
                    PanelButton {
                        width: 40
                        height: 40
                        dark: true
                        faceText: "+"
                        text: "A4 one hertz higher"
                        onClicked: popup.tuner.referenceHz = Math.round(popup.tuner.referenceHz) + 1
                    }
                    Repeater {
                        model: [432, 440, 442, 444]
                        PanelButton {
                            required property int modelData
                            width: 48
                            height: 40
                            dark: true
                            faceText: modelData.toString()
                            text: "A4 at " + modelData + " hertz"
                            tint: popup.tuner.referenceHz === modelData ? "#3B4A5A" : "transparent"
                            onClicked: popup.tuner.referenceHz = modelData
                        }
                    }
                }
            }
        }
        Caption {
            width: parent.width
            text: "Reads the guitar as it comes in, before the pedals" + (popup.tuner.mute ? "; the amp is silent while the tuner is open." : "; the amp plays on (MUTE is off).")
            font.bold: false
            wrapMode: Text.WordWrap
        }
    }
}

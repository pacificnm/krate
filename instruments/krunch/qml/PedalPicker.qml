pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Krate

// Adding a pedal: every kind, in groups (drives, dynamics and EQ, modulation,
// time and space, rigs), each shown as it looks on the board with a line on
// what it does and where it goes (FRONT or LOOP). A click adds it; typing
// narrows the list.
Popup {
    id: picker
    required property Pedals pedals

    readonly property color cream: "#E9DFC4"
    readonly property var groupOrder: ["Drive & boost", "Dynamics & EQ", "Modulation", "Time & space", "Rigs", "More"]
    readonly property real thumbScale: 0.62
    readonly property real pedalHeight: 238 // as on the board
    readonly property real slot: 164
    readonly property real tileWidth: Math.floor((groups.width - 4 * 12) / 5) // five to a row

    function matches(t) {
        const q = search.text.trim().toLowerCase()
        return q === "" || t.name.toLowerCase().includes(q) || t.blurb.toLowerCase().includes(q) || t.group.toLowerCase().includes(q)
    }
    function typesIn(group) {
        return picker.pedals.types.filter(t => t.group === group && picker.matches(t))
    }
    readonly property int shown: picker.pedals.types.filter(t => picker.matches(t)).length

    parent: Overlay.overlay
    x: Math.round((parent.width - width) / 2)
    y: Math.round((parent.height - height) / 2)
    width: Math.min(1180, parent.width - 40)
    height: Math.min(820, parent.height - 40)
    padding: 20
    modal: true
    focus: true
    onOpened: {
        search.text = ""
        search.forceActiveFocus()
        list.contentY = 0
    }
    background: Rectangle { radius: 10; color: "#1B1A19"; border.color: "#5A5650"; border.width: 2 }
    Overlay.modal: Rectangle { color: "#A0000000" }

    // --- Header: title, places left, search, close --------------------------------------
    Item {
        id: header
        width: parent.width
        height: 44
        Column {
            anchors.verticalCenter: parent.verticalCenter
            Text {
                text: "ADD A PEDAL"
                color: picker.cream
                font.family: Theme.labelFont
                font.pixelSize: 22
                font.bold: true
            }
            Text {
                readonly property int free: picker.pedals.maxPedals - picker.pedals.count
                text: free + (free === 1 ? " place" : " places") + " left on the board  ·  it goes at the end; drag it where you want it"
                color: "#8F897A"
                font.family: Theme.labelFont
                font.pixelSize: 12
            }
        }
        Row {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: 10
            TextField {
                id: search
                width: 260
                height: 40
                placeholderText: "Search: fuzz, echo, EQ…"
                color: "#F2F2F2"
                placeholderTextColor: "#7A766C"
                font.pixelSize: 15
                background: Rectangle { radius: 6; color: "#262422"; border.color: search.activeFocus ? "#C9A86A" : "#4A4640" }
                Keys.onReturnPressed: { // Enter: the only match, if there's one
                    const hits = picker.pedals.types.filter(t => picker.matches(t))
                    if (hits.length === 1) {
                        picker.pedals.add(hits[0].type)
                        picker.close()
                    }
                }
                Accessible.name: "Search the pedals"
            }
            PanelButton {
                width: 90
                height: 40
                dark: true
                faceText: "CLOSE"
                text: "Close without adding a pedal"
                onClicked: picker.close()
            }
        }
    }

    // --- The pedals, by group ---------------------------------------------------------------
    Flickable {
        id: list
        y: header.height + 16
        width: parent.width
        height: parent.height - y
        contentHeight: groups.height + 8
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        Column {
            id: groups
            width: list.width - 14
            spacing: 22

            Text {
                visible: picker.shown === 0
                text: "No pedal matches “" + search.text.trim() + "”."
                color: "#8F897A"
                font.family: Theme.labelFont
                font.pixelSize: 15
            }

            Repeater {
                model: picker.groupOrder
                Column {
                    id: group
                    required property string modelData
                    readonly property var types: picker.pedals.types.length >= 0 ? picker.typesIn(modelData) : []
                    visible: types.length > 0
                    width: groups.width
                    spacing: 10

                    Row {
                        spacing: 10
                        Text {
                            id: groupTitle
                            text: group.modelData.toUpperCase()
                            color: picker.cream
                            font.family: Theme.labelFont
                            font.pixelSize: 14
                            font.bold: true
                        }
                        Rectangle { // a rule to the right edge
                            anchors.verticalCenter: parent.verticalCenter
                            width: groups.width - groupTitle.width - 10
                            height: 1
                            color: "#3A3733"
                        }
                    }
                    Flow {
                        width: parent.width
                        spacing: 12
                        Repeater {
                            model: group.types
                            delegate: tile
                        }
                    }
                }
            }
        }
    }

    // One pedal: how it looks, its name, what it does, where it goes.
    Component {
        id: tile
        Rectangle {
            id: card
            required property var modelData
            readonly property real nativeWidth: (modelData.width ?? 1) * picker.slot - 12
            width: Math.max(picker.tileWidth, nativeWidth * picker.thumbScale + 24)
            height: picker.pedalHeight * picker.thumbScale + 108
            radius: 8
            color: cardArea.containsMouse ? "#2E2B27" : "#24221F"
            border.color: cardArea.containsMouse ? "#C9A86A" : "#3A3733"
            border.width: cardArea.containsMouse ? 2 : 1
            Accessible.role: Accessible.Button
            Accessible.name: "Add " + modelData.name + ": " + modelData.blurb

            Item { // the pedal, small: drawn once into a picture (which also clips cleanly as the list scrolls)
                x: (parent.width - width) / 2
                y: 12
                width: card.nativeWidth * picker.thumbScale
                height: picker.pedalHeight * picker.thumbScale
                layer.enabled: true
                layer.smooth: true
                PedalThumb {
                    info: card.modelData
                    width: card.nativeWidth
                    height: picker.pedalHeight
                    scale: picker.thumbScale
                    transformOrigin: Item.TopLeft
                }
            }
            Column {
                x: 12
                y: picker.pedalHeight * picker.thumbScale + 20
                width: parent.width - 24
                spacing: 4
                Row {
                    width: parent.width
                    spacing: 6
                    Text {
                        width: parent.width - placeTag.width - 6
                        text: card.modelData.name
                        color: "#F4F1E6"
                        font.family: Theme.labelFont
                        font.pixelSize: 15
                        font.bold: true
                        elide: Text.ElideRight
                    }
                    Rectangle {
                        id: placeTag
                        width: placeText.implicitWidth + 10
                        height: 18
                        radius: 3
                        color: card.modelData.loop ? "#2F4A5E" : "#4A3A22"
                        Text {
                            id: placeText
                            anchors.centerIn: parent
                            text: card.modelData.loop ? "LOOP" : "FRONT"
                            color: "#E8E4DA"
                            font.family: Theme.labelFont
                            font.pixelSize: 10
                            font.bold: true
                        }
                    }
                }
                Text {
                    width: parent.width
                    text: card.modelData.blurb
                    color: "#B9B2A0"
                    font.family: Theme.labelFont
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                    maximumLineCount: 3
                    elide: Text.ElideRight
                }
            }
            MouseArea { // over everything: the thumbnail's knobs are a picture here
                id: cardArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    picker.pedals.add(card.modelData.type)
                    picker.close()
                }
            }
        }
    }

    // A pedal as the board draws it, at its defaults (to be scaled down): case,
    // knobs (or sliders, or the editor's curve), light, name, footswitch.
    component PedalThumb: PedalCase {
        id: thumb
        required property var info
        colour: info.colour ?? "#444"
        pattern: info.pattern ?? ""
        readonly property var knobs: info.knobs ?? []

        Rectangle { // on a patterned case, a panel behind the knobs
            visible: thumb.knobPlate && knobRow.visible
            x: 5
            y: 14
            width: parent.width - 10
            height: knobRow.size + 22
            radius: 6
            color: "#101010"
        }
        Row {
            id: knobRow
            visible: !(thumb.info.sliders ?? false) && !(thumb.info.editor ?? false)
            readonly property real size: Math.min(44, (thumb.width - 12) / Math.max(1, thumb.knobs.length) - 2)
            x: (parent.width - width) / 2
            y: 20
            spacing: 2
            Repeater {
                model: knobRow.visible ? thumb.knobs : []
                Column {
                    id: knobColumn
                    required property var modelData
                    spacing: 1
                    Knob {
                        width: knobRow.size
                        height: knobRow.size
                        name: knobColumn.modelData.label
                        value: knobColumn.modelData.defaultValue
                        defaultValue: knobColumn.modelData.defaultValue
                        detents: knobColumn.modelData.detents ?? 0
                        capColor: "#1E1E1E"
                        capShadeColor: "#000"
                        pointerColor: "#F2F2F2"
                    }
                    Text {
                        width: knobRow.size + 2
                        horizontalAlignment: Text.AlignHCenter
                        text: knobColumn.modelData.label
                        color: thumb.lettering
                        font.family: Theme.labelFont
                        font.pixelSize: 9
                        font.bold: true
                        fontSizeMode: Text.HorizontalFit
                        minimumPixelSize: 6
                    }
                }
            }
        }
        Row { // a graphic EQ's sliders, flat
            id: sliderRow
            visible: thumb.info.sliders ?? false
            readonly property int n: thumb.knobs.length
            readonly property real each: (thumb.width - 16) / Math.max(1, n)
            x: 8
            y: 12
            Repeater {
                model: sliderRow.visible ? thumb.knobs : []
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
                        name: sliderColumn.modelData.label
                        value: sliderColumn.modelData.defaultValue
                        defaultValue: sliderColumn.modelData.defaultValue
                    }
                    Text {
                        width: parent.width
                        horizontalAlignment: Text.AlignHCenter
                        text: sliderColumn.modelData.label
                        color: thumb.lettering
                        font.family: Theme.labelFont
                        font.pixelSize: 9
                        font.bold: true
                        fontSizeMode: Text.HorizontalFit
                        minimumPixelSize: 6
                    }
                }
            }
        }
        Item { // the editor's face: a flat curve and EDIT
            visible: thumb.info.editor ?? false
            x: 8
            y: 12
            width: thumb.width - 16
            height: 88
            Rectangle {
                width: parent.width
                height: 60
                color: "#0E1418"
                Rectangle { y: 29; width: parent.width; height: 2; color: "#5FD3C8" }
            }
            Rectangle {
                y: 64
                width: parent.width
                height: 22
                radius: 3
                color: "#3A3B3E"
                Text {
                    anchors.centerIn: parent
                    text: "EDIT"
                    color: "#F2F2F2"
                    font.family: Theme.labelFont
                    font.pixelSize: 11
                    font.bold: true
                }
            }
        }
        Led {
            x: (thumb.info.editor ?? false) ? parent.width - 22 : (thumb.info.sliders ?? false) ? 12 : (parent.width - width) / 2
            y: (thumb.info.editor ?? false) ? 140 : (thumb.info.sliders ?? false) ? 116 : 88
            width: 10
            height: 10
            on: true
        }
        Rectangle { // on a patterned case, a plate behind the name
            visible: thumb.patterned
            x: 14
            y: (thumb.info.sliders ?? false) || (thumb.info.editor ?? false) ? 106 : 98
            width: parent.width - 28
            height: 30
            radius: 4
            color: "#101010"
            border.color: "#60FFFFFF"
        }
        Text {
            x: 8
            y: (thumb.info.sliders ?? false) || (thumb.info.editor ?? false) ? 110 : 102
            width: parent.width - 16
            horizontalAlignment: Text.AlignHCenter
            text: thumb.info.name ?? ""
            color: thumb.patterned ? "#F4F1E6" : thumb.lettering
            style: thumb.lightCase ? Text.Normal : Text.Raised
            styleColor: "#80000000"
            font.family: Theme.labelFont
            font.pixelSize: 19
            font.bold: true
            font.italic: true
            wrapMode: Text.WordWrap
        }
        Rectangle { // the footswitch
            x: (parent.width - width) / 2
            y: parent.height - height - 26
            width: 48
            height: 48
            radius: width / 2
            border.color: "#555"
            border.width: 2
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#E4E4E4" }
                GradientStop { position: 1.0; color: "#7A7A7A" }
            }
        }
    }
}

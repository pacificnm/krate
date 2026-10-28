import QtQuick

// MIDI Learn layer for a control. Put it inside the control with anchors.fill.
// Shown only in learn mode, where it takes the clicks: left-click arms the
// control (click again to disarm), right-click clears its mappings. Outline
// pulses while armed; a badge shows the mapped CC numbers.
Item {
    id: overlay

    required property string paramKey
    property bool round: true

    readonly property bool armed: MidiLearn.armedParam === paramKey
    readonly property string ccs: MidiLearn.revision >= 0 ? MidiLearn.mappingFor(paramKey) : ""

    visible: MidiLearn.active
    z: 100

    Rectangle {
        id: outline
        anchors.fill: parent
        anchors.margins: -6
        radius: overlay.round ? width / 2 : 6
        color: overlay.armed ? "#202A7FFF" : "transparent"
        border.color: Theme.accentUi
        border.width: overlay.armed ? 3 : 1

        SequentialAnimation on opacity {
            running: overlay.armed
            loops: Animation.Infinite
            alwaysRunToEnd: true
            NumberAnimation { from: 1.0; to: 0.35; duration: 450; easing.type: Easing.InOutSine }
            NumberAnimation { from: 0.35; to: 1.0; duration: 450; easing.type: Easing.InOutSine }
        }
    }

    Rectangle { // CC badge
        visible: overlay.ccs !== ""
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: -10
        width: badgeText.implicitWidth + 10
        height: badgeText.implicitHeight + 4
        radius: 4
        color: Theme.accentUi

        Text {
            id: badgeText
            anchors.centerIn: parent
            text: "CC " + overlay.ccs
            color: "white"
            font.family: Theme.labelFont
            font.pixelSize: 12
            font.bold: true
        }
    }

    MouseArea {
        anchors.fill: parent
        anchors.margins: -6
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        cursorShape: Qt.PointingHandCursor
        onClicked: mouse => {
            if (mouse.button === Qt.RightButton)
                MidiLearn.clear(overlay.paramKey)
            else
                MidiLearn.arm(overlay.paramKey)
        }
        onWheel: wheel => wheel.accepted = true // knobs don't turn in learn mode
    }
}

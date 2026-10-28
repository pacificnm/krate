import QtQuick

// Hardware push button. Labels usually sit on the panel beside it (PanelLabel);
// faceText prints on the button itself. Optional LED and RUN/STOP-style red bar.
Item {
    id: btn

    property string text: ""            // accessible name
    property string faceText: ""
    property bool dark: false
    property bool checkable: false
    property bool checked: false
    property bool hasLed: false
    property bool ledOn: checked
    property bool indicatorBar: false
    // A colored face (function buttons); transparent = the light or dark face.
    property color tint: "transparent"
    readonly property bool tinted: tint.a > 0

    signal clicked()
    signal pressed()
    signal released()

    width: 56
    height: 34
    activeFocusOnTab: true

    Accessible.role: Accessible.Button
    Accessible.name: text || faceText
    Accessible.checkable: checkable
    Accessible.checked: checked

    function activate() {
        if (checkable)
            checked = !checked
        clicked()
    }

    // Enter, not Space: Space is the panel's run/stop key (docs/UI.md).
    Keys.onReturnPressed: activate()
    Keys.onEnterPressed: activate()

    Rectangle { // drop shadow
        x: 1
        y: 4
        width: btn.width
        height: btn.height - 2
        radius: face.radius
        color: "#50000000"
    }

    Rectangle {
        id: face

        width: btn.width
        height: btn.height - 3
        y: mouse.pressed ? 2 : 0
        radius: 4
        border.color: btn.activeFocus ? Theme.accentUi : (btn.dark ? "#0E0E0F" : Theme.buttonEdge)
        border.width: btn.activeFocus ? 2 : 1
        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: btn.tinted ? Qt.lighter(btn.tint, 1.15) : btn.dark ? Theme.buttonDark : Theme.buttonLight
            }
            GradientStop {
                position: 1.0
                color: btn.tinted ? Qt.darker(btn.tint, 1.25) : btn.dark ? Theme.buttonDarkShade : Theme.buttonLightShade
            }
        }

        Rectangle {
            visible: btn.indicatorBar
            anchors.horizontalCenter: parent.horizontalCenter
            y: 10
            width: parent.width * 0.4
            height: 7
            radius: 2
            color: btn.ledOn ? Theme.ledOn : Theme.ledOff
        }

        Led {
            visible: btn.hasLed
            anchors.horizontalCenter: parent.horizontalCenter
            y: 7
            width: 10
            height: 10
            on: btn.ledOn
        }

        Text {
            visible: btn.faceText !== ""
            anchors.centerIn: parent
            text: btn.faceText
            color: btn.dark && !btn.tinted ? Theme.inkInverse : Theme.ink
            font.family: Theme.labelFont
            font.pixelSize: Math.min(16, btn.height * 0.45)
            font.bold: true
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        onPressed: btn.pressed()
        onReleased: btn.released()
        onCanceled: btn.released()
        onClicked: {
            btn.forceActiveFocus()
            btn.activate()
        }
    }
}

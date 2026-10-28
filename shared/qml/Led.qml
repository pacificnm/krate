import QtQuick

Rectangle {
    id: led

    property bool on: false

    width: 12
    height: 12
    radius: width / 2
    color: on ? Theme.ledOn : Theme.ledOff
    border.color: Qt.darker(Theme.ledOff, 1.6)
    border.width: 1

    Rectangle {
        z: -1
        visible: led.on
        anchors.centerIn: parent
        width: parent.width + 10
        height: parent.height + 10
        radius: led.radius + 5
        color: Theme.ledOn
        opacity: 0.22
    }

    Rectangle {
        visible: led.on
        anchors.centerIn: parent
        width: Math.min(parent.width, parent.height) * 0.45
        height: width
        radius: width / 2
        color: "#FFC2B5"
    }
}

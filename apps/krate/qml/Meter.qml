import QtQuick
import Krate

// Vertical level meter, 0..1 (-60..0 dB), red over the top 6 dB.
Rectangle {
    id: meter

    property real level: 0

    width: 8
    height: 120
    radius: 2
    color: Theme.ledOff

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: parent.height * Math.max(0, Math.min(1, meter.level))
        radius: 2
        color: meter.level > 0.9 ? Theme.ledOn : Theme.labelOrange
    }
}

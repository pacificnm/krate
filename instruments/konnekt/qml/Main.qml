import QtQuick
import QtQuick.Controls
import Krate

// The Konnekt app's window: its panel, filling it.
ApplicationWindow {
    id: root

    width: 1500
    height: 920
    minimumWidth: 900
    minimumHeight: 600
    visible: true
    title: "Konnekt"
    color: "#141518"

    required property Konnekt konnekt

    Panel {
        anchors.fill: parent
        konnekt: root.konnekt
        focus: true
    }
}

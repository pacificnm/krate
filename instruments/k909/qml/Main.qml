import QtQuick
import Krate

// The standalone K909 window: the panel at its design size, scaled to fit.
Window {
    id: root

    width: 1200
    height: 705
    minimumWidth: 800
    minimumHeight: 470
    visible: true
    title: "K909"
    color: Theme.desk

    readonly property real panelScale: Math.min(width / panel.width, height / panel.height)

    Panel {
        id: panel
        focus: true
        x: (root.width - width * root.panelScale) / 2
        y: (root.height - height * root.panelScale) / 2
        transform: Scale {
            xScale: root.panelScale
            yScale: root.panelScale
        }
    }
}

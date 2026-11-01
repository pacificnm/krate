import QtQuick
import Krate

// The standalone Kessel window: the panel at its design size, scaled to fit.
Window {
    id: root
    required property Drums drums
    required property Kits kits
    required property Samples samples

    width: 1260
    height: 700
    minimumWidth: 840
    minimumHeight: 467
    visible: true
    title: "Kessel"
    color: Theme.desk

    readonly property real panelScale: Math.min(width / panel.width, height / panel.height)

    Panel {
        drums: root.drums
        kits: root.kits
        samples: root.samples
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

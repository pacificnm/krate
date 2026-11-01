import QtQuick
import Krate

// The standalone Kobalt window: the panel at its design size, scaled to fit.
Window {
    id: root
    required property Kobalt kobalt
    required property Patches patches
    required property Sequencer sequencer

    width: 1200
    height: 825
    minimumWidth: 800
    minimumHeight: 550
    visible: true
    title: "Kobalt"
    color: Theme.desk

    readonly property real panelScale: Math.min(width / panel.width, height / panel.height)

    Panel {
        kobalt: root.kobalt
        patches: root.patches
        sequencer: root.sequencer
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

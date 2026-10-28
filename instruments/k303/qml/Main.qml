import QtQuick
import Krate

Window {
    id: root

    width: 1200
    height: 519
    minimumWidth: 800
    minimumHeight: 346
    visible: true
    title: "k303"
    color: Theme.desk

    readonly property real panelScale: Math.min(width / Theme.designWidth,
                                                height / Theme.designHeight)

    Panel {
        focus: true
        x: (root.width - width * root.panelScale) / 2
        y: (root.height - height * root.panelScale) / 2
        transform: Scale {
            xScale: root.panelScale
            yScale: root.panelScale
        }
    }
}

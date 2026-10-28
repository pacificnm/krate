import QtQuick
import QtQuick.Shapes

// Two-position slide switch: saw (0) / square (1), with waveform icons above.
Item {
    id: sw

    property int position: 0

    signal toggled(int position)

    width: 90
    height: 62

    Accessible.role: Accessible.CheckBox
    Accessible.name: "Waveform " + (position === 0 ? "saw" : "square")

    component WaveIcon: Shape {
        id: icon
        property string path
        width: 22
        height: 12
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeColor: Theme.ink
            strokeWidth: 2
            fillColor: "transparent"
            PathSvg { path: icon.path }
        }
    }

    WaveIcon { x: 12; y: 0; path: "M 0 12 L 11 0 L 11 12 L 22 0 L 22 12" }
    WaveIcon { x: 56; y: 0; path: "M 0 12 L 0 0 L 11 0 L 11 12 L 22 12 L 22 0" }

    Rectangle { // track
        x: 8
        y: 18
        width: 74
        height: 20
        radius: 10
        color: Theme.band
        border.color: "#0C0C0D"

        Rectangle { // thumb
            x: sw.position === 0 ? 2 : parent.width - width - 2
            y: 2
            width: 34
            height: 16
            radius: 8
            gradient: Gradient {
                GradientStop { position: 0.0; color: Theme.knobCap }
                GradientStop { position: 1.0; color: Theme.knobCapShade }
            }

            Behavior on x { NumberAnimation { duration: 80 } }
        }
    }

    PanelLabel {
        centerX: sw.width / 2
        y: 44
        text: "WAVEFORM"
        font.pixelSize: 12
    }

    MouseArea {
        anchors.fill: parent
        onClicked: sw.toggled(1 - sw.position) // owner writes the model
    }
}

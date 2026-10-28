import QtQuick

// Silk-screened panel text. Set centerX to center it on a control.
Text {
    property real centerX: NaN
    property bool orange: false
    property bool inverse: false

    x: isNaN(centerX) ? 0 : centerX - width / 2
    color: orange ? Theme.labelOrange : (inverse ? Theme.inkInverse : Theme.ink)
    font.family: Theme.labelFont
    font.pixelSize: 14
    font.bold: true
    horizontalAlignment: Text.AlignHCenter
}

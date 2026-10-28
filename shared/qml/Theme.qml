pragma Singleton

import QtQuick

QtObject {
    // Design resolution: the panel is laid out at this size and scaled to the window.
    readonly property int designWidth: 1600
    readonly property int designHeight: 692

    readonly property color desk: "#2A2B2E"     // letterbox around the panel

    // Upper deck (brushed silver)
    readonly property color silverLight: "#E6E7E9"
    readonly property color silverDark: "#BDBEC1"
    readonly property color deckLine: "#8C8E92"

    // Lower deck and key bed
    readonly property color lowerDeck: "#CDCED1"
    readonly property color keyBed: "#E9E9EA"
    readonly property color band: "#26272A"

    // Buttons and keys
    readonly property color buttonLight: "#FBFBFA"
    readonly property color buttonLightShade: "#D2D3D5"
    readonly property color buttonDark: "#4A4B4E"
    readonly property color buttonDarkShade: "#1F2022"
    readonly property color buttonEdge: "#7B7D81"

    // Knobs
    readonly property color knobSkirt: "#2E2F32"
    readonly property color knobSkirtShade: "#141516"
    readonly property color knobRib: "#5C5E62"
    readonly property color knobCap: "#F7F7F7"
    readonly property color knobCapShade: "#9EA0A4"

    // Type, LEDs, display
    readonly property color ink: "#1C1C1C"
    readonly property color inkInverse: "#F2F2F2"
    readonly property color labelOrange: "#E0781E"
    readonly property color ledOn: "#FF2A1A"
    readonly property color ledOff: "#5A1E18"
    readonly property color lcdBg: "#D8DBD4"
    readonly property color lcdInk: "#2A2D2A"
    readonly property color accentUi: "#2A7FFF"

    readonly property string labelFont: "sans-serif"
}

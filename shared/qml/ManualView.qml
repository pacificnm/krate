pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

// An instrument's manual (a Markdown file, see ManualReader): its sections on
// the left (filtered by the search), the chosen one on the right. It sizes
// itself to the window (not to a scaled panel it's declared in).
Popup {
    id: view

    parent: Overlay.overlay
    x: Math.round((parent.width - width) / 2)
    y: Math.round((parent.height - height) / 2)
    width: Math.min(1400, parent.width - 60)
    height: parent.height - 60

    property string source: ""  // e.g. "qrc:/qt/qml/Krimson/docs/MANUAL.md"
    property string title: ""
    property int current: 0
    readonly property var sections: source !== "" ? Manual.sections(source) : []
    readonly property var shown: search.text.trim() === "" ? sections.map((s, i) => i) : Manual.search(sections, search.text)

    modal: true
    padding: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    onOpened: search.forceActiveFocus()
    onShownChanged: if (shown.length > 0 && shown.indexOf(current) < 0) current = shown[0]

    background: Rectangle {
        radius: 8
        color: "#1C1D20"
        border.color: "#4A4C50"
        border.width: 2
    }

    Item {
        anchors.fill: parent

        // Title, search, close.
        Rectangle {
            id: header
            width: parent.width
            height: 58
            radius: 8
            color: "#26272B"
            Text {
                x: 20
                anchors.verticalCenter: parent.verticalCenter
                text: view.title + "  ·  MANUAL"
                color: "#F2F2F2"
                font.family: Theme.labelFont
                font.pixelSize: 20
                font.bold: true
            }
            TextField {
                id: search
                anchors.right: closeButton.left
                anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                width: 300
                placeholderText: "Search the manual (e.g. sync, glide)"
                Accessible.name: "Search the manual"
            }
            PanelButton {
                id: closeButton
                anchors.right: parent.right
                anchors.rightMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                width: 80
                height: 34
                faceText: "CLOSE"
                text: "Close the manual"
                onClicked: view.close()
            }
        }

        // The sections.
        ListView {
            id: contents
            x: 10
            y: header.height + 10
            width: 250
            height: parent.height - y - 10
            clip: true
            model: view.shown
            ScrollBar.vertical: ScrollBar {}
            delegate: Rectangle {
                id: entry
                required property int modelData
                readonly property bool chosen: modelData === view.current
                width: ListView.view.width - 10
                height: 36
                radius: 4
                color: chosen ? "#3B4A5A" : (entryArea.containsMouse ? "#2E2F33" : "transparent")
                Text {
                    x: 12
                    width: parent.width - 20
                    anchors.verticalCenter: parent.verticalCenter
                    text: view.sections[entry.modelData]?.title ?? ""
                    color: entry.chosen ? "#FFFFFF" : "#C9CACC"
                    font.family: Theme.labelFont
                    font.pixelSize: 14
                    font.bold: entry.chosen
                    elide: Text.ElideRight
                }
                MouseArea {
                    id: entryArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: view.current = entry.modelData
                }
                Accessible.role: Accessible.ListItem
                Accessible.name: view.sections[entry.modelData]?.title ?? ""
            }
        }
        Text {
            x: contents.x + 12
            y: contents.y + 8
            visible: view.shown.length === 0
            text: view.sections.length === 0 ? "The manual couldn't be read." : "Nothing matches."
            color: "#9A9488"
            font.family: Theme.labelFont
            font.pixelSize: 14
        }

        // The chosen section.
        Flickable {
            id: page
            x: contents.x + contents.width + 10
            y: header.height + 10
            width: parent.width - x - 14
            height: parent.height - y - 10
            clip: true
            contentHeight: body.height + 40
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
            Connections {
                target: view
                function onCurrentChanged() { page.contentY = 0 }
            }

            Column {
                id: body
                x: 16
                width: page.width - 48
                spacing: 14
                Text {
                    width: parent.width
                    text: view.sections[view.current]?.title ?? ""
                    color: "#FFFFFF"
                    font.family: Theme.labelFont
                    font.pixelSize: 26
                    font.bold: true
                    wrapMode: Text.WordWrap
                }
                Text {
                    width: parent.width
                    text: view.sections[view.current]?.text ?? ""
                    textFormat: Text.MarkdownText
                    color: "#E2E2E2"
                    linkColor: "#7FB2FF"
                    font.family: Theme.labelFont
                    font.pixelSize: 16
                    lineHeight: 1.25
                    wrapMode: Text.WordWrap
                    onLinkActivated: link => Qt.openUrlExternally(link)
                }
            }
        }
    }
}

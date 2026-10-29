pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Krate
import KrateRack

// The song library (docs/SONG_PLAYER.md#library): artists, their albums and
// the songs in track order; search; click a song to open it. A dot marks
// songs with their sounds saved.
Rectangle {
    id: panel

    radius: 4
    color: Theme.silverLight
    border.color: Theme.deckLine

    Text {
        id: heading
        x: 12
        y: 10
        text: "LIBRARY"
        color: Theme.ink
        font.family: Theme.labelFont
        font.pixelSize: 13
        font.bold: true
    }

    TextField {
        id: search
        x: 10
        y: heading.y + heading.height + 8
        width: panel.width - 20
        height: 32
        placeholderText: "Search artist, album, title"
        Accessible.name: "Search the library"
    }

    ListView {
        id: rows
        x: 6
        y: search.y + search.height + 8
        width: panel.width - 12
        height: panel.height - y - 6
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        model: Song.libraryRevision >= 0 ? Song.libraryRows(search.text) : []
        ScrollBar.vertical: ScrollBar {}

        delegate: Item {
            id: row
            required property var modelData
            readonly property string kind: modelData.kind
            width: rows.width
            height: kind === "artist" ? 30 : kind === "album" ? 22 : 30

            Rectangle { // the open song
                anchors.fill: parent
                visible: row.kind === "song" && row.modelData.open
                radius: 3
                color: "#F4E7D6"
                border.color: Theme.labelOrange
            }
            Text {
                x: row.kind === "artist" ? 6 : row.kind === "album" ? 14 : 22
                width: parent.width - x - 24
                anchors.verticalCenter: parent.verticalCenter
                text: row.kind === "song" && row.modelData.track > 0 ? row.modelData.track + ".  " + row.modelData.text
                                                                    : row.modelData.text
                color: row.kind === "album" ? "#55575B" : Theme.ink
                font.family: Theme.labelFont
                font.pixelSize: row.kind === "artist" ? 15 : row.kind === "album" ? 11 : 13
                font.bold: row.kind === "artist" || (row.kind === "song" && row.modelData.open)
                font.italic: row.kind === "album"
                elide: Text.ElideRight
            }
            Rectangle { // sounds saved
                visible: row.kind === "song" && row.modelData.soundsSaved
                anchors.right: parent.right
                anchors.rightMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                width: 7
                height: 7
                radius: 3.5
                color: Theme.labelOrange
            }
            MouseArea {
                anchors.fill: parent
                enabled: row.kind === "song"
                cursorShape: Qt.PointingHandCursor
                onClicked: Song.openSong(row.modelData.id)
            }
            Accessible.role: row.kind === "song" ? Accessible.Button : Accessible.StaticText
            Accessible.name: row.modelData.text + (row.kind === "song" && row.modelData.soundsSaved ? ", sounds saved" : "")
        }

        Text {
            anchors.centerIn: parent
            width: parent.width - 24
            visible: rows.count === 0
            text: search.text !== "" ? "Nothing matches" : "No songs yet: OPEN or drop a song to add it"
            color: "#6A6C70"
            font.family: Theme.labelFont
            font.pixelSize: 13
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
        }
    }
}

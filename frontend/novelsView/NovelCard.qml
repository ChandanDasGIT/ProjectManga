import QtQuick

// One book: cover, title and author.
Item {
    id: root
    property string title: ""
    property url coverUrl: ""
    property string subtitle: ""
    signal clicked()

    // The cover keeps the covers' shape when a narrow window makes the card narrower.
    readonly property int coverHeight: Math.round(width * Theme.coverHeight / Theme.coverWidth)

    implicitWidth: Theme.coverWidth
    implicitHeight: coverHeight + 58

    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler { onTapped: root.clicked() }

    CoverImage {
        id: cover
        width: parent.width
        height: root.coverHeight
        source: root.coverUrl
        highlighted: hover.hovered
    }

    Column {
        anchors.top: cover.bottom
        anchors.topMargin: 8
        width: parent.width
        spacing: 2

        Text {
            width: parent.width
            text: root.title
            color: hover.hovered ? Theme.accentHover : Theme.text
            font.pixelSize: Theme.bodySize
            font.weight: Font.Medium
            wrapMode: Text.Wrap
            maximumLineCount: 2
            elide: Text.ElideRight
        }
        Text {
            width: parent.width
            visible: text.length > 0
            text: root.subtitle
            color: Theme.textMuted
            font.pixelSize: Theme.smallSize
            elide: Text.ElideRight
        }
    }
}

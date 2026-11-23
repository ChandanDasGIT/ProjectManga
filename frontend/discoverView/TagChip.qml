import QtQuick

// A pill that is on or off: a category to browse by, or a content rating to show.
Rectangle {
    id: root
    property string text: ""
    property bool selected: false
    signal clicked()

    implicitWidth: label.implicitWidth + 24
    implicitHeight: 30
    radius: height / 2
    color: selected ? Theme.accent : hover.hovered ? Theme.cardHover : Theme.cardBg
    border.width: selected ? 0 : 1
    border.color: Theme.edge

    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler { onTapped: root.clicked() }

    Text {
        id: label
        anchors.centerIn: parent
        text: root.text
        color: root.selected ? Theme.accentText : Theme.text
        font.pixelSize: Theme.smallSize + 1
        font.weight: root.selected ? Font.DemiBold : Font.Normal
    }
}

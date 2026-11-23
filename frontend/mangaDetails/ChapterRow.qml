import QtQuick
import QtQuick.Layouts

// One chapter in the details list: its name, who translated it and when, and whether it is new,
// read, or only on an official site.
Item {
    id: root
    property string label: ""
    property string group: ""
    property string date: ""
    property bool read: false
    property bool isNew: false
    property bool external: false
    signal clicked()

    implicitHeight: 56

    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler { onTapped: root.clicked() }

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusSmall
        color: hover.hovered ? Theme.cardHover : "transparent"
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: Theme.gap

        Column {
            Layout.fillWidth: true
            spacing: 3

            Text {
                width: parent.width
                text: root.label
                color: root.read ? Theme.textFaint : Theme.text
                font.pixelSize: Theme.bodySize
                font.weight: root.read ? Font.Normal : Font.Medium
                elide: Text.ElideRight
            }
            Text {
                width: parent.width
                visible: text.length > 0
                text: [root.group, root.date].filter(part => part.length > 0).join(" · ")
                color: Theme.textFaint
                font.pixelSize: Theme.smallSize
                elide: Text.ElideRight
            }
        }

        Text {
            visible: root.external
            text: "Official site ↗"
            color: Theme.textMuted
            font.pixelSize: Theme.smallSize
        }
        Text {
            visible: root.read && !root.isNew
            text: "Read"
            color: Theme.textFaint
            font.pixelSize: Theme.smallSize
        }
        Rectangle {
            visible: root.isNew
            implicitWidth: newText.implicitWidth + 14
            implicitHeight: 20
            radius: 10
            color: Theme.accent

            Text {
                id: newText
                anchors.centerIn: parent
                text: "NEW"
                color: Theme.accentText
                font.pixelSize: 11
                font.weight: Font.DemiBold
            }
        }
    }

    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        height: 1
        color: Theme.edge
    }
}

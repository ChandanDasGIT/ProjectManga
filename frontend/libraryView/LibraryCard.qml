import QtQuick

// One library entry: cover with a "new chapters" badge, title, where it comes from and, when one
// can be estimated, when the next chapter is likely. A book already started shows how far through it
// the reader is as a bar under the cover, opens where reading stopped, and has a "Details" button
// for the details page. The ✕ in the cover's corner (or right-click, or a long press on a touch
// screen) asks to remove it.
Item {
    id: root
    property string title: ""
    property url coverUrl: ""
    property string sourceName: ""
    property int unreadCount: 0
    property string nextEstimate: ""
    property bool started: false
    property real progress: -1          // 0 .. 1 through the book; -1: no bar
    signal clicked()
    signal detailsClicked()
    signal removeRequested()

    // The cover keeps the covers' shape when a narrow window makes the card narrower.
    readonly property int coverHeight: Math.round(width * Theme.coverHeight / Theme.coverWidth)

    implicitWidth: Theme.coverWidth
    implicitHeight: coverHeight + 76

    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler {
        onTapped: point => {
            const p = detailsButton.mapFromItem(root, point.position)
            const x = removeButton.mapFromItem(root, point.position)
            if (removeButton.contains(x)) root.removeRequested()
            else if (detailsButton.visible && detailsButton.contains(p)) root.detailsClicked()
            else root.clicked()
        }
        onLongPressed: root.removeRequested()
    }
    // A touch has no buttons, so without the device limit every tap on a phone would count here.
    TapHandler {
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        acceptedButtons: Qt.RightButton
        onTapped: root.removeRequested()
    }

    CoverImage {
        id: cover
        width: parent.width
        height: root.coverHeight
        source: root.coverUrl
        highlighted: hover.hovered

        Rectangle {
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: 8
            visible: root.unreadCount > 0
            width: badgeText.implicitWidth + 14
            height: 22
            radius: 11
            color: Theme.accent

            Text {
                id: badgeText
                anchors.centerIn: parent
                text: (root.unreadCount > 99 ? "99+" : root.unreadCount) + " new"
                color: Theme.accentText
                font.pixelSize: Theme.smallSize
                font.weight: Font.DemiBold
            }
        }

        Rectangle {
            id: removeButton
            objectName: "cardRemove"
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: 8
            width: 26
            height: 26
            radius: 13
            color: removeHover.hovered ? Theme.cardHover : Theme.cardBg
            border.width: 1
            border.color: Theme.edge

            HoverHandler { id: removeHover; cursorShape: Qt.PointingHandCursor }

            // Drawn, not the ✕ character: Android's fonts have no glyph for it.
            Repeater {
                model: [45, -45]
                Rectangle {
                    required property int modelData
                    anchors.centerIn: parent
                    width: 11
                    height: 1.5
                    radius: 0.75
                    rotation: modelData
                    color: Theme.text
                    antialiasing: true
                }
            }
        }

        Rectangle {
            id: detailsButton
            objectName: "cardDetails"
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            anchors.margins: 8
            visible: root.started
            width: detailsText.implicitWidth + 16
            height: 24
            radius: Theme.radiusSmall
            color: detailsHover.hovered ? Theme.cardHover : Theme.cardBg
            border.width: 1
            border.color: Theme.edge

            HoverHandler { id: detailsHover; cursorShape: Qt.PointingHandCursor }

            Text {
                id: detailsText
                anchors.centerIn: parent
                text: "Details"
                color: Theme.text
                font.pixelSize: Theme.smallSize
            }
        }
    }

    // In the gap between cover and title, so the card keeps its size.
    Rectangle {
        objectName: "cardProgress"
        anchors.top: cover.bottom
        anchors.topMargin: 2
        width: parent.width
        height: 4
        radius: 2
        visible: root.progress >= 0
        color: Theme.edge

        Rectangle {
            width: Math.max(height, parent.width * Math.min(1, root.progress))
            height: parent.height
            radius: 2
            color: Theme.accent
            visible: root.progress > 0
        }
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
            text: root.sourceName
            color: Theme.textMuted
            font.pixelSize: Theme.smallSize
            elide: Text.ElideRight
        }
        Text {
            width: parent.width
            visible: root.nextEstimate.length > 0
            text: "Next: " + root.nextEstimate
            color: Theme.textFaint
            font.pixelSize: Theme.smallSize
            elide: Text.ElideRight
        }
    }
}

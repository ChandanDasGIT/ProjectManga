import QtQuick

// A manga cover in a card frame, with a text in its place while it loads or when there is none.
Rectangle {
    id: root
    property alias source: image.source
    property bool highlighted: false

    radius: Theme.radiusSmall
    color: Theme.cardBg
    border.width: 1
    border.color: highlighted ? Theme.accent : Theme.edge

    Image {
        id: image
        anchors.fill: parent
        anchors.margins: 1
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        sourceSize.width: Theme.coverWidth * 2
    }

    Text {
        anchors.centerIn: parent
        visible: image.status !== Image.Ready
        text: image.status === Image.Loading ? "Loading…" : "No cover"
        color: Theme.textFaint
        font.pixelSize: Theme.smallSize
    }
}

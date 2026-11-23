import QtQuick

// One page of a chapter at the reader's width. Until it has loaded it keeps a page-shaped
// space with its number, and offers to try again when it fails.
Item {
    id: root
    property url source
    property int pageNumber: 0
    // Width over height; a usual page's until the image is in.
    readonly property real aspect: image.status === Image.Ready && image.implicitHeight > 0
            ? image.implicitWidth / image.implicitHeight : 1 / 1.42

    height: image.status === Image.Ready && image.implicitWidth > 0
            ? Math.round(width * image.implicitHeight / image.implicitWidth)
            : Math.round(width * 1.42)

    function retry() {
        image.source = ""
        image.source = Qt.binding(() => root.source)
    }

    Image {
        id: image
        anchors.fill: parent
        source: root.source
        fillMode: Image.PreserveAspectFit
        asynchronous: true
        smooth: true
        mipmap: true
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 1
        visible: image.status !== Image.Ready
        color: Theme.cardBg
        radius: Theme.radiusSmall

        Column {
            anchors.centerIn: parent
            spacing: 12

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: image.status === Image.Error ? "Page " + root.pageNumber + " did not load"
                                                   : "Loading page " + root.pageNumber + "…"
                color: image.status === Image.Error ? Theme.error : Theme.textFaint
                font.pixelSize: Theme.bodySize
            }
            ThemedButton {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: image.status === Image.Error
                text: "Try again"
                onClicked: root.retry()
            }
        }
    }
}

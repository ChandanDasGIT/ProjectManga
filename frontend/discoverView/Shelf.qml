import QtQuick
import QtQuick.Controls

// One titled row of covers that scrolls sideways ("Popular", "For you", ...).
Column {
    id: root
    property string title: ""
    property var items: []
    property bool loading: false
    property string error: ""
    signal mangaOpened(string sourceId, string mangaId)

    spacing: 12

    Row {
        spacing: Theme.gap

        Text {
            id: shelfTitle
            text: root.title
            color: Theme.text
            font.pixelSize: Theme.headingSize
            font.weight: Font.DemiBold
        }
        Text {
            anchors.baseline: shelfTitle.baseline
            // Wraps instead of running past the edge of a narrow window.
            width: Math.min(implicitWidth, root.width - shelfTitle.width - parent.spacing)
            wrapMode: Text.Wrap
            visible: text.length > 0
            text: root.loading && root.items.length === 0 ? "Loading…"
                : root.error.length > 0 ? root.error
                : !root.loading && root.items.length === 0 ? "Nothing here yet."
                : ""
            color: root.error.length > 0 ? Theme.error : Theme.textMuted
            font.pixelSize: Theme.bodySize
        }
    }

    ListView {
        id: row
        width: root.width
        height: root.items.length > 0 ? Theme.coverHeight + 58 + 14 : 0
        visible: root.items.length > 0
        orientation: ListView.Horizontal
        spacing: Theme.gap + 8
        clip: true
        model: root.items
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }

        delegate: SearchResultCard {
            required property var modelData
            width: Theme.coverWidth
            title: modelData.title
            coverUrl: modelData.coverUrl
            subtitle: modelData.subtitle
            onClicked: root.mangaOpened(modelData.sourceId, modelData.mangaId)
        }
    }
}

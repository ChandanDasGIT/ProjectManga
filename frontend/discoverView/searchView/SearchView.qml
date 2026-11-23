import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// MangaDex search results as a grid of covers, with a message in the middle while there is
// nothing to show (before the first search, searching, nothing found, failed). The search field
// itself is in the top bar.
Item {
    id: root
    readonly property var results: wrap.results
    readonly property bool loading: wrap.loading
    readonly property string searchedFor: wrap.searchedFor
    readonly property string errorText: wrap.errorText
    signal mangaOpened(string sourceId, string mangaId)

    function search(query) {
        if (query.trim().length > 0) wrap.search(query)
    }

    SearchViewWrap { id: wrap }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.margin
        spacing: 18

        Text {
            visible: root.results.length > 0
            text: root.results.length + " results for “" + root.searchedFor + "”"
            color: Theme.textMuted
            font.pixelSize: Theme.smallSize
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            GridView {
                id: grid
                anchors.fill: parent
                clip: true
                visible: root.results.length > 0
                // Compact: two columns that share the width.
                readonly property int cardWidth: Theme.compact ? Math.floor(width / 2) - 2 * Theme.gap : Theme.coverWidth
                cellWidth: cardWidth + 2 * Theme.gap
                cellHeight: Math.round(cardWidth * Theme.coverHeight / Theme.coverWidth) + 58 + 2 * Theme.gap
                model: wrap.resultModel
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}

                delegate: SearchResultCard {
                    required property string sourceId
                    required property string mangaId
                    required title
                    required coverUrl
                    required subtitle
                    width: grid.cardWidth
                    onClicked: root.mangaOpened(sourceId, mangaId)
                }

                footer: Item {
                    width: grid.width
                    height: wrap.canLoadMore || (root.loading && root.results.length > 0) ? 72 : 0
                    visible: height > 0

                    ThemedButton {
                        anchors.centerIn: parent
                        text: root.loading ? "Loading…" : "Load more"
                        enabled: wrap.canLoadMore
                        onClicked: wrap.loadMore()
                    }
                }
            }

            Column {
                anchors.centerIn: parent
                width: Math.min(parent.width, 420)
                spacing: 8
                visible: root.results.length === 0

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: root.loading ? "Searching…"
                        : root.errorText.length > 0 ? "Search failed"
                        : root.searchedFor.length > 0 ? "Nothing found"
                        : "Find a manga"
                    color: Theme.text
                    font.pixelSize: Theme.headingSize
                    font.weight: Font.DemiBold
                }
                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    visible: !root.loading
                    text: root.errorText.length > 0 ? root.errorText
                        : root.searchedFor.length > 0 ? "Nothing matches “" + root.searchedFor + "”. Try another title or spelling."
                        : "Type a title in the search bar above and press Enter. Results come from MangaDex."
                    color: Theme.textMuted
                    font.pixelSize: Theme.bodySize
                }
            }
        }
    }
}

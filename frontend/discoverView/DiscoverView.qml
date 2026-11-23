import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Discover MangaDex: the categories panel (content ratings, demographics, genres, themes) on top;
// below it rows of popular, updated, top rated, new and "For you" titles, or the titles of the
// chosen categories. A search from the top bar shows its results here instead.
Item {
    id: root
    property bool searching: false
    property bool categoriesOpen: false
    signal mangaOpened(string sourceId, string mangaId)

    function search(query) {
        searching = true
        searchView.search(query)
    }
    function closeSearch() {
        searching = false
    }

    onVisibleChanged: if (visible) wrap.start()
    Component.onCompleted: if (visible) wrap.start()

    DiscoverViewWrap { id: wrap }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.margin
            Layout.rightMargin: Theme.margin
            Layout.topMargin: 18
            spacing: 8

            Item { Layout.fillWidth: true }
            ThemedButton {
                id: categoriesButton
                objectName: "categoriesButton"
                visible: !root.searching
                text: wrap.selectedCount > 0 ? "Categories (" + wrap.selectedCount + ")" : "Categories"
                onClicked: root.categoriesOpen = !root.categoriesOpen

                // The label as ThemedButton draws it, with room on the right for the arrow.
                contentItem: Text {
                    text: categoriesButton.text
                    font: categoriesButton.font
                    color: Theme.text
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                    rightPadding: Math.ceil(font.pixelSize * 1.05)

                    ThemedTriangle {
                        anchors.right: parent.right
                        anchors.rightMargin: 1
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.verticalCenterOffset: -size * 0.1
                        up: root.categoriesOpen
                        size: categoriesButton.font.pixelSize
                    }
                }
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.searching ? 1 : 0

            Flickable {
                id: page
                clip: true
                contentWidth: width
                contentHeight: content.implicitHeight
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}

                Column {
                    id: content
                    x: Theme.margin
                    width: page.width - 2 * Theme.margin
                    topPadding: 18
                    bottomPadding: Theme.margin
                    spacing: 28

                    // ---- categories ----
                    Rectangle {
                        width: content.width
                        visible: root.categoriesOpen
                        height: visible ? categories.implicitHeight + 32 : 0
                        radius: Theme.radius
                        color: Theme.panelBg
                        border.width: 1
                        border.color: Theme.edge

                        Column {
                            id: categories
                            x: 16
                            y: 16
                            width: parent.width - 32
                            spacing: 10

                            RowLayout {
                                width: parent.width

                                Text {
                                    text: "Browse by category"
                                    color: Theme.text
                                    font.pixelSize: Theme.headingSize
                                    font.weight: Font.DemiBold
                                }
                                Item { Layout.fillWidth: true }
                                ThemedButton {
                                    visible: wrap.browsing
                                    text: "Clear"
                                    onClicked: wrap.clearBrowse()
                                }
                            }
                            Text {
                                visible: wrap.tagsLoading || wrap.tagsError.length > 0
                                text: wrap.tagsLoading ? "Loading categories…" : wrap.tagsError
                                color: wrap.tagsLoading ? Theme.textMuted : Theme.error
                                font.pixelSize: Theme.bodySize
                            }

                            Text { text: "Content"; color: Theme.textMuted; font.pixelSize: Theme.smallSize }
                            Flow {
                                width: parent.width
                                spacing: 8
                                Repeater {
                                    model: wrap.ratings
                                    TagChip {
                                        required property var modelData
                                        objectName: "rating_" + modelData.key
                                        text: modelData.label
                                        selected: modelData.on
                                        onClicked: wrap.setRating(modelData.key, !modelData.on)
                                    }
                                }
                            }

                            Text { text: "Demographic"; color: Theme.textMuted; font.pixelSize: Theme.smallSize }
                            Flow {
                                width: parent.width
                                spacing: 8
                                Repeater {
                                    model: wrap.demographics
                                    TagChip {
                                        required property var modelData
                                        text: modelData.label
                                        selected: modelData.selected
                                        onClicked: wrap.toggleDemographic(modelData.key)
                                    }
                                }
                            }

                            Text { text: "Genres"; color: Theme.textMuted; font.pixelSize: Theme.smallSize; visible: wrap.genres.length > 0 }
                            Flow {
                                width: parent.width
                                spacing: 8
                                Repeater {
                                    model: wrap.genres
                                    TagChip {
                                        required property var modelData
                                        objectName: "tag_" + modelData.name
                                        text: modelData.name
                                        selected: modelData.selected
                                        onClicked: wrap.toggleTag(modelData.id)
                                    }
                                }
                            }

                            Text { text: "Themes"; color: Theme.textMuted; font.pixelSize: Theme.smallSize; visible: wrap.themes.length > 0 }
                            Flow {
                                width: parent.width
                                spacing: 8
                                Repeater {
                                    model: wrap.themes
                                    TagChip {
                                        required property var modelData
                                        text: modelData.name
                                        selected: modelData.selected
                                        onClicked: wrap.toggleTag(modelData.id)
                                    }
                                }
                            }
                        }
                    }

                    // ---- shelves ----
                    Repeater {
                        model: wrap.browsing ? [] : wrap.shelfModel

                        Shelf {
                            required title
                            required items
                            required loading
                            required error
                            width: content.width
                            onMangaOpened: (sourceId, mangaId) => root.mangaOpened(sourceId, mangaId)
                        }
                    }

                    // ---- the chosen categories' titles ----
                    Column {
                        width: content.width
                        visible: wrap.browsing
                        spacing: 16

                        Text {
                            text: wrap.resultsLoading && wrap.results.length === 0 ? "Loading…"
                                : wrap.resultsError.length > 0 ? wrap.resultsError
                                : wrap.results.length === 0 ? "No titles with English chapters in these categories."
                                : "Titles in the chosen categories"
                            color: wrap.resultsError.length > 0 ? Theme.error : Theme.textMuted
                            font.pixelSize: Theme.bodySize
                        }
                        Flow {
                            width: parent.width
                            spacing: Theme.gap + 8

                            Repeater {
                                model: wrap.resultModel

                                SearchResultCard {
                                    required property string sourceId
                                    required property string mangaId
                                    required title
                                    required coverUrl
                                    required subtitle
                                    width: Theme.coverWidth
                                    onClicked: root.mangaOpened(sourceId, mangaId)
                                }
                            }
                        }
                        ThemedButton {
                            anchors.horizontalCenter: parent.horizontalCenter
                            visible: wrap.canLoadMore || (wrap.resultsLoading && wrap.results.length > 0)
                            text: wrap.resultsLoading ? "Loading…" : "Load more"
                            enabled: wrap.canLoadMore
                            onClicked: wrap.loadMore()
                        }
                    }
                }
            }

            SearchView {
                id: searchView
                objectName: "searchView"
                onMangaOpened: (sourceId, mangaId) => root.mangaOpened(sourceId, mangaId)
            }
        }
    }
}

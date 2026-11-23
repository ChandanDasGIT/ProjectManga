import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Novels and books: choose a source, browse its list or search it, or add a novel site by its
// address. Saved books appear in the Library.
Item {
    id: root
    signal mangaOpened(string sourceId, string mangaId)

    onVisibleChanged: if (visible) wrap.start()
    Component.onCompleted: if (visible) wrap.start()

    NovelsViewWrap {
        id: wrap
        onSiteAdded: addDialog.close()
    }

    // In the title row; in a compact window on a row of its own.
    ThemedTextField {
        id: searchField
        objectName: "novelSearch"
        Layout.preferredWidth: 300
        Layout.fillWidth: Theme.compact
        placeholderText: "Search " + wrap.sourceName
        EnterKey.type: Qt.EnterKeySearch           // a phone keyboard's action key searches
        clearable: true
        onAccepted: wrap.search(text)
        onCleared: wrap.search("")
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.margin
        spacing: 16

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gap

            Text {
                text: "Novels"
                color: Theme.text
                font.pixelSize: Theme.titleSize
                font.weight: Font.DemiBold
            }
            Item { Layout.fillWidth: true }
            LayoutItemProxy { target: searchField; visible: !Theme.compact }
            ThemedButton {
                objectName: "addNovelSite"
                text: "Add novel site"
                onClicked: addDialog.open()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: Theme.compact
            LayoutItemProxy { target: searchField }
        }

        // Wraps onto more lines when the chips do not fit.
        Flow {
            Layout.fillWidth: true
            spacing: 8

            Text {
                height: 30
                verticalAlignment: Text.AlignVCenter
                rightPadding: 4
                text: "Source"
                color: Theme.textMuted
                font.pixelSize: Theme.smallSize + 1
            }
            Repeater {
                model: wrap.sources

                Rectangle {
                    id: chip
                    required property var modelData
                    objectName: "source_" + modelData.id
                    implicitWidth: chipText.implicitWidth + 24
                    implicitHeight: 30
                    radius: height / 2
                    color: modelData.selected ? Theme.accent : chipHover.hovered ? Theme.cardHover : Theme.cardBg
                    border.width: modelData.selected ? 0 : 1
                    border.color: Theme.edge

                    HoverHandler { id: chipHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: wrap.setSource(chip.modelData.id) }

                    Text {
                        id: chipText
                        anchors.centerIn: parent
                        text: chip.modelData.name
                        color: chip.modelData.selected ? Theme.accentText : Theme.text
                        font.pixelSize: Theme.smallSize + 1
                        font.weight: chip.modelData.selected ? Font.DemiBold : Font.Normal
                    }
                }
            }
        }

        Text {
            text: wrap.listTitle
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
                visible: wrap.results.length > 0
                // Compact: two columns that share the width.
                readonly property int cardWidth: Theme.compact ? Math.floor(width / 2) - 2 * Theme.gap : Theme.coverWidth
                cellWidth: cardWidth + 2 * Theme.gap
                cellHeight: Math.round(cardWidth * Theme.coverHeight / Theme.coverWidth) + 58 + 2 * Theme.gap
                model: wrap.resultModel
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}

                delegate: NovelCard {
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
                    height: wrap.canLoadMore || (wrap.loading && wrap.results.length > 0) ? 72 : 0
                    visible: height > 0

                    ThemedButton {
                        anchors.centerIn: parent
                        text: wrap.loading ? "Loading…" : "Load more"
                        enabled: wrap.canLoadMore
                        onClicked: wrap.loadMore()
                    }
                }
            }

            Text {
                anchors.centerIn: parent
                width: Math.min(parent.width, 440)
                visible: wrap.results.length === 0
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                text: wrap.emptyText
                color: wrap.errorText.length > 0 && !wrap.loading ? Theme.error : Theme.textMuted
                font.pixelSize: Theme.bodySize
            }
        }
    }

    AddSiteDialog {
        id: addDialog
        objectName: "addNovelSiteDialog"
        heading: "Add novel site"
        description: "For a site with novel chapters. The app reads the chapter list from this page, shows each chapter's text and checks the page daily for new chapters. Sites that block apps or build their pages with JavaScript will not work."
        busy: wrap.adding
        errorText: wrap.addError
        onOpened: wrap.clearAddError()
        onAddRequested: (name, address) => wrap.addSite(name, address)
    }
}

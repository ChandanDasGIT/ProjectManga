import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The saved manga and sites as a grid of covers, with a kind filter (all, manga, novels), a title
// filter and a sort order; "Sync" opens the sync panel and "Add site" the shortcut dialog. Books
// already started open where reading stopped; right-click or a long press on a card asks to remove it.
Item {
    id: root
    readonly property var entries: wrap.entries
    signal searchRequested()
    signal mangaOpened(string sourceId, string mangaId)
    signal resumeRequested(string sourceId, string mangaId)

    LibraryViewWrap {
        id: wrap
        onSiteAdded: addSiteDialog.close()
    }

    // Search and sort sit in the title row; in a compact window they get a row of their own.
    ThemedTextField {
        id: librarySearch
        objectName: "librarySearch"
        Layout.preferredWidth: 260
        Layout.fillWidth: Theme.compact
        visible: wrap.totalCount > 0
        placeholderText: "Search your library"
        clearable: true
        onTextChanged: wrap.filterText = text
    }
    ThemedComboBox {
        id: librarySort
        objectName: "librarySort"
        visible: wrap.totalCount > 0
        model: wrap.sortOptions
        textRole: "label"
        valueRole: "key"
        currentIndex: Math.max(0, model.findIndex(option => option.key === wrap.sortKey))
        onActivated: wrap.sortKey = currentValue
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.margin
        spacing: 18

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gap

            Text {
                text: "Library"
                color: Theme.text
                font.pixelSize: Theme.titleSize
                font.weight: Font.DemiBold
            }
            Text {
                Layout.alignment: Qt.AlignBaseline
                visible: wrap.totalCount > 0
                text: wrap.kindCount + (wrap.kindCount === 1 ? " title" : " titles")
                color: Theme.textMuted
                font.pixelSize: Theme.bodySize
            }
            Item { Layout.fillWidth: true }
            LayoutItemProxy { target: librarySearch; visible: !Theme.compact }
            LayoutItemProxy { target: librarySort; visible: !Theme.compact }
            ThemedButton {
                objectName: "syncButton"
                text: "Sync"
                onClicked: syncDialog.open()
            }
            ThemedButton {
                objectName: "addSiteButton"
                text: "Add site"
                onClicked: addSiteDialog.open()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: Theme.compact && wrap.totalCount > 0
            spacing: Theme.gap

            LayoutItemProxy { target: librarySearch }
            LayoutItemProxy { target: librarySort }
        }

        Row {
            visible: wrap.totalCount > 0
            spacing: 8

            Repeater {
                model: [{ key: "all", name: "All" }, { key: "manga", name: "Manga" }, { key: "novel", name: "Novels" }]

                Rectangle {
                    id: kindChip
                    required property var modelData
                    readonly property bool selected: wrap.kindKey === modelData.key
                    objectName: "kind_" + modelData.key
                    implicitWidth: kindText.implicitWidth + 24
                    implicitHeight: 30
                    radius: height / 2
                    color: selected ? Theme.accent : kindHover.hovered ? Theme.cardHover : Theme.cardBg
                    border.width: selected ? 0 : 1
                    border.color: Theme.edge

                    HoverHandler { id: kindHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: wrap.kindKey = kindChip.modelData.key }

                    Text {
                        id: kindText
                        anchors.centerIn: parent
                        text: kindChip.modelData.name
                        color: kindChip.selected ? Theme.accentText : Theme.text
                        font.pixelSize: Theme.smallSize + 1
                        font.weight: kindChip.selected ? Font.DemiBold : Font.Normal
                    }
                }
            }
        }

        Text {
            Layout.fillWidth: true
            visible: wrap.libraryProblem.length > 0
            text: wrap.libraryProblem
            color: Theme.error
            font.pixelSize: Theme.bodySize
            wrapMode: Text.Wrap
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            GridView {
                id: grid
                // Compact: two columns that share the width.
                readonly property int cardWidth: Theme.compact ? Math.floor(width / 2) - 2 * Theme.gap : Theme.coverWidth
                anchors.fill: parent
                clip: true
                visible: root.entries.length > 0
                cellWidth: cardWidth + 2 * Theme.gap
                cellHeight: Math.round(cardWidth * Theme.coverHeight / Theme.coverWidth) + 76 + 2 * Theme.gap
                model: root.entries
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}

                delegate: LibraryCard {
                    required property var modelData
                    width: grid.cardWidth
                    title: modelData.title
                    coverUrl: modelData.coverUrl
                    sourceName: modelData.sourceName
                    unreadCount: modelData.unreadCount
                    nextEstimate: modelData.nextEstimate
                    started: modelData.started
                    progress: modelData.progress
                    onClicked: modelData.started ? root.resumeRequested(modelData.sourceId, modelData.mangaId)
                                                 : root.mangaOpened(modelData.sourceId, modelData.mangaId)
                    onDetailsClicked: root.mangaOpened(modelData.sourceId, modelData.mangaId)
                    onRemoveRequested: removeDialog.ask(modelData.sourceId, modelData.mangaId, modelData.title)
                }
            }

            Text {
                anchors.centerIn: parent
                width: Math.min(parent.width, 420)
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                visible: root.entries.length === 0 && wrap.totalCount > 0
                text: wrap.filterText.trim().length > 0
                      ? "Nothing in your library matches “" + wrap.filterText.trim() + "”."
                      : wrap.kindKey === "novel" ? "No novels in your library." : "No manga in your library."
                color: Theme.textMuted
                font.pixelSize: Theme.bodySize
            }

            Column {
                anchors.centerIn: parent
                width: Math.min(parent.width, 420)
                spacing: 8
                visible: wrap.totalCount === 0

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: "Your library is empty"
                    color: Theme.text
                    font.pixelSize: Theme.headingSize
                    font.weight: Font.DemiBold
                }
                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    text: "Add manga from Discover, or add a site you read on. New chapters show up here as a badge."
                    color: Theme.textMuted
                    font.pixelSize: Theme.bodySize
                }
                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    topPadding: 10
                    spacing: Theme.gap

                    ThemedButton {
                        text: "Search manga"
                        accent: true
                        onClicked: root.searchRequested()
                    }
                    ThemedButton {
                        text: "Add site"
                        onClicked: addSiteDialog.open()
                    }
                }
            }
        }
    }

    AddSiteDialog {
        id: addSiteDialog
        objectName: "addSiteDialog"
        busy: wrap.busy
        errorText: wrap.errorText
        onOpened: wrap.clearError()
        onAddRequested: (name, address) => wrap.addSite(name, address)
    }

    SyncDialog {
        id: syncDialog
        objectName: "syncDialog"
    }

    Popup {
        id: removeDialog
        objectName: "removeDialog"
        property string sourceId: ""
        property string mangaId: ""
        property string title: ""

        function ask(source, manga, name) {
            sourceId = source
            mangaId = manga
            title = name
            open()
        }

        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(440, (parent ? parent.width : 440) - 2 * Theme.margin)
        modal: true
        focus: true
        padding: 24
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        Overlay.modal: Rectangle { color: Theme.overlay }

        background: Rectangle {
            radius: Theme.radius
            color: Theme.panelBg
            border.width: 1
            border.color: Theme.edge
        }

        contentItem: ColumnLayout {
            spacing: Theme.gap

            Text {
                Layout.fillWidth: true
                text: "Remove “" + removeDialog.title + "” from your library?"
                color: Theme.text
                font.pixelSize: Theme.headingSize
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
            }
            Text {
                Layout.fillWidth: true
                text: "Its reading progress goes too."
                color: Theme.textMuted
                font.pixelSize: Theme.bodySize
                wrapMode: Text.Wrap
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 8
                spacing: Theme.gap

                Item { Layout.fillWidth: true }
                ThemedButton {
                    objectName: "removeConfirm"
                    text: "Remove"
                    accent: true
                    onClicked: {
                        wrap.remove(removeDialog.sourceId, removeDialog.mangaId)
                        removeDialog.close()
                    }
                }
                ThemedButton {
                    text: "Cancel"
                    onClicked: removeDialog.close()
                }
            }
        }
    }

    // The data on the server was deleted on another device: ask at once.
    Connections {
        target: AppCoreWrap
        function onSyncChanged() {
            if (AppCoreWrap.syncWipeQuestion && !syncDialog.opened) syncDialog.open()
        }
    }
}

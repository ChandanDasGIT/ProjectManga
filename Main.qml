import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Window {
    id: window
    width: 1280
    height: 820
    minimumWidth: 760
    minimumHeight: 520
    visible: true
    title: "ProjectManga"
    color: Theme.windowBg

    property int currentTab: 0                     // 0 Library, 1 Discover, 2 Novels
    property bool searchOpen: false
    readonly property int newChapterCount: AppCoreWrap.newChapterCount
    readonly property bool discoverShown: currentTab === 1 && pages.depth === 1
    // The manga reader asks for the whole screen; the window goes back as it was after.
    readonly property bool readerFullScreen: !!pages.currentItem && pages.currentItem.fullScreen === true
    property int windowedVisibility: Window.Windowed
    onReaderFullScreenChanged: {
        if (readerFullScreen) {
            windowedVisibility = window.visibility === Window.Maximized ? Window.Maximized : Window.Windowed
            window.visibility = Window.FullScreen
        } else if (window.visibility === Window.FullScreen) {
            window.visibility = windowedVisibility
        }
    }
    // Left full screen some other way (the desktop's own key): the reader shows its bars again.
    onVisibilityChanged: if (readerFullScreen && window.visibility !== Window.FullScreen) pages.currentItem.fullScreen = false

    function openManga(sourceId, mangaId) {
        pages.push(detailsPage, { sourceId: sourceId, mangaId: mangaId })
    }
    // A book already started: straight to where reading stopped (Back returns to the library).
    function resumeManga(sourceId, mangaId) {
        pages.push(detailsPage, { sourceId: sourceId, mangaId: mangaId, resume: true })
    }
    function openSearch() {
        currentTab = 1
        pages.pop(null)
        searchOpen = true
        topSearch.forceActiveFocus()
    }
    function closeSearch() {
        searchOpen = false
        discoverView.closeSearch()
    }

    Binding { target: Theme; property: "compact"; value: window.width < 600 }

    // Android's Back: one step back, as the page's own Back button does; on the first page it
    // closes the app as usual.
    function goBack() {
        const page = pages.currentItem
        if (page && page.fullScreen === true) page.fullScreen = false
        else if (pages.depth > 1) typeof page.leave === "function" ? page.leave() : pages.pop()
        else if (searchOpen) closeSearch()
        else return false
        return true
    }
    onClosing: close => { if (Qt.platform.os === "android" && goBack()) close.accepted = false }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.barHeight
            visible: !window.readerFullScreen
            color: Theme.barBg

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.margin
                anchors.rightMargin: Theme.margin
                spacing: Theme.compact ? 4 : 32

                Text {
                    visible: !Theme.compact
                    text: "ProjectManga"
                    color: Theme.text
                    font.pixelSize: Theme.headingSize
                    font.weight: Font.Bold
                }

                Row {
                    Layout.fillHeight: true
                    visible: !(Theme.compact && topSearch.visible)
                    spacing: 4

                    Repeater {
                        model: ["Library", "Discover", "Novels"]

                        delegate: Item {
                            id: tab
                            required property string modelData
                            required property int index
                            readonly property bool active: window.currentTab === index && pages.depth === 1
                            objectName: "tab" + modelData
                            width: tabRow.implicitWidth + 28
                            height: parent.height

                            HoverHandler { id: tabHover; cursorShape: Qt.PointingHandCursor }
                            TapHandler {
                                onTapped: {
                                    window.currentTab = tab.index
                                    pages.pop(null)
                                }
                            }

                            Row {
                                id: tabRow
                                anchors.centerIn: parent
                                spacing: 8

                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: tab.modelData
                                    color: tab.active || tabHover.hovered ? Theme.text : Theme.textMuted
                                    font.pixelSize: Theme.bodySize
                                    font.weight: tab.active ? Font.DemiBold : Font.Normal
                                }
                                Rectangle {
                                    anchors.verticalCenter: parent.verticalCenter
                                    visible: tab.modelData === "Library" && window.newChapterCount > 0
                                    width: Math.max(20, countText.implicitWidth + 10)
                                    height: 20
                                    radius: 10
                                    color: Theme.accent

                                    Text {
                                        id: countText
                                        anchors.centerIn: parent
                                        text: window.newChapterCount > 99 ? "99+" : window.newChapterCount
                                        color: Theme.accentText
                                        font.pixelSize: Theme.smallSize
                                        font.weight: Font.DemiBold
                                    }
                                }
                            }

                            Rectangle {
                                anchors.bottom: parent.bottom
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: parent.width - 16
                                height: 2
                                radius: 1
                                color: Theme.accent
                                visible: tab.active
                            }
                        }
                    }
                }

                Item {
                    Layout.fillWidth: true
                    visible: !(Theme.compact && topSearch.visible)
                }

                ThemedTextField {
                    id: topSearch
                    objectName: "topSearch"
                    Layout.preferredWidth: 340
                    Layout.fillWidth: Theme.compact
                    visible: window.discoverShown && window.searchOpen
                    placeholderText: "Search MangaDex by title"
                    EnterKey.type: Qt.EnterKeySearch       // a phone keyboard's action key searches
                    clearable: true
                    onAccepted: if (text.trim().length > 0) discoverView.search(text.trim())
                    Keys.onEscapePressed: window.closeSearch()
                }

                Item {
                    objectName: "searchIcon"
                    Layout.preferredWidth: 38
                    Layout.preferredHeight: 38
                    visible: window.discoverShown

                    HoverHandler { id: searchHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: window.searchOpen ? window.closeSearch() : window.openSearch() }

                    Rectangle {
                        anchors.fill: parent
                        radius: Theme.radiusSmall
                        color: searchHover.hovered ? Theme.cardHover : "transparent"
                    }
                    // A magnifier: a ring and a short handle at its lower right.
                    Item {
                        readonly property color ink: window.searchOpen ? Theme.accent
                                                     : searchHover.hovered ? Theme.text : Theme.textMuted
                        id: magnifier
                        anchors.centerIn: parent
                        width: 18
                        height: 18

                        Rectangle {
                            width: 13
                            height: 13
                            radius: 6.5
                            color: "transparent"
                            border.width: 2
                            border.color: magnifier.ink
                        }
                        Rectangle {
                            x: 11
                            y: 10.5
                            width: 2.5
                            height: 7
                            radius: 1.25
                            rotation: -45
                            transformOrigin: Item.Top
                            color: magnifier.ink
                        }
                    }
                }
            }

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.edge
            }
        }

        StackView {
            id: pages
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            pushEnter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 120 } }
            pushExit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 120 } }
            popEnter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 120 } }
            popExit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 120 } }

            initialItem: StackLayout {
                currentIndex: window.currentTab

                LibraryView {
                    objectName: "libraryView"
                    onSearchRequested: window.openSearch()
                    onMangaOpened: (sourceId, mangaId) => window.openManga(sourceId, mangaId)
                    onResumeRequested: (sourceId, mangaId) => window.resumeManga(sourceId, mangaId)
                }
                DiscoverView {
                    id: discoverView
                    objectName: "discoverView"
                    onMangaOpened: (sourceId, mangaId) => window.openManga(sourceId, mangaId)
                }
                NovelsView {
                    objectName: "novelsView"
                    onMangaOpened: (sourceId, mangaId) => window.openManga(sourceId, mangaId)
                }
            }
        }
    }

    Component {
        id: detailsPage

        MangaDetails {
            id: details
            objectName: "mangaDetails"
            onBackRequested: pages.pop()
            onChapterOpened: chapterId => {
                const reader = AppCoreWrap.isNovelSource(details.sourceId) ? novelReaderPage : readerPage
                const properties = {
                    sourceId: details.sourceId,
                    mangaId: details.mangaId,
                    chapterId: chapterId,
                    mangaTitle: details.title
                }
                if (details.resume) pages.replace(details, reader, properties, StackView.Immediate)
                else pages.push(reader, properties)
            }
        }
    }

    Component {
        id: novelReaderPage

        NovelReaderView {
            objectName: "novelReaderView"
            onBackRequested: pages.pop()
        }
    }

    Component {
        id: readerPage

        ReaderView {
            objectName: "readerView"
            onBackRequested: pages.pop()
        }
    }
}

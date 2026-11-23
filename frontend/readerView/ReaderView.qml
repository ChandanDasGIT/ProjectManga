import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Reads one chapter. A bar on top names it, picks how pages show, zooms them and moves to the
// chapter before or after. Scroll: the chapter top to bottom, Left and Right change chapter and
// Page Up / Page Down scroll a screen at a time. Fit page and Flip (see PagedView): a page or two
// at a time, turned with Left / Right, Page Up / Page Down or Space. The page reached is kept,
// and reaching the end marks the chapter read. Full screen (its button or F11; Esc leaves it)
// hides the bars; the mouse at the top edge brings the reader's bar back for a moment.
Item {
    id: root
    required property string sourceId
    required property string mangaId
    required property string chapterId
    required property string mangaTitle
    signal backRequested()

    // The window in full screen, the bars hidden (Main switches the window).
    property bool fullScreen: false
    property bool barShown: false
    onFullScreenChanged: barShown = false
    function revealBar() {
        barShown = true
        barHide.restart()
    }

    readonly property string mode: AppCoreWrap.mangaReaderMode
    readonly property bool paged: mode !== "scroll"
    property real zoom: 1.0
    readonly property real minZoom: 0.5
    readonly property real maxZoom: 3.0
    readonly property int pageWidth: Math.round(Math.min(pageList.width, 960) * zoom)

    // The page to show once the chapter's pages are there; -1: none. While pages above it are still
    // loading (and changing height) it is held in place, until the reader scrolls or a moment passes.
    property int holdIndex: -1

    function topIndex() {
        const i = pageList.indexAt(pageList.contentX + pageList.width / 2, pageList.contentY + 1)
        return i < 0 ? 0 : i
    }
    function fraction() {
        if (paged) return pagedView.fraction()
        if (pageList.count === 0) return 0
        if (pageList.atYEnd) return 1
        return topIndex() / pageList.count
    }
    function hold() {
        if (holdIndex < 0 || pageList.count === 0) return
        pageList.positionViewAtIndex(Math.min(holdIndex, pageList.count - 1), ListView.Beginning)
    }
    // A kept place waits for this chapter's pages.
    property bool waiting: false
    function startHold() {
        if (!waiting || wrap.pages.length === 0) return
        waiting = false
        // The kept fraction was of this many pages: the page it names.
        if (paged) {
            // Once the page view has the new pages too.
            const f = wrap.startFraction
            Qt.callLater(() => pagedView.goToFraction(f))
            return
        }
        holdIndex = Math.min(wrap.pages.length - 1, Math.floor(wrap.startFraction * wrap.pages.length + 0.001))
        Qt.callLater(hold)
        holdTimer.restart()
    }
    function release() {
        waiting = false
        holdIndex = -1
        holdTimer.stop()
    }
    function leave() {
        fullScreen = false
        if (holdIndex < 0) wrap.savePosition(fraction())
        root.backRequested()
    }

    function setZoom(value) {
        const next = Math.max(minZoom, Math.min(maxZoom, Math.round(value * 4) / 4))
        if (next === zoom) return
        if (paged) {
            zoom = next
            return
        }
        const fraction = pageList.contentHeight > 0 ? (pageList.contentY - pageList.originY) / pageList.contentHeight : 0
        zoom = next
        Qt.callLater(() => pageList.contentY = pageList.originY + fraction * pageList.contentHeight)
    }

    // One screen down (or up), keeping a strip of the last screen in view.
    function scrollScreen(direction) {
        root.release()
        const step = Math.max(40, pageList.height - 60) * direction
        const top = pageList.originY
        const bottom = pageList.originY + Math.max(0, pageList.contentHeight - pageList.height)
        const from = scroll.running ? scroll.to : pageList.contentY
        scroll.stop()
        scroll.to = Math.max(top, Math.min(bottom, from + step))
        scroll.start()
    }

    focus: true
    Component.onCompleted: forceActiveFocus()
    // Shows the same place in another mode.
    function setMode(next) {
        if (next === mode) return
        const f = fraction()
        const n = wrap.pages.length
        release()
        AppCoreWrap.mangaReaderMode = next
        if (n === 0) return
        if (paged) pagedView.goToFraction(f)
        else {
            holdIndex = f >= 1 ? n - 1 : Math.min(n - 1, Math.floor(f * n + 0.001))
            Qt.callLater(hold)
            holdTimer.restart()
        }
    }

    Keys.onLeftPressed: paged ? pagedView.back() : wrap.previous()
    Keys.onRightPressed: paged ? pagedView.forward() : wrap.next()
    Keys.onEscapePressed: root.fullScreen ? root.fullScreen = false : root.leave()
    Keys.onPressed: event => {
        if (event.key === Qt.Key_F11) {
            root.fullScreen = !root.fullScreen
            event.accepted = true
        } else if (paged && (event.key === Qt.Key_PageDown || event.key === Qt.Key_Space)) {
            pagedView.forward()
            event.accepted = true
        } else if (paged && event.key === Qt.Key_PageUp) {
            pagedView.back()
            event.accepted = true
        } else if (event.key === Qt.Key_PageDown) {
            root.scrollScreen(1)
            event.accepted = true
        } else if (event.key === Qt.Key_PageUp) {
            root.scrollScreen(-1)
            event.accepted = true
        }
    }

    NumberAnimation {
        id: scroll
        target: pageList
        property: "contentY"
        duration: 160
        easing.type: Easing.OutCubic
    }

    ReaderViewWrap {
        id: wrap
        Component.onCompleted: open(root.sourceId, root.mangaId, root.chapterId)
        onChapterChanged: {
            root.release()
            pageList.positionViewAtBeginning()
            pagedView.reset()
            root.waiting = wrap.startFraction > 0
            root.startHold()
        }
        onPagesChanged: root.startHold()
    }

    Timer {
        id: holdTimer
        interval: 3000
        onTriggered: root.holdIndex = -1
    }

    // The bar goes again a moment after the mouse leaves it.
    Timer {
        id: barHide
        interval: 2000
        onTriggered: {
            if (barHover.hovered || modeMenu.popup.visible || speedMenu.popup.visible) restart()
            else root.barShown = false
        }
    }

    // Saves the page reached a moment after scrolling ends.
    Timer {
        id: savePosition
        interval: 1200
        onTriggered: wrap.savePosition(root.fraction())
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // The bar's place; in full screen the bar slides in over the pages instead.
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: root.fullScreen ? 0 : bar.height
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: pageList
                objectName: "pageList"
                anchors.fill: parent
                clip: true
                visible: !root.paged
                model: root.paged ? [] : wrap.pages
                cacheBuffer: 6000
                boundsBehavior: Flickable.StopAtBounds
                contentWidth: Math.max(width, root.pageWidth)
                flickableDirection: Flickable.AutoFlickIfNeeded
                ScrollBar.vertical: ScrollBar {}
                ScrollBar.horizontal: ScrollBar { policy: root.pageWidth > pageList.width ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff }

                delegate: Item {
                    required property string modelData
                    required property int index
                    width: Math.max(pageList.width, root.pageWidth)
                    height: page.height

                    PageImage {
                        id: page
                        width: root.pageWidth
                        anchors.horizontalCenter: parent.horizontalCenter
                        source: parent.modelData
                        pageNumber: parent.index + 1
                    }
                }

                footer: Item {
                    width: pageList.contentWidth
                    height: wrap.pages.length > 0 ? 180 : 0
                    visible: height > 0

                    Column {
                        anchors.centerIn: parent
                        spacing: 14

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "End of " + wrap.chapterLabel
                            color: Theme.textMuted
                            font.pixelSize: Theme.bodySize
                        }
                        ThemedButton {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: wrap.hasNext ? "Next chapter ›" : "Back to chapters"
                            accent: true
                            focusPolicy: Qt.NoFocus
                            onClicked: wrap.hasNext ? wrap.next() : root.leave()
                        }
                    }
                }

                onAtYEndChanged: if (atYEnd && count > 0 && contentHeight > height && root.holdIndex < 0) wrap.finished()
                onContentYChanged: if (count > 0 && root.holdIndex < 0) savePosition.restart()
                onContentHeightChanged: if (root.holdIndex >= 0) root.hold()
                onMovementStarted: root.release()
            }

            PagedView {
                id: pagedView
                objectName: "pagedView"
                anchors.fill: parent
                visible: root.paged
                pages: root.paged ? wrap.pages : []
                flip: root.mode === "flip"
                turnDuration: ({ slow: 1400, normal: 850, fast: 500, veryFast: 300 })[AppCoreWrap.flipSpeed] ?? 850
                zoom: root.zoom
                chapterLabel: wrap.chapterLabel
                hasNext: wrap.hasNext
                hasPrevious: wrap.hasPrevious
                onNextChapter: wrap.next()
                onPreviousChapter: wrap.previousAtEnd()
                onLastPageReached: wrap.finished()
                onBackToChapters: root.leave()
                onMoved: savePosition.restart()
            }

            Column {
                anchors.centerIn: parent
                width: Math.min(parent.width - 2 * Theme.margin, 440)
                spacing: 14
                visible: wrap.pages.length === 0

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    text: wrap.loading ? "Loading pages…"
                        : wrap.external ? "This chapter is published on an official site."
                        : wrap.errorText
                    color: !wrap.loading && !wrap.external ? Theme.error : Theme.textMuted
                    font.pixelSize: Theme.bodySize
                }
                ThemedButton {
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: !wrap.loading && (wrap.external || wrap.errorText.length > 0)
                    text: wrap.external ? "Open official site ↗" : "Try again"
                    accent: wrap.external
                    focusPolicy: Qt.NoFocus
                    onClicked: wrap.external ? Qt.openUrlExternally(wrap.externalUrl) : wrap.reload()
                }
            }
        }
    }

    // In full screen the mouse at the top edge brings the bar back.
    Item {
        objectName: "barReveal"
        visible: root.fullScreen
        width: parent.width
        height: 6
        z: 11
        HoverHandler {
            onHoveredChanged: if (hovered) root.revealBar()
        }
    }

    Rectangle {
        id: bar
        objectName: "readerBar"
        width: parent.width
        height: Theme.compact ? compactRows.implicitHeight + 16 : 52
        z: 10
        y: !root.fullScreen || root.barShown ? 0 : -height - 1
        color: Theme.barBg
        Behavior on y { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

        HoverHandler { id: barHover }

        // The controls sit in one row; a compact window stacks them in three.
        ThemedButton {
            id: backButton
            objectName: "readerBack"
            text: "‹ Back"
            focusPolicy: Qt.NoFocus
            onClicked: root.leave()
        }
        Text {
            id: titleText
            Layout.fillWidth: true
            Layout.leftMargin: 6
            text: root.mangaTitle + (wrap.chapterLabel.length > 0 ? "  ·  " + wrap.chapterLabel : "")
            color: Theme.text
            font.pixelSize: Theme.bodySize
            font.weight: Font.Medium
            elide: Text.ElideRight
        }
        Row {
            id: zoomButtons
            spacing: 4

            ThemedButton {
                objectName: "zoomOut"
                text: "−"
                focusPolicy: Qt.NoFocus
                enabled: root.zoom > root.minZoom
                onClicked: root.setZoom(root.zoom - 0.25)
            }
            ThemedButton {
                objectName: "zoomReset"
                width: 64
                leftPadding: 6
                rightPadding: 6
                text: Math.round(root.zoom * 100) + "%"
                focusPolicy: Qt.NoFocus
                onClicked: root.setZoom(1.0)
            }
            ThemedButton {
                objectName: "zoomIn"
                text: "+"
                focusPolicy: Qt.NoFocus
                enabled: root.zoom < root.maxZoom
                onClicked: root.setZoom(root.zoom + 0.25)
            }
        }
        ThemedComboBox {
            id: modeMenu
            objectName: "readerMode"
            implicitWidth: 132
            focusPolicy: Qt.NoFocus
            textRole: "label"
            valueRole: "key"
            model: [
                { label: "Scroll", key: "scroll" },
                { label: "Fit page", key: "page" },
                { label: "Flip book", key: "flip" }
            ]
            currentIndex: Math.max(0, model.findIndex(option => option.key === root.mode))
            onActivated: {
                root.setMode(currentValue)
                root.forceActiveFocus()
            }
        }
        ThemedComboBox {
            id: speedMenu
            objectName: "flipSpeed"
            visible: root.paged
            implicitWidth: 116
            focusPolicy: Qt.NoFocus
            textRole: "label"
            valueRole: "key"
            model: [
                { label: "Slow", key: "slow" },
                { label: "Normal", key: "normal" },
                { label: "Fast", key: "fast" },
                { label: "Very fast", key: "veryFast" }
            ]
            currentIndex: Math.max(0, model.findIndex(option => option.key === AppCoreWrap.flipSpeed))
            onActivated: {
                AppCoreWrap.flipSpeed = currentValue
                root.forceActiveFocus()
            }
        }
        ThemedButton {
            id: previousButton
            text: "‹ Previous"
            focusPolicy: Qt.NoFocus
            enabled: wrap.hasPrevious
            onClicked: wrap.previous()
        }
        ThemedButton {
            id: nextButton
            text: "Next ›"
            focusPolicy: Qt.NoFocus
            enabled: wrap.hasNext
            onClicked: wrap.next()
        }
        ThemedButton {
            id: fullScreenButton
            objectName: "fullScreen"
            text: root.fullScreen ? "Exit full screen" : "Full screen"
            focusPolicy: Qt.NoFocus
            onClicked: root.fullScreen = !root.fullScreen
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Theme.margin
            anchors.rightMargin: Theme.margin
            visible: !Theme.compact
            spacing: Theme.gap

            LayoutItemProxy { target: backButton }
            LayoutItemProxy { target: titleText }
            LayoutItemProxy { target: zoomButtons }
            Item { width: 8 }
            LayoutItemProxy { target: modeMenu }
            LayoutItemProxy { target: speedMenu }
            Item { width: 8 }
            LayoutItemProxy { target: previousButton }
            LayoutItemProxy { target: nextButton }
            LayoutItemProxy { target: fullScreenButton }
        }

        ColumnLayout {
            id: compactRows
            anchors.fill: parent
            anchors.margins: 8
            anchors.leftMargin: Theme.margin
            anchors.rightMargin: Theme.margin
            visible: Theme.compact
            spacing: 8

            RowLayout {
                spacing: Theme.gap
                LayoutItemProxy { target: backButton }
                LayoutItemProxy { target: titleText }
                LayoutItemProxy { target: fullScreenButton }
            }
            RowLayout {
                spacing: Theme.gap
                LayoutItemProxy { target: modeMenu }
                LayoutItemProxy { target: speedMenu }
                Item { Layout.fillWidth: true }
            }
            RowLayout {
                spacing: Theme.gap
                LayoutItemProxy { target: zoomButtons }
                Item { Layout.fillWidth: true }
                LayoutItemProxy { target: previousButton }
                LayoutItemProxy { target: nextButton }
            }
        }
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: Theme.edge
        }
    }
}

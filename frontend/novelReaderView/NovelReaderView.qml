import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Reads one chapter of a novel as text in a centred column, in the chosen size and page colours.
// Left / Right move between chapters, Page Up / Page Down scroll a screen; where reading stopped
// is kept, and reaching the end marks the chapter read.
Item {
    id: root
    required property string sourceId
    required property string mangaId
    required property string chapterId
    required property string mangaTitle
    signal backRequested()

    readonly property color pageColor: AppCoreWrap.readerTheme === "sepia" ? Theme.readerSepiaBg
                                     : AppCoreWrap.readerTheme === "light" ? Theme.readerLightBg : Theme.readerDarkBg
    readonly property color inkColor: AppCoreWrap.readerTheme === "sepia" ? Theme.readerSepiaText
                                    : AppCoreWrap.readerTheme === "light" ? Theme.readerLightText : Theme.readerDarkText

    // Where to scroll once the new chapter's text is laid out; -1: nothing waiting. While a
    // position waits, scrolling is not saved (it would overwrite the saved place with the top).
    property real pendingFraction: -1

    function restore() {
        if (pendingFraction < 0) return
        const range = page.contentHeight - page.height
        if (pendingFraction > 0 && range <= 0) return            // not laid out yet
        page.contentY = Math.max(0, pendingFraction * range)
        pendingFraction = -1
    }
    function fraction() {
        const range = page.contentHeight - page.height
        return range > 0 ? Math.max(0, Math.min(1, page.contentY / range)) : 0
    }
    function scrollScreen(direction) {
        const bottom = Math.max(0, page.contentHeight - page.height)
        const from = scroll.running ? scroll.to : page.contentY
        scroll.stop()
        scroll.to = Math.max(0, Math.min(bottom, from + Math.max(40, page.height - 60) * direction))
        scroll.start()
    }
    function leave() {
        wrap.savePosition(fraction())
        root.backRequested()
    }

    focus: true
    Component.onCompleted: forceActiveFocus()
    Keys.onLeftPressed: wrap.previous()
    Keys.onRightPressed: wrap.next()
    Keys.onEscapePressed: root.leave()
    Keys.onPressed: event => {
        if (event.key === Qt.Key_PageDown) { root.scrollScreen(1); event.accepted = true }
        else if (event.key === Qt.Key_PageUp) { root.scrollScreen(-1); event.accepted = true }
    }

    NovelReaderViewWrap {
        id: wrap
        Component.onCompleted: open(root.sourceId, root.mangaId, root.chapterId)
        onTextChanged: {
            scroll.stop()
            root.pendingFraction = wrap.startFraction
            page.contentY = 0
            Qt.callLater(root.restore)
        }
    }

    NumberAnimation {
        id: scroll
        target: page
        property: "contentY"
        duration: 160
        easing.type: Easing.OutCubic
    }

    // Saves where reading stopped a moment after scrolling ends.
    Timer {
        id: savePosition
        interval: 1200
        onTriggered: wrap.savePosition(root.fraction())
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.compact ? compactRows.implicitHeight + 16 : 52
            color: Theme.barBg

            // The controls sit in one row; a compact window puts Previous and Next in a second one.

            ThemedButton {
                id: backButton
                objectName: "novelBack"
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
            ThemedButton {
                id: textButton
                objectName: "textSettings"
                text: "Aa"
                focusPolicy: Qt.NoFocus
                onClicked: textSettings.opened ? textSettings.close() : textSettings.open()

                ReaderTextSettings {
                    id: textSettings
                    objectName: "textSettingsPopup"
                    x: textButton.width - width
                    y: textButton.height + 8
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

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.margin
                anchors.rightMargin: Theme.margin
                visible: !Theme.compact
                spacing: Theme.gap

                LayoutItemProxy { target: backButton }
                LayoutItemProxy { target: titleText }
                LayoutItemProxy { target: textButton }
                Item { width: 8 }
                LayoutItemProxy { target: previousButton }
                LayoutItemProxy { target: nextButton }
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
                    LayoutItemProxy { target: textButton }
                }
                RowLayout {
                    spacing: Theme.gap
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

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: root.pageColor

            Flickable {
                id: page
                objectName: "novelPage"
                anchors.fill: parent
                clip: true
                contentWidth: width
                contentHeight: column.implicitHeight
                boundsBehavior: Flickable.StopAtBounds
                visible: wrap.paragraphs.length > 0
                ScrollBar.vertical: ScrollBar {}

                onContentYChanged: if (wrap.paragraphs.length > 0 && root.pendingFraction < 0) savePosition.restart()
                onContentHeightChanged: if (root.pendingFraction >= 0) Qt.callLater(root.restore)
                onAtYEndChanged: if (atYEnd && contentHeight > height) wrap.finished()

                Column {
                    id: column
                    width: Math.min(AppCoreWrap.readerWidth, page.width - 2 * Theme.margin)
                    x: Math.round((page.width - width) / 2)
                    topPadding: 40
                    bottomPadding: 24
                    spacing: Math.round(AppCoreWrap.readerFontSize * 0.75)

                    Text {
                        width: parent.width
                        bottomPadding: 12
                        text: wrap.heading.length > 0 ? wrap.heading : wrap.chapterLabel
                        color: root.inkColor
                        font.pixelSize: Math.round(AppCoreWrap.readerFontSize * 1.45)
                        font.weight: Font.DemiBold
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                    }
                    Repeater {
                        model: wrap.paragraphs

                        Text {
                            required property string modelData
                            width: column.width
                            text: modelData
                            color: root.inkColor
                            font.pixelSize: AppCoreWrap.readerFontSize
                            lineHeight: AppCoreWrap.readerLineSpacing
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
                        }
                    }
                    Item {
                        width: parent.width
                        height: 160

                        Column {
                            anchors.centerIn: parent
                            spacing: 14

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "End of " + wrap.chapterLabel
                                color: root.inkColor
                                opacity: 0.6
                                font.pixelSize: Theme.bodySize
                            }
                            Row {
                                anchors.horizontalCenter: parent.horizontalCenter
                                spacing: Theme.gap

                                ThemedButton {
                                    text: "‹ Previous chapter"
                                    visible: wrap.hasPrevious
                                    focusPolicy: Qt.NoFocus
                                    onClicked: wrap.previous()
                                }
                                ThemedButton {
                                    text: wrap.hasNext ? "Next chapter ›" : "Back to chapters"
                                    accent: true
                                    focusPolicy: Qt.NoFocus
                                    onClicked: wrap.hasNext ? wrap.next() : root.leave()
                                }
                            }
                        }
                    }
                }
            }

            Column {
                anchors.centerIn: parent
                width: Math.min(parent.width - 2 * Theme.margin, 440)
                spacing: 14
                visible: wrap.paragraphs.length === 0

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    text: wrap.loading ? "Loading chapter…" : wrap.errorText
                    color: wrap.loading ? root.inkColor : Theme.error
                    font.pixelSize: Theme.bodySize
                }
                ThemedButton {
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: !wrap.loading && wrap.errorText.length > 0
                    text: "Try again"
                    focusPolicy: Qt.NoFocus
                    onClicked: wrap.reload()
                }
            }
        }
    }
}

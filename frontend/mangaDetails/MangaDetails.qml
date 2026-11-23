import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// One manga: cover, title, the library button and description on the left (scrolling as one, so a
// short window still reaches the buttons), the chapter list (newest first) on the right; in a
// compact window all of it in one list, the chapters last.
Item {
    id: root
    required property string sourceId
    required property string mangaId
    readonly property string title: wrap.title
    // Opened to resume reading: once the chapters are in, the chapter to continue opens by itself,
    // and the page stays covered until then (or until it turns out there is none, or an error).
    property bool resume: false
    property bool resumeStarted: false
    readonly property bool resuming: resume && !resumeStarted && wrap.chapters.length === 0 && wrap.errorText.length === 0
    signal backRequested()
    signal chapterOpened(string chapterId)

    MangaDetailsWrap {
        id: wrap
        Component.onCompleted: open(root.sourceId, root.mangaId)
        onContinueChapterIdChanged: {
            if (!root.resume || root.resumeStarted || wrap.continueChapterId.length === 0) return
            root.resumeStarted = true
            // After the wrapper has handed the chapter list on to the reader.
            Qt.callLater(() => root.chapterOpened(wrap.continueChapterId))
        }
    }

    // Shared by both layouts: the wide one (info column beside the chapters) and the compact one
    // (everything in one list that scrolls).
    Component {
        id: infoBlock

        ColumnLayout {
            spacing: 10

            CoverImage {
                Layout.preferredWidth: 220
                Layout.preferredHeight: Math.round(220 * Theme.coverHeight / Theme.coverWidth)
                source: wrap.coverUrl
            }
            Text {
                Layout.fillWidth: true
                Layout.topMargin: 6
                text: wrap.title.length > 0 ? wrap.title : (wrap.loading ? "Loading…" : "")
                color: Theme.text
                font.pixelSize: Theme.titleSize - 2
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
            }
            Text {
                Layout.fillWidth: true
                visible: text.length > 0
                text: wrap.subtitle
                color: Theme.textMuted
                font.pixelSize: Theme.bodySize
                wrapMode: Text.Wrap
            }
            Text {
                text: wrap.sourceName
                color: Theme.textFaint
                font.pixelSize: Theme.smallSize
            }

            Flow {
                Layout.fillWidth: true
                Layout.topMargin: 6
                spacing: Theme.gap

                ThemedButton {
                    objectName: "readButton"
                    text: wrap.continueLabel
                    accent: true
                    enabled: wrap.continueChapterId.length > 0
                    onClicked: root.chapterOpened(wrap.continueChapterId)
                }
                ThemedButton {
                    objectName: "libraryButton"
                    // A touch screen has no hover to show what a tap does.
                    text: !wrap.inLibrary ? "Add to library"
                          : hovered || Qt.platform.os === "android" ? "Remove from library" : "In library ✓"
                    enabled: wrap.title.length > 0
                    onClicked: wrap.toggleLibrary()
                }
            }
        }
    }

    Component {
        id: chaptersHeading

        RowLayout {
            spacing: Theme.gap

            Text {
                text: "Chapters"
                color: Theme.text
                font.pixelSize: Theme.headingSize
                font.weight: Font.DemiBold
            }
            Text {
                visible: wrap.chapters.length > 0
                text: wrap.chapters.length
                color: Theme.textMuted
                font.pixelSize: Theme.bodySize
            }
            Item { Layout.fillWidth: true }
            ThemedButton {
                objectName: "chapterOrder"
                visible: wrap.chapters.length > 1
                leftPadding: 12
                rightPadding: 12
                text: wrap.newestFirst ? "Newest first ↓" : "Oldest first ↑"
                onClicked: AppCoreWrap.chapterOrder = wrap.newestFirst ? "oldest" : "newest"
            }
        }
    }

    Component {
        id: chapterRow

        ChapterRow {
            required property var modelData
            width: ListView.view.width - 12
            label: modelData.label
            group: modelData.group
            date: modelData.date
            read: modelData.read
            isNew: modelData.isNew
            external: modelData.external
            onClicked: modelData.external ? Qt.openUrlExternally(modelData.externalUrl)
                                          : root.chapterOpened(modelData.id)
        }
    }

    // While there are no chapters: loading, the error with "Try again", or that there are none.
    Component {
        id: noChapters

        Column {
            spacing: 12

            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                text: wrap.loading ? "Loading chapters…"
                    : wrap.errorText.length > 0 ? wrap.errorText
                    : "No English chapters here yet."
                color: wrap.errorText.length > 0 && !wrap.loading ? Theme.error : Theme.textMuted
                font.pixelSize: Theme.bodySize
            }
            ThemedButton {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: !wrap.loading && wrap.errorText.length > 0
                text: "Try again"
                onClicked: wrap.reload()
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.margin
        spacing: 18

        ThemedButton {
            objectName: "detailsBack"
            text: "‹ Back"
            onClicked: root.backRequested()
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !Theme.compact
            spacing: 36

            Flickable {
                id: infoScroll
                objectName: "infoScroll"
                Layout.preferredWidth: 280
                Layout.maximumWidth: 280
                Layout.fillHeight: true
                clip: true
                contentHeight: infoColumn.implicitHeight
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}

                ColumnLayout {
                    id: infoColumn
                    width: infoScroll.width - 8
                    spacing: 10

                    Loader {
                        Layout.fillWidth: true
                        sourceComponent: Theme.compact ? null : infoBlock
                    }

                    Text {
                        Layout.fillWidth: true
                        Layout.topMargin: 6
                        text: wrap.description
                        color: Theme.textMuted
                        font.pixelSize: Theme.bodySize
                        lineHeight: 1.25
                        wrapMode: Text.Wrap
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 10

                Loader {
                    Layout.fillWidth: true
                    sourceComponent: Theme.compact ? null : chaptersHeading
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ListView {
                        id: chapterList
                        objectName: "chapterList"
                        anchors.fill: parent
                        clip: true
                        model: Theme.compact ? [] : wrap.chapters
                        boundsBehavior: Flickable.StopAtBounds
                        ScrollBar.vertical: ScrollBar {}
                        delegate: chapterRow
                    }

                    Loader {
                        anchors.centerIn: parent
                        width: Math.min(parent.width, 420)
                        active: wrap.chapters.length === 0 && !Theme.compact
                        sourceComponent: noChapters
                    }
                }
            }
        }

        // Compact: info, description and chapter heading scroll away above the chapters.
        ListView {
            id: compactList
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: Theme.compact
            clip: true
            model: Theme.compact ? wrap.chapters : []
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
            delegate: chapterRow

            header: ColumnLayout {
                width: compactList.width
                spacing: 10

                Loader {
                    Layout.fillWidth: true
                    sourceComponent: infoBlock
                }
                Text {
                    Layout.fillWidth: true
                    Layout.topMargin: 6
                    visible: text.length > 0
                    text: wrap.description
                    color: Theme.textMuted
                    font.pixelSize: Theme.bodySize
                    lineHeight: 1.25
                    wrapMode: Text.Wrap
                }
                Loader {
                    Layout.fillWidth: true
                    Layout.topMargin: 12
                    Layout.bottomMargin: 4
                    sourceComponent: chaptersHeading
                }
                Loader {
                    Layout.fillWidth: true
                    Layout.topMargin: 24
                    active: wrap.chapters.length === 0
                    visible: active
                    sourceComponent: noChapters
                }
            }
        }
    }

    Rectangle {
        objectName: "resumeCover"
        anchors.fill: parent
        visible: root.resuming
        color: Theme.windowBg

        HoverHandler {}
        TapHandler {}

        Text {
            anchors.centerIn: parent
            text: "Opening where you left off…"
            color: Theme.textMuted
            font.pixelSize: Theme.bodySize
        }
    }
}

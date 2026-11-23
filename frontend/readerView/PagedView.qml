import QtQuick
import QtQuick.Controls

// A chapter one page at a time, fitted to the view; pages curl over as they turn, by themselves
// or dragged by a corner, and lie flat between turns. With `flip` it is a book: two pages side by
// side (the first alone on the right, like a cover; one page when the view is too narrow).
// Turning past the last page opens the next chapter (or shows the end), turning back past the
// first opens the one before.
Item {
    id: root
    property var pages: []
    property bool flip: false
    // How long a whole page turn takes, in ms.
    property int turnDuration: 850
    property real zoom: 1.0
    property string chapterLabel
    property bool hasNext: false
    property bool hasPrevious: false
    signal nextChapter()
    signal previousChapter()
    signal lastPageReached()
    signal backToChapters()
    signal moved()

    readonly property int count: pages.length
    // The page shown (in a spread: either of its two); `count` is the end screen.
    property int current: 0
    readonly property bool atEnd: count > 0 && current >= count
    readonly property bool spread: flip && count > 1 && width >= height
    readonly property bool zoomed: zoom > 1.0
    readonly property real margin: 12
    readonly property real availWidth: Math.max(100, width - 2 * margin)
    readonly property real availHeight: Math.max(100, height - 2 * margin)

    // ---- spreads: 0 is [ -, 0 ], then [ 1, 2 ], [ 3, 4 ] ...
    function spreadOf(p) { return p <= 0 ? 0 : Math.floor((p + 1) / 2) }
    function leftOf(s) { return s === 0 ? -1 : 2 * s - 1 }
    function rightOf(s) { return 2 * s }
    function firstOf(s) { return s === 0 ? 0 : 2 * s - 1 }
    readonly property int shownPage: Math.max(0, Math.min(current, count - 1))
    readonly property int currentSpread: spreadOf(shownPage)
    function firstVisible() { return spread ? firstOf(currentSpread) : shownPage }
    function lastVisible() { return spread ? Math.min(count - 1, rightOf(currentSpread)) : shownPage }

    // Page shapes, kept as the images come in.
    property var aspects: ({})
    property int aspectsVersion: 0
    function noteAspect(i, a) {
        if (i < 0 || aspects[i] === a) return
        aspects[i] = a
        aspectsVersion++
    }
    function aspectOf(i) {
        aspectsVersion
        const a = aspects[i]
        return a === undefined ? 1 / 1.42 : a
    }
    function urlOf(i) { return i >= 0 && i < count ? pages[i] : "" }

    function fraction() {
        if (count === 0) return 0
        if (atEnd) return 1
        return firstVisible() / count
    }
    function goToFraction(f) {
        stopTurn()
        if (count === 0) { current = 0; return }
        current = Math.max(0, Math.min(count - 1, Math.floor(f * count + 0.001)))
    }
    function reset() {
        stopTurn()
        current = 0
        aspects = ({})
        aspectsVersion++
    }

    function arrived() {
        if (!atEnd && lastVisible() >= count - 1) root.lastPageReached()
        root.moved()
    }

    function forward() {
        if (count === 0 || dragging) return
        if (turning) skipTurn()
        if (atEnd) {
            if (hasNext) root.nextChapter()
            return
        }
        if (lastVisible() >= count - 1) {
            root.lastPageReached()
            if (hasNext) root.nextChapter()
            else {
                current = count
                root.moved()
            }
            return
        }
        startTurn(1, spread ? firstOf(currentSpread + 1) : shownPage + 1, false)
    }

    function back() {
        if (count === 0 || dragging) return
        if (turning) skipTurn()
        if (atEnd) {
            current = count - 1
            root.moved()
            return
        }
        if (firstVisible() <= 0) {
            if (hasPrevious) root.previousChapter()
            return
        }
        const target = spread ? firstOf(currentSpread - 1) : shownPage - 1
        startTurn(-1, target, false)
    }

    // ---- turning a page over: the paper curls up from a corner (see pageCurl.frag), the corner
    // following a point that moves from where it lies to where it lands, mirrored over the hinge.
    property bool turning: false
    property int turnDirection: 0          // 1 forward, -1 back
    property int turnTarget: 0
    property bool dragging: false
    // During a turn: what lies under the turning page, and its two sides.
    property int underLeft: -1
    property int underRight: -1
    property int underSingle: -1
    property int leafFront: -1
    property int leafBack: -1

    // The turning page laid flat, in the book's coordinates. Two pages: it hinges on the spine;
    // one page: on its left edge, going off to the left and coming back from there.
    readonly property real curlWidth: spread ? (turnDirection > 0 ? rightWidth : leftWidth) : singleWidth
    readonly property real curlHeight: curlWidth / aspectOf(leafFront)
    readonly property real curlTop: (bookHeight - curlHeight) / 2
    readonly property real hingeX: spread ? bookWidth / 2 : 0
    readonly property int curlSide: spread ? turnDirection : 1            // where the free edge is
    readonly property real freeX: hingeX + curlSide * curlWidth
    readonly property real landX: hingeX - curlSide * curlWidth
    // One page going back starts turned over and lays itself down.
    readonly property bool unturning: !spread && turnDirection < 0
    property real cornerY: 0
    property real pointX: 0
    property real pointY: 0

    // The path the corner point follows to finish (or undo) a turn.
    property real fromX: 0
    property real fromY: 0
    property real toX: 0
    property real toY: 0
    property real pathLift: 0
    property real pathT: 0
    property bool pathCompletes: true
    onPathTChanged: {
        if (!turning) return
        pointX = fromX + (toX - fromX) * pathT
        pointY = fromY + (toY - fromY) * pathT - pathLift * Math.sin(Math.PI * pathT)
    }

    function startTurn(direction, target, byDrag, grabY) {
        turnDirection = direction
        turnTarget = target
        if (spread) {
            const s = currentSpread
            const t = spreadOf(target)
            underLeft = direction > 0 ? leftOf(s) : leftOf(t)
            underRight = direction > 0 ? rightOf(t) : rightOf(s)
            leafFront = direction > 0 ? rightOf(s) : leftOf(s)
            leafBack = direction > 0 ? leftOf(t) : rightOf(t)
        } else {
            underSingle = direction > 0 ? target : shownPage
            leafFront = direction > 0 ? shownPage : target
            leafBack = -1
        }
        // A corner grabbed near the top or bottom curls from there; the middle pulls straight.
        const bottom = curlTop + curlHeight
        if (grabY === undefined) cornerY = bottom
        else {
            const at = (grabY - curlTop) / curlHeight
            cornerY = at > 0.66 ? bottom : at < 0.33 ? curlTop : grabY
        }
        pointX = unturning ? landX : freeX
        pointY = cornerY
        turning = true
        dragging = byDrag
        if (!byDrag) runTurn(true, curlHeight * 0.22)
    }
    // How far a drag has taken the page, 0 .. 1.
    function turnedFraction() {
        const start = unturning ? landX : freeX
        const end = unturning ? freeX : landX
        return Math.max(0, Math.min(1, (pointX - start) / (end - start)))
    }
    function runTurn(complete, lift) {
        dragging = false
        turnAnimation.stop()
        pathCompletes = complete
        fromX = pointX
        fromY = pointY
        toX = complete !== unturning ? landX : freeX
        toY = cornerY
        pathLift = lift
        pathT = 0
        turnAnimation.duration = Math.max(120, turnDuration * Math.abs(toX - fromX) / (2 * curlWidth))
        turnAnimation.start()
    }
    function finishTurn() {
        const complete = pathCompletes
        turning = false
        dragging = false
        if (complete) {
            current = turnTarget
            arrived()
        }
    }
    // A key pressed during a turn: that turn ends at once and the next one starts.
    function skipTurn() {
        turnAnimation.stop()
        pathT = 1
        finishTurn()
    }
    function stopTurn() {
        turnAnimation.stop()
        turning = false
        dragging = false
    }

    NumberAnimation {
        id: turnAnimation
        target: root
        property: "pathT"
        from: 0
        to: 1
        easing.type: Easing.InOutSine
        onFinished: root.finishTurn()
    }

    // ---- what is on screen
    readonly property int idleLeft: spread ? leftOf(currentSpread) : -1
    readonly property int idleRight: spread ? rightOf(currentSpread) : -1
    readonly property int shownLeft: turning && spread ? underLeft : idleLeft
    readonly property int shownRight: turning && spread ? underRight : idleRight
    readonly property int shownSingle: turning && !spread ? underSingle : (spread ? -1 : shownPage)

    // The spread's size: both pages the same height, the spine in the middle.
    readonly property real leftAspect: aspectOf(idleLeft >= 0 ? idleLeft : idleRight)
    readonly property real rightAspect: aspectOf(idleRight < count ? idleRight : idleLeft)
    readonly property real spreadHeight: Math.min(availHeight, availWidth / (leftAspect + rightAspect)) * zoom
    readonly property real leftWidth: spreadHeight * leftAspect
    readonly property real rightWidth: spreadHeight * rightAspect
    readonly property real singleWidth: Math.min(availWidth, availHeight * aspectOf(shownPage)) * zoom
    readonly property real singleHeight: singleWidth / aspectOf(shownPage)

    readonly property real bookWidth: spread ? 2 * Math.max(leftWidth, rightWidth) : singleWidth
    readonly property real bookHeight: spread ? spreadHeight : singleHeight

    component Leaf: PageImage {
        property int page: -1
        source: root.urlOf(page)
        pageNumber: page + 1
        visible: page >= 0 && page < root.count
        onAspectChanged: root.noteAspect(page, aspect)
        onPageChanged: root.noteAspect(page, aspect)
    }

    Flickable {
        id: viewport
        objectName: "pagedViewport"
        anchors.fill: parent
        clip: true
        visible: !root.atEnd
        contentWidth: Math.max(width, root.bookWidth + 2 * root.margin)
        contentHeight: Math.max(height, root.bookHeight + 2 * root.margin)
        interactive: root.zoomed
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: viewport.contentHeight > viewport.height ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff }
        ScrollBar.horizontal: ScrollBar { policy: viewport.contentWidth > viewport.width ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff }

        Item {
            id: book
            objectName: "book"
            width: root.bookWidth
            height: root.bookHeight
            x: (viewport.contentWidth - width) / 2
            y: (viewport.contentHeight - height) / 2
            readonly property real spine: width / 2

            // ---- two pages
            Leaf {
                id: leftPage
                page: root.shownLeft
                width: root.leftWidth
                x: book.spine - width
                anchors.verticalCenter: parent.verticalCenter
                visible: root.spread && page >= 0 && page < root.count
            }
            Leaf {
                id: rightPage
                page: root.shownRight
                width: root.rightWidth
                x: book.spine
                anchors.verticalCenter: parent.verticalCenter
                visible: root.spread && page >= 0 && page < root.count
            }
            // A black line where the two pages meet; a turning page passes over it.
            Rectangle {
                objectName: "pageSeparator"
                visible: root.spread && root.shownLeft >= 0 && root.shownRight < root.count
                z: 4
                x: book.spine - width / 2
                width: 2
                height: book.height
                color: "black"
            }

            // ---- one page
            Leaf {
                id: singlePage
                objectName: "singlePage"
                page: root.shownSingle
                width: root.singleWidth
                anchors.centerIn: parent
                visible: !root.spread && page >= 0 && page < root.count
            }

            // ---- the turning page
            // Its two sides, drawn only through the curl.
            Leaf {
                id: curlFront
                page: root.turning ? root.leafFront : -1
                width: root.curlWidth
            }
            Leaf {
                id: curlBack
                page: root.turning ? root.leafBack : -1
                width: root.curlWidth
            }
            ShaderEffectSource {
                id: frontTexture
                sourceItem: curlFront
                hideSource: true
                visible: false
            }
            ShaderEffectSource {
                id: backTexture
                sourceItem: curlBack
                hideSource: true
                visible: false
            }

            ShaderEffect {
                id: curl
                objectName: "pageCurl"
                visible: root.turning
                z: 5
                // Room around the page for the paper as it lifts and lands.
                readonly property real marginX: root.curlWidth * 0.1
                readonly property real marginY: root.curlHeight * 0.35
                x: root.hingeX - root.curlWidth - marginX
                y: root.curlTop - marginY
                width: 2 * (root.curlWidth + marginX)
                height: root.curlHeight + 2 * marginY

                // The paper stays bound at the spine: the corner can be no further from either end
                // of the spine than it was when the page lay flat.
                readonly property point held: {
                    let px = root.pointX
                    let py = root.pointY
                    const ends = [root.curlTop, root.curlTop + root.curlHeight]
                    for (const ey of ends) {
                        const reach = Math.sqrt(root.curlWidth * root.curlWidth + (root.cornerY - ey) * (root.cornerY - ey))
                        const dx = px - root.hingeX
                        const dy = py - ey
                        const dist = Math.sqrt(dx * dx + dy * dy)
                        if (dist > reach) {
                            px = root.hingeX + dx * reach / dist
                            py = ey + dy * reach / dist
                        }
                    }
                    return Qt.point(px, py)
                }
                readonly property real pullX: root.freeX - held.x
                readonly property real pullY: root.cornerY - held.y
                readonly property real pull: Math.sqrt(pullX * pullX + pullY * pullY)
                readonly property bool lifted: pull > 0.5

                property vector2d areaSize: Qt.vector2d(width, height)
                property vector4d pageRect: Qt.vector4d(root.hingeX - (root.curlSide < 0 ? root.curlWidth : 0) - x,
                                                        root.curlTop - y, root.curlWidth, root.curlHeight)
                // The fold halfway between the corner and where it is now, square to the pull.
                property vector2d foldPoint: lifted
                    ? Qt.vector2d((root.freeX + held.x) / 2 - x, (root.cornerY + held.y) / 2 - y)
                    : Qt.vector2d(root.freeX + root.curlSide * 10000 - x, 0)
                property vector2d foldNormal: lifted ? Qt.vector2d(pullX / pull, pullY / pull) : Qt.vector2d(root.curlSide, 0)
                // Rolls widest halfway over, flat again where it lands.
                property real radius: root.curlWidth * 0.14 * Math.sin(Math.PI * Math.min(1, pull / (2 * root.curlWidth)))
                property real hasBack: root.leafBack >= 0 ? 1 : 0
                property color paper: "#e9e6df"
                property variant front: frontTexture
                property variant back: root.leafBack >= 0 ? backTexture : frontTexture
                fragmentShader: Qt.resolvedUrl("pageCurl.frag.qsb")
            }
        }

        // Clicks on the left or right half turn back or forward; a page dragged from its outer
        // part turns with the pointer. A zoomed page is panned instead.
        MouseArea {
            id: input
            objectName: "pagedInput"
            width: viewport.contentWidth
            height: viewport.contentHeight
            enabled: !root.zoomed
            property real pressX: 0
            property real pressY: 0
            property bool moved: false
            property real wheelSum: 0
            // Where the grabbed point started, in the book's coordinates.
            property real grabX: 0
            property real grabY: 0

            onPressed: mouse => {
                pressX = mouse.x
                pressY = mouse.y
                moved = false
            }
            onPositionChanged: mouse => {
                const dx = mouse.x - pressX
                if (!root.turning && !moved && Math.abs(dx) > 12) {
                    moved = true
                    if (root.count === 0 || root.atEnd) return
                    const pageW = root.spread ? root.rightWidth : root.singleWidth
                    const y = pressY - book.y
                    if (dx < 0 && pressX > book.x + book.width - pageW * 0.6 && root.lastVisible() < root.count - 1)
                        root.startTurn(1, root.spread ? root.firstOf(root.currentSpread + 1) : root.shownPage + 1, true, y)
                    else if (dx > 0 && pressX < book.x + (root.spread ? pageW * 0.6 + book.spine - root.leftWidth : pageW * 0.6) && root.firstVisible() > 0)
                        root.startTurn(-1, root.spread ? root.firstOf(root.currentSpread - 1) : root.shownPage - 1, true, y)
                    grabX = root.pointX
                    grabY = root.pointY
                }
                if (root.dragging) {
                    const lo = Math.min(root.freeX, root.landX)
                    const hi = Math.max(root.freeX, root.landX)
                    root.pointX = Math.max(lo, Math.min(hi, grabX + mouse.x - pressX))
                    root.pointY = Math.max(root.curlTop - root.curlHeight * 0.3,
                                           Math.min(root.curlTop + root.curlHeight * 1.3, grabY + mouse.y - pressY))
                }
            }
            onReleased: mouse => {
                if (root.dragging) {
                    root.runTurn(root.turnedFraction() > 0.3, 0)
                    return
                }
                if (moved) return
                if (mouse.x < viewport.contentWidth / 2) root.back()
                else root.forward()
            }
            onWheel: wheel => {
                if (root.turning || wheelPause.running) {
                    wheelSum = 0
                    return
                }
                wheelSum += wheel.angleDelta.y
                if (Math.abs(wheelSum) < 120) return
                const down = wheelSum < 0
                wheelSum = 0
                wheelPause.restart()
                if (down) root.forward()
                else root.back()
            }
        }
    }

    // Keeps a flick of the wheel to one turn.
    Timer {
        id: wheelPause
        interval: 280
    }

    // The next few pages load before they are needed.
    Repeater {
        model: root.count > 0 ? 4 : 0
        delegate: Leaf {
            required property int index
            readonly property int ahead: (root.spread ? root.rightOf(root.currentSpread) : root.shownPage) + 1 + index
            visible: false
            width: 1
            page: ahead < root.count ? ahead : -1
        }
    }

    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 10
        visible: root.count > 0 && !root.atEnd
        width: counter.implicitWidth + 20
        height: counter.implicitHeight + 8
        radius: height / 2
        color: Theme.overlay

        Text {
            id: counter
            objectName: "pageCounter"
            anchors.centerIn: parent
            text: {
                const first = root.firstVisible() + 1
                const last = root.lastVisible() + 1
                return (first === last ? first : first + "–" + last) + " / " + root.count
            }
            color: Theme.text
            font.pixelSize: Theme.smallSize
        }
    }

    Column {
        anchors.centerIn: parent
        spacing: 14
        visible: root.atEnd

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "End of " + root.chapterLabel
            color: Theme.textMuted
            font.pixelSize: Theme.bodySize
        }
        ThemedButton {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.hasNext ? "Next chapter ›" : "Back to chapters"
            accent: true
            focusPolicy: Qt.NoFocus
            onClicked: root.hasNext ? root.nextChapter() : root.backToChapters()
        }
    }
}

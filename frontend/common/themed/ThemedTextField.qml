import QtQuick
import QtQuick.Controls

// A one-line text field in the app's style; the border lights up in the accent while typing.
// clearable: a × at the right end empties the field (only the text; `cleared` follows).
TextField {
    id: root
    property bool clearable: false
    readonly property bool showsClear: clearable && text.length > 0
    signal cleared()

    implicitHeight: Theme.controlHeight
    leftPadding: 12
    rightPadding: showsClear ? 34 : 12
    font.pixelSize: Theme.bodySize
    color: Theme.text
    placeholderTextColor: Theme.textFaint
    selectionColor: Theme.accent
    selectedTextColor: Theme.accentText

    background: Rectangle {
        radius: Theme.radiusSmall
        color: Theme.fieldBg
        border.width: 1
        border.color: root.activeFocus ? Theme.accent : Theme.edge
    }

    Item {
        objectName: "clearButton"
        visible: root.showsClear
        width: 30
        height: parent.height
        anchors.right: parent.right
        anchors.rightMargin: 2

        HoverHandler { id: clearHover; cursorShape: Qt.PointingHandCursor }
        TapHandler {
            onTapped: {
                root.clear()
                root.forceActiveFocus()
                root.cleared()
            }
        }

        Text {
            anchors.centerIn: parent
            text: "×"
            color: clearHover.hovered ? Theme.text : Theme.textMuted
            font.pixelSize: 20
        }
    }
}

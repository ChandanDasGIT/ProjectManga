import QtQuick
import QtQuick.Controls

// A button in the app's style. `accent: true` marks the main action of a view or dialog.
Button {
    id: root
    property bool accent: false

    implicitHeight: Theme.controlHeight
    leftPadding: 16
    rightPadding: 16
    hoverEnabled: true
    opacity: enabled ? 1 : 0.45
    font.pixelSize: Theme.bodySize
    font.weight: accent ? Font.DemiBold : Font.Normal

    contentItem: Text {
        text: root.text
        font: root.font
        color: root.accent ? Theme.accentText : Theme.text
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: Theme.radiusSmall
        color: root.accent
               ? (root.down ? Theme.accentDown : root.hovered ? Theme.accentHover : Theme.accent)
               : (root.down || root.hovered ? Theme.cardHover : Theme.cardBg)
        border.width: root.accent ? 0 : 1
        border.color: root.visualFocus ? Theme.accent : Theme.edge
    }
}

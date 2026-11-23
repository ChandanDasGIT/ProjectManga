import QtQuick
import QtQuick.Controls

// A drop-down list in the app's style. Give it a model of objects with textRole / valueRole.
ComboBox {
    id: root

    implicitHeight: Theme.controlHeight
    implicitWidth: 190
    leftPadding: 12
    rightPadding: 30
    hoverEnabled: true
    font.pixelSize: Theme.bodySize

    contentItem: Text {
        text: root.displayText
        font: root.font
        color: Theme.text
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: ThemedTriangle {
        x: root.width - width - 14
        anchors.verticalCenter: parent.verticalCenter
        anchors.verticalCenterOffset: -size * 0.1
        color: Theme.textMuted
        size: root.font.pixelSize
    }

    background: Rectangle {
        radius: Theme.radiusSmall
        color: root.hovered || root.down ? Theme.cardHover : Theme.cardBg
        border.width: 1
        border.color: root.visualFocus || root.popup.visible ? Theme.accent : Theme.edge
    }

    delegate: ItemDelegate {
        id: option
        required property var model
        required property int index
        width: ListView.view ? ListView.view.width : root.width
        height: Theme.controlHeight
        hoverEnabled: true
        highlighted: root.highlightedIndex === index

        contentItem: Text {
            text: option.model[root.textRole]
            color: root.currentIndex === option.index ? Theme.accent : Theme.text
            font.pixelSize: Theme.bodySize
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle {
            radius: Theme.radiusSmall - 2
            color: option.highlighted || option.hovered ? Theme.cardHover : "transparent"
        }
    }

    popup: Popup {
        y: root.height + 4
        width: root.width
        padding: 4
        implicitHeight: contentItem.implicitHeight + 8

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: root.popup.visible ? root.delegateModel : null
            currentIndex: root.highlightedIndex
        }

        background: Rectangle {
            radius: Theme.radiusSmall
            color: Theme.panelBg
            border.width: 1
            border.color: Theme.edge
        }
    }
}

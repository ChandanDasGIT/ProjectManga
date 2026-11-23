import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// How sync through the server is going, with Sync now and deleting this user's data there; asks
// whether to put this device's data back after it was deleted on another device.
Popup {
    id: root

    property bool confirmingDelete: false

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(520, (parent ? parent.width : 520) - 2 * Theme.margin)
    modal: true
    focus: true
    padding: 24
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    onOpened: confirmingDelete = false

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
            text: "Sync"
            color: Theme.text
            font.pixelSize: Theme.headingSize
            font.weight: Font.DemiBold
        }
        Text {
            Layout.fillWidth: true
            Layout.bottomMargin: 4
            text: "Keeps your library, the chapters you have read and where you stopped the same on all your devices."
            color: Theme.textMuted
            font.pixelSize: Theme.bodySize
            wrapMode: Text.Wrap
        }

        Text {
            objectName: "syncStatus"
            Layout.fillWidth: true
            text: AppCoreWrap.syncStatus
            color: AppCoreWrap.syncOk ? Theme.textMuted : Theme.error
            font.pixelSize: Theme.bodySize
            wrapMode: Text.Wrap
        }

        Text {
            Layout.fillWidth: true
            Layout.topMargin: 4
            visible: root.confirmingDelete
            text: "This deletes your library from the server and turns sync off on this device. Your library on this device stays. Other devices upload theirs again on their next sync unless sync is turned off there too."
            color: Theme.error
            font.pixelSize: Theme.bodySize
            wrapMode: Text.Wrap
        }

        ThemedButton {
            objectName: "syncDelete"
            Layout.topMargin: 4
            visible: !root.confirmingDelete && !AppCoreWrap.syncWipeQuestion && AppCoreWrap.syncConfigured
                     && AppCoreWrap.syncEnabled
            text: "Delete my data from server"
            onClicked: root.confirmingDelete = true
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 8
            spacing: Theme.gap
            visible: !root.confirmingDelete && !AppCoreWrap.syncWipeQuestion

            Item { Layout.fillWidth: true }
            ThemedButton {
                objectName: "syncNow"
                visible: AppCoreWrap.syncConfigured
                text: AppCoreWrap.syncEnabled ? "Sync now" : "Turn sync on"
                accent: true
                enabled: !AppCoreWrap.syncBusy
                onClicked: AppCoreWrap.syncEnabled ? AppCoreWrap.syncNow() : AppCoreWrap.setSyncEnabled(true)
            }
            ThemedButton {
                text: "Close"
                onClicked: root.close()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 8
            spacing: Theme.gap
            visible: AppCoreWrap.syncWipeQuestion

            Item { Layout.fillWidth: true }
            ThemedButton {
                objectName: "syncPutBack"
                text: "Put it back"
                accent: true
                onClicked: AppCoreWrap.answerWipe(true)
            }
            ThemedButton {
                objectName: "syncKeepOff"
                text: "Keep sync off"
                onClicked: AppCoreWrap.answerWipe(false)
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 8
            spacing: Theme.gap
            visible: root.confirmingDelete

            Item { Layout.fillWidth: true }
            ThemedButton {
                objectName: "syncDeleteConfirm"
                text: "Delete"
                accent: true
                onClicked: {
                    AppCoreWrap.deleteServerData()
                    root.confirmingDelete = false
                }
            }
            ThemedButton {
                text: "Cancel"
                onClicked: root.confirmingDelete = false
            }
        }
    }
}

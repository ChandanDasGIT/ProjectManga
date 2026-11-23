import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Adds a website to the library like a browser shortcut: a name and the address of the page that
// lists the chapters. The owner says through errorText when the page cannot be used.
Popup {
    id: root
    property string heading: "Add site"
    property string description: "Like a browser shortcut. The app reads the chapter list from this page and checks it daily for new chapters."
    property string errorText: ""
    property bool busy: false
    signal addRequested(string name, string address)

    function add() {
        if (addButton.enabled) root.addRequested(nameField.text.trim(), addressField.text.trim())
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(480, (parent ? parent.width : 480) - 2 * Theme.margin)
    modal: true
    focus: true
    padding: 24
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    onOpened: {
        nameField.text = ""
        addressField.text = ""
        nameField.forceActiveFocus()
    }

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
            text: root.heading
            color: Theme.text
            font.pixelSize: Theme.headingSize
            font.weight: Font.DemiBold
        }
        Text {
            Layout.fillWidth: true
            Layout.bottomMargin: 4
            text: root.description
            color: Theme.textMuted
            font.pixelSize: Theme.bodySize
            wrapMode: Text.Wrap
        }

        Text {
            text: "Name"
            color: Theme.textMuted
            font.pixelSize: Theme.smallSize
        }
        ThemedTextField {
            id: nameField
            Layout.fillWidth: true
            placeholderText: "As it should appear in your library"
            KeyNavigation.tab: addressField
            onAccepted: addressField.forceActiveFocus()
        }

        Text {
            Layout.topMargin: 4
            text: "Address"
            color: Theme.textMuted
            font.pixelSize: Theme.smallSize
        }
        ThemedTextField {
            id: addressField
            Layout.fillWidth: true
            placeholderText: "https://… (the page that lists the chapters)"
            inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase
            onAccepted: root.add()
        }

        Text {
            Layout.fillWidth: true
            visible: root.errorText.length > 0
            text: root.errorText
            color: Theme.error
            font.pixelSize: Theme.bodySize
            wrapMode: Text.Wrap
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 8
            spacing: Theme.gap

            Item { Layout.fillWidth: true }
            ThemedButton {
                text: "Cancel"
                onClicked: root.close()
            }
            ThemedButton {
                id: addButton
                text: root.busy ? "Reading page…" : "Add"
                accent: true
                enabled: !root.busy && nameField.text.trim().length > 0 && addressField.text.trim().length > 0
                onClicked: root.add()
            }
        }
    }
}

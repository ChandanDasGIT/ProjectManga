import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The text settings of the novel reader: size, line spacing, column width and page colours.
// They are kept between runs.
Popup {
    id: root
    padding: 18
    width: 300

    background: Rectangle {
        radius: Theme.radius
        color: Theme.panelBg
        border.width: 1
        border.color: Theme.edge
    }

    component Stepper: RowLayout {
        id: stepper
        property string label
        property string value
        signal decrease()
        signal increase()
        Layout.fillWidth: true
        spacing: 6

        Text {
            Layout.fillWidth: true
            text: stepper.label
            color: Theme.textMuted
            font.pixelSize: Theme.bodySize
        }
        ThemedButton { text: "−"; focusPolicy: Qt.NoFocus; onClicked: stepper.decrease() }
        Text {
            Layout.preferredWidth: 52
            horizontalAlignment: Text.AlignHCenter
            text: stepper.value
            color: Theme.text
            font.pixelSize: Theme.bodySize
        }
        ThemedButton { text: "+"; focusPolicy: Qt.NoFocus; onClicked: stepper.increase() }
    }

    contentItem: ColumnLayout {
        spacing: 10

        Stepper {
            objectName: "fontSizeStepper"
            label: "Text size"
            value: AppCoreWrap.readerFontSize
            onDecrease: AppCoreWrap.readerFontSize = AppCoreWrap.readerFontSize - 1
            onIncrease: AppCoreWrap.readerFontSize = AppCoreWrap.readerFontSize + 1
        }
        Stepper {
            label: "Line spacing"
            value: AppCoreWrap.readerLineSpacing.toFixed(1)
            onDecrease: AppCoreWrap.readerLineSpacing = AppCoreWrap.readerLineSpacing - 0.1
            onIncrease: AppCoreWrap.readerLineSpacing = AppCoreWrap.readerLineSpacing + 0.1
        }
        Stepper {
            label: "Width"
            value: AppCoreWrap.readerWidth
            onDecrease: AppCoreWrap.readerWidth = AppCoreWrap.readerWidth - 80
            onIncrease: AppCoreWrap.readerWidth = AppCoreWrap.readerWidth + 80
        }

        Text {
            Layout.topMargin: 4
            text: "Page"
            color: Theme.textMuted
            font.pixelSize: Theme.bodySize
        }
        RowLayout {
            spacing: 8

            Repeater {
                model: [
                    { key: "dark", label: "Dark", bg: Theme.readerDarkBg, ink: Theme.readerDarkText },
                    { key: "sepia", label: "Sepia", bg: Theme.readerSepiaBg, ink: Theme.readerSepiaText },
                    { key: "light", label: "Light", bg: Theme.readerLightBg, ink: Theme.readerLightText }
                ]

                Rectangle {
                    id: swatch
                    required property var modelData
                    objectName: "theme_" + modelData.key
                    implicitWidth: 76
                    implicitHeight: 34
                    radius: Theme.radiusSmall
                    color: modelData.bg
                    border.width: AppCoreWrap.readerTheme === modelData.key ? 2 : 1
                    border.color: AppCoreWrap.readerTheme === modelData.key ? Theme.accent : Theme.edge

                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: AppCoreWrap.readerTheme = swatch.modelData.key }

                    Text {
                        anchors.centerIn: parent
                        text: swatch.modelData.label
                        color: swatch.modelData.ink
                        font.pixelSize: Theme.smallSize + 1
                    }
                }
            }
        }
    }
}

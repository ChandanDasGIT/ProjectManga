pragma Singleton
import QtQuick

// Every colour and size of the app lives here. Use it anywhere as Theme.windowBg, Theme.accent, ...
QtObject {
    // ---- surfaces: the window is the darkest, bars and dialogs a step lighter, cards lighter still ----
    readonly property color windowBg:   "#111215"
    readonly property color barBg:      "#17181c"
    readonly property color panelBg:    "#1c1e23"
    readonly property color cardBg:     "#22252b"
    readonly property color cardHover:  "#2a2d34"
    readonly property color fieldBg:    "#141519"
    readonly property color edge:       "#2c2f36"
    readonly property color overlay:    "#a6000000"

    // ---- text ----
    readonly property color text:       "#eceef2"
    readonly property color textMuted:  "#9ba0a9"
    readonly property color textFaint:  "#6a6f78"

    // ---- accent: the main action, the active tab, focus and the "new chapters" badge ----
    readonly property color accent:      "#ff6b57"
    readonly property color accentHover: "#ff8472"
    readonly property color accentDown:  "#e55a47"
    readonly property color accentText:  "#1d0f0c"
    readonly property color error:       "#ff8a80"
    readonly property color accentBg:    "#33201c"   // behind something chosen (the model in use)

    // ---- the novel reader's page, per reading theme ----
    readonly property color readerDarkBg:    "#141518"
    readonly property color readerDarkText:  "#dcdee3"
    readonly property color readerSepiaBg:   "#f3ead6"
    readonly property color readerSepiaText: "#4a3a26"
    readonly property color readerLightBg:   "#fbfaf7"
    readonly property color readerLightText: "#1f2328"

    // ---- sizes ----
    // A phone-width window (set by Main): views stack what sits side by side on the desktop.
    property bool compact: false
    readonly property int radius:        10
    readonly property int radiusSmall:   6
    readonly property int gap:           12
    readonly property int margin:        compact ? 16 : 24
    readonly property int barHeight:     56
    readonly property int controlHeight: 38
    readonly property int coverWidth:    160
    readonly property int coverHeight:   228   // manga covers are about 1 : 1.42

    // ---- text sizes ----
    readonly property int smallSize:   12
    readonly property int bodySize:    14
    readonly property int headingSize: 17
    readonly property int titleSize:   24
}

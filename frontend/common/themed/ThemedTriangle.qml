import QtQuick
import QtQuick.Shapes

// A small filled triangle for drop-down arrows, drawn instead of the ▾ / ▴ characters: Android's
// fonts have no glyph for them. Sized like that glyph in text of `size` pixels.
Item {
    id: root
    property bool up: false
    property color color: Theme.text
    property real size: Theme.bodySize

    implicitWidth: Math.round(size * 0.46)
    implicitHeight: Math.round(size * 0.46)

    Shape {
        id: shape
        readonly property real w: root.size * 0.47
        readonly property real h: root.size * 0.36
        x: (root.width - w) / 2
        y: (root.height - h) / 2
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeWidth: -1
            fillColor: root.color
            startX: 0; startY: root.up ? shape.h : 0
            PathLine { x: shape.w; y: root.up ? shape.h : 0 }
            PathLine { x: shape.w / 2; y: root.up ? 0 : shape.h }
            PathLine { x: 0; y: root.up ? shape.h : 0 }
        }
    }
}

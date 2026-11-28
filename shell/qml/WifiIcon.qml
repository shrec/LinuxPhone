import QtQuick
import Revive.Shell

// Wi-Fi fan: a dot and three concentric quarter-circle arcs opening upward. `bars` (0-4) maps to the
// lit parts: 1 = dot, 2 = dot + 1 arc, 3 = dot + 2 arcs, 4 = dot + 3 arcs. Lit parts use the accent
// colour, unlit parts the dim colour at 0.3 opacity. `active: false` leaves everything unlit.
Item {
    id: icon

    property int bars: 0
    property bool active: true
    readonly property int litBars: active ? bars : 0

    implicitWidth: 16
    implicitHeight: 12

    onLitBarsChanged: canvas.requestPaint()

    Canvas {
        id: canvas
        anchors.fill: parent
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            const ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            const u = Math.min(width / 16, height / 12)
            const cx = width / 2
            const cy = height - 1.4 * u
            const arcStart = -0.75 * Math.PI
            const arcEnd = -0.25 * Math.PI
            ctx.lineWidth = 1.6 * u
            ctx.lineCap = "round"

            // Dot.
            const dotLit = icon.litBars >= 1
            ctx.globalAlpha = dotLit ? 1.0 : 0.3
            ctx.fillStyle = dotLit ? Theme.accent : Theme.dim
            ctx.beginPath()
            ctx.arc(cx, cy, 1.4 * u, 0, 2 * Math.PI)
            ctx.fill()

            // Three arcs, innermost first.
            for (let i = 1; i <= 3; ++i) {
                const lit = icon.litBars >= i + 1
                ctx.globalAlpha = lit ? 1.0 : 0.3
                ctx.strokeStyle = lit ? Theme.accent : Theme.dim
                ctx.beginPath()
                ctx.arc(cx, cy, (1 + 3 * i) * u, arcStart, arcEnd)
                ctx.stroke()
            }
        }
    }
}

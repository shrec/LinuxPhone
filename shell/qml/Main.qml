import QtQuick
import QtQuick.Window
import org.kde.layershell 1.0 as LayerShell
import Revive.Shell

// Non-visual root: three layer-shell windows on phoc.
QtObject {
    id: root
    // Home = the launcher is shown above the apps. phoc ignores foreign-toplevel minimize requests
    // and layer-shell-qt does not apply a layer change to a mapped surface, so the launcher lives
    // in the top layer and is mapped only while home.
    property bool home: true
    // Quick settings shade: progress 0 (hidden) .. 1 (open). It follows the finger while dragged and
    // settles with an animation on release.
    property real shade: 0
    property bool shadeOpen: false       // the state the shade is heading to / resting in
    property bool shadeDragging: false
    property real shadeBase: 0           // progress when the current drag started
    readonly property bool shadeMapped: shade > 0 || shadeDragging || shadeAnim.running
    readonly property real openThreshold: 0.35
    readonly property real flickSpeed: 800  // px/s

    property Animation shadeAnim: NumberAnimation {
        target: root
        property: "shade"
        duration: 180
        easing.type: Easing.OutCubic
    }

    function dragShadeStart() {
        shadeAnim.stop()
        shadeBase = shade
        shadeDragging = true
    }

    function dragShadeMove(dy) {
        shade = Math.max(0, Math.min(1, shadeBase + dy / Math.max(1, quickSettings.panelHeight)))
    }

    // Release: a fast flick decides by its direction, otherwise the progress does.
    function dragShadeEnd(dy, vy) {
        dragShadeMove(dy)
        shadeDragging = false
        if (vy > flickSpeed)
            settleShade(true)
        else if (vy < -flickSpeed)
            settleShade(false)
        else
            settleShade(shade > openThreshold)
    }

    function settleShade(open) {
        shadeOpen = open
        shadeAnim.stop()
        shadeAnim.from = shade
        shadeAnim.to = open ? 1 : 0
        shadeAnim.restart()
    }

    // Top bar: layer top, anchored top, reserves 28 px.
    property Window statusWindow: Window {
        height: 28
        color: Theme.panel
        LayerShell.Window.layer: LayerShell.Window.LayerTop
        // The shell never takes text. With layer-shell-qt's default (on-demand) a shell surface keeps
        // the keyboard focus, so app windows (foot) never receive wl_keyboard.enter.
        LayerShell.Window.keyboardInteractivity: LayerShell.Window.KeyboardInteractivityNone
        LayerShell.Window.anchors: LayerShell.Window.AnchorTop | LayerShell.Window.AnchorLeft | LayerShell.Window.AnchorRight
        LayerShell.Window.exclusionZone: 28
        visible: true

        StatusBar {
            anchors.fill: parent
            onToggleRequested: root.settleShade(!root.shadeOpen)
            onDragStarted: root.dragShadeStart()
            onDragMoved: (dy) => root.dragShadeMove(dy)
            onDragFinished: (dy, vy) => root.dragShadeEnd(dy, vy)
        }
    }

    // App grid: full screen, above the apps, shown only while home.
    property Window launcherWindow: Window {
        color: Theme.background
        LayerShell.Window.layer: LayerShell.Window.LayerTop
        LayerShell.Window.keyboardInteractivity: LayerShell.Window.KeyboardInteractivityNone
        LayerShell.Window.anchors: LayerShell.Window.AnchorTop | LayerShell.Window.AnchorBottom | LayerShell.Window.AnchorLeft | LayerShell.Window.AnchorRight
        visible: root.home

        Launcher {
            anchors.fill: parent
            onLaunch: (appId) => {
                root.home = false
                if (!ToplevelModel.activate(appId)) AppModel.launch(appId)
            }
        }
    }

    // Home strip: layer overlay, anchored bottom, 28 px. Swipe up goes home.
    property Window homeWindow: Window {
        height: 28
        color: "transparent"
        LayerShell.Window.layer: LayerShell.Window.LayerOverlay
        LayerShell.Window.keyboardInteractivity: LayerShell.Window.KeyboardInteractivityNone
        LayerShell.Window.anchors: LayerShell.Window.AnchorBottom | LayerShell.Window.AnchorLeft | LayerShell.Window.AnchorRight
        LayerShell.Window.exclusionZone: 28  // app windows end above the strip
        visible: true

        Rectangle {
            anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 8 }
            width: 120; height: 4; radius: 2
            color: Theme.dim
        }
        DragHandler {
            target: null  // detect the gesture only; don't move the strip
            xAxis.enabled: false
            yAxis.enabled: true
            // activeTranslation is already reset when `active` turns false, so keep the last value.
            property real lastDy: 0
            onActiveTranslationChanged: lastDy = activeTranslation.y
            onActiveChanged: if (!active && lastDy < -60) root.home = true
        }
    }

    // Quick settings shade: a full-screen overlay below the bar, mapped while it is dragged, animating
    // or open. The only shell window that takes the keyboard (on demand), so the on-screen keyboard can
    // type a Wi-Fi passphrase.
    property Window quickWindow: Window {
        color: "transparent"
        LayerShell.Window.layer: LayerShell.Window.LayerOverlay
        LayerShell.Window.keyboardInteractivity: LayerShell.Window.KeyboardInteractivityOnDemand
        LayerShell.Window.anchors: LayerShell.Window.AnchorTop | LayerShell.Window.AnchorBottom | LayerShell.Window.AnchorLeft | LayerShell.Window.AnchorRight
        LayerShell.Window.margins.top: 28  // the status bar stays visible
        visible: root.shadeMapped

        QuickSettings {
            id: quickSettings
            anchors.fill: parent
            progress: root.shade
            open: root.shadeMapped
            onCloseRequested: root.settleShade(false)
            onDragStarted: root.dragShadeStart()
            onDragMoved: (dy) => root.dragShadeMove(dy)
            onDragFinished: (dy, vy) => root.dragShadeEnd(dy, vy)
        }
    }

    // The last app window closed: nothing left to show but the launcher.
    // A new window appeared (incoming call, app started over SSH): leave home so it isn't hidden.
    property Connections toplevels: Connections {
        target: ToplevelModel
        function onRowsRemoved() { if (ToplevelModel.rowCount() === 0) root.home = true }
        function onRowsInserted() { root.home = false }
    }
}

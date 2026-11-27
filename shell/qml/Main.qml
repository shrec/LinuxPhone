import QtQuick
import QtQuick.Window
import org.kde.layershell 1.0 as LayerShell
import Revive.Shell

// Non-visual root: three layer-shell windows on phoc.
QtObject {
    // Top bar: layer top, anchored top, reserves 28 px.
    property Window statusWindow: Window {
        height: 28
        color: Theme.panel
        LayerShell.Window.layer: LayerShell.Window.LayerTop
        LayerShell.Window.anchors: LayerShell.Window.AnchorTop | LayerShell.Window.AnchorLeft | LayerShell.Window.AnchorRight
        LayerShell.Window.exclusionZone: 28
        visible: true

        StatusBar { anchors.fill: parent }
    }

    // App grid: layer background, full screen (running apps cover it).
    property Window launcherWindow: Window {
        color: Theme.background
        LayerShell.Window.layer: LayerShell.Window.LayerBackground
        LayerShell.Window.anchors: LayerShell.Window.AnchorTop | LayerShell.Window.AnchorBottom | LayerShell.Window.AnchorLeft | LayerShell.Window.AnchorRight
        visible: true

        Launcher {
            anchors.fill: parent
            onLaunch: AppModel.launch(appId)
        }
    }

    // Home strip: layer overlay, anchored bottom, 28 px. V2 wires the swipe to ToplevelModel.
    property Window homeWindow: Window {
        height: 28
        color: "transparent"
        LayerShell.Window.layer: LayerShell.Window.LayerOverlay
        LayerShell.Window.anchors: LayerShell.Window.AnchorBottom | LayerShell.Window.AnchorLeft | LayerShell.Window.AnchorRight
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
            onActiveChanged: {
                if (!active && translation.y < -60) console.log("home")
            }
        }
    }
}

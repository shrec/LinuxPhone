import QtQuick
import Revive.Shell

Rectangle {
    id: bar
    height: 28
    color: Theme.panel
    property string now: Qt.formatTime(new Date(), "hh:mm")
    Timer { interval: 1000; running: true; repeat: true; onTriggered: bar.now = Qt.formatTime(new Date(), "hh:mm") }

    // Tap toggles the quick settings shade. A downward drag pulls it down with the finger: the touch
    // stays on this window while the finger moves over the screen, so the drag is reported as a
    // total dy (px) plus the release velocity (px/s).
    signal toggleRequested()
    signal dragStarted()
    signal dragMoved(real dy)
    signal dragFinished(real dy, real vy)
    TapHandler { onTapped: bar.toggleRequested() }
    DragHandler {
        target: null  // detect the gesture only; don't move the bar
        xAxis.enabled: false
        yAxis.enabled: true
        // activeTranslation is reset when `active` turns false, so keep the last values while active.
        property real lastDy: 0
        property real lastVy: 0
        onActiveTranslationChanged: if (active) { lastDy = activeTranslation.y; bar.dragMoved(lastDy) }
        onCentroidChanged: if (active) lastVy = centroid.velocity.y
        onActiveChanged: if (active) { lastDy = 0; lastVy = 0; bar.dragStarted() } else bar.dragFinished(lastDy, lastVy)
    }

    Text {
        anchors { left: parent.left; leftMargin: 12; verticalCenter: parent.verticalCenter }
        text: "revive"; color: Theme.accent
        font { family: Theme.mono; pixelSize: 13; bold: true }
    }

    Row {
        id: indicators
        anchors { right: clock.left; rightMargin: 12; verticalCenter: parent.verticalCenter }
        spacing: 10

        // Wi-Fi: dot and arcs fan; a dim "x" when not connected.
        Row {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 4
            WifiIcon {
                id: wifi
                anchors.verticalCenter: parent.verticalCenter
                bars: StatusModel.wifiBars
                active: StatusModel.wifiConnected
            }
            Text {
                visible: !StatusModel.wifiConnected
                anchors.verticalCenter: parent.verticalCenter
                text: "x"; color: Theme.dim
                font { family: Theme.mono; pixelSize: 11 }
            }
        }

        // Battery: "87%", with a "+" in front while charging.
        Row {
            visible: StatusModel.batteryPercent >= 0
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2
            Text {
                visible: StatusModel.charging
                text: "+"; color: Theme.accent
                font { family: Theme.mono; pixelSize: 11; bold: true }
            }
            Text {
                text: StatusModel.batteryPercent + "%"; color: Theme.text
                font { family: Theme.mono; pixelSize: 13 }
            }
        }
    }

    Text {
        id: clock
        anchors { right: parent.right; rightMargin: 12; verticalCenter: parent.verticalCenter }
        text: bar.now; color: Theme.text
        font { family: Theme.mono; pixelSize: 13 }
    }
}

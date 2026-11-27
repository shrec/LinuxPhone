import QtQuick
import Revive.Shell

Rectangle {
    id: bar
    height: 28
    color: Theme.panel
    property string now: Qt.formatTime(new Date(), "hh:mm")
    Timer { interval: 1000; running: true; repeat: true; onTriggered: bar.now = Qt.formatTime(new Date(), "hh:mm") }

    Text {
        anchors { left: parent.left; leftMargin: 12; verticalCenter: parent.verticalCenter }
        text: "revive"; color: Theme.accent
        font { family: Theme.mono; pixelSize: 13; bold: true }
    }
    Text {
        anchors { right: parent.right; rightMargin: 12; verticalCenter: parent.verticalCenter }
        text: bar.now; color: Theme.text
        font { family: Theme.mono; pixelSize: 13 }
    }
}

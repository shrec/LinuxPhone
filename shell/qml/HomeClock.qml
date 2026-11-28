import QtQuick
import Revive.Shell

// Home-screen header: terminal-style prompt + large time, date below.
Item {
    id: clock
    height: 120
    property date now: new Date()
    Timer { interval: 1000; running: true; repeat: true; onTriggered: clock.now = new Date() }

    Row {
        id: timeRow
        anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 14 }
        Text {
            text: "> "; color: Theme.dim
            font { family: Theme.mono; pixelSize: 56 }
        }
        Text {
            text: Qt.formatTime(clock.now, "hh:mm"); color: Theme.accent
            font { family: Theme.mono; pixelSize: 56; bold: true }
        }
    }
    Text {
        anchors { horizontalCenter: parent.horizontalCenter; top: timeRow.bottom; topMargin: 2 }
        text: Qt.formatDate(clock.now, "dddd, d MMMM"); color: Theme.dim
        font { family: Theme.mono; pixelSize: 14 }
    }
}

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

    Row {
        id: indicators
        anchors { right: clock.left; rightMargin: 12; verticalCenter: parent.verticalCenter }
        spacing: 10

        // Wi-Fi: four bars of rising height; a dim "x" when not connected.
        Row {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 4
            Row {
                id: wifi
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2
                height: 12
                Repeater {
                    model: 4
                    Rectangle {
                        required property int index
                        width: 3
                        height: 3 + index * 3
                        anchors.bottom: parent.bottom
                        color: StatusModel.wifiConnected && index < StatusModel.wifiBars ? Theme.accent : Theme.dim
                        opacity: StatusModel.wifiConnected && index < StatusModel.wifiBars ? 1.0 : 0.3
                    }
                }
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

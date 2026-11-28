import QtQuick
import Revive.Shell

Item {
    id: root
    signal launch(string appId)

    HomeClock {
        id: clock
        anchors { top: parent.top; left: parent.left; right: parent.right }
    }

    // Dock taps reuse the grid's launch signal, so Main's activate-or-launch path serves both.
    Dock {
        id: dock
        z: 1  // its toast rises over the grid's bottom rows
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom
                  leftMargin: 12; rightMargin: 12; bottomMargin: 36 }  // 28 px HomeStrip + 8
        onLaunch: (appId) => root.launch(appId)
    }

    GridView {
        id: grid
        anchors { top: clock.bottom; bottom: dock.top; left: parent.left; right: parent.right
                  leftMargin: 12; rightMargin: 12; bottomMargin: 8 }
        clip: true
        cellWidth: width / 4
        cellHeight: cellWidth + 22
        model: AppModel
        delegate: Item {
            id: cell
            required property string name
            required property string icon
            required property string appId
            width: grid.cellWidth; height: grid.cellHeight

            Rectangle {
                id: tile
                width: parent.width - 16; height: width
                anchors.horizontalCenter: parent.horizontalCenter
                radius: 14; color: Theme.panel
                border { color: tap.pressed ? Theme.accent : Theme.dim; width: 1 }
                Image {
                    id: img
                    anchors { fill: parent; margins: 10 }
                    source: cell.icon ? "image://icon/" + cell.icon : ""
                    sourceSize: Qt.size(64, 64)
                    fillMode: Image.PreserveAspectFit
                    visible: status === Image.Ready
                }
                Text {  // no icon: terminal-style letter tile
                    anchors.centerIn: parent
                    visible: img.status !== Image.Ready
                    text: cell.name.charAt(0).toUpperCase()
                    color: Theme.accent
                    font { family: Theme.mono; pixelSize: 26; bold: true }
                }
            }
            Text {
                anchors { top: tile.bottom; topMargin: 4; horizontalCenter: parent.horizontalCenter }
                width: parent.width - 4
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
                text: cell.name; color: Theme.text
                font { family: Theme.mono; pixelSize: 11 }
            }
            TapHandler { id: tap; onTapped: root.launch(cell.appId) }
        }
    }
}

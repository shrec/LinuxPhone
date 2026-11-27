import QtQuick
import QtQuick.Window
import Revive.Shell

Window {
    visible: true
    color: Theme.background
    ListView {
        anchors.fill: parent
        model: AppModel
        delegate: Text { required property string name; text: "> " + name; color: Theme.accent; font.family: Theme.mono; font.pixelSize: 16 }
    }
}

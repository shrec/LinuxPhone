import QtQuick
import Revive.Shell

// Four-slot dock. A slot resolves to the first installed desktop id of its candidate list;
// taps on a resolved slot go out through launch(), the same signal the grid uses.
Rectangle {
    id: dock
    signal launch(string appId)
    height: 72
    radius: 20
    color: Theme.panel
    border { color: Theme.dim; width: 1 }

    property int revision: 0
    property string toastText: ""
    readonly property var slots: [
        { label: "Phone", icon: "phone", ids: ["org.gnome.Calls"] },
        { label: "Messages", icon: "messages", ids: ["sm.puri.Chatty", "org.gnome.Chatty"] },
        { label: "Contacts", icon: "contacts", ids: ["org.gnome.Contacts"] },
        { label: "Camera", icon: "camera", ids: ["org.gnome.Snapshot", "org.postmarketos.Megapixels", "megapixels"] }
    ]

    Component.onCompleted: revision++
    Connections {
        target: AppModel
        function onModelReset() { dock.revision++ }
    }
    Timer { id: toastTimer; interval: 2000; onTriggered: dock.toastText = "" }

    Row {
        anchors.fill: parent
        Repeater {
            model: dock.slots
            delegate: Item {
                id: slot
                required property var modelData
                // Reading dock.revision makes this binding re-resolve on every model reset.
                readonly property string appId: { dock.revision; return AppModel.resolve(modelData.ids) }
                width: dock.width / 4; height: dock.height
                opacity: appId !== "" ? 1.0 : 0.35

                Accessible.role: Accessible.Button
                Accessible.name: modelData.label

                Rectangle {
                    anchors.centerIn: parent
                    width: 56; height: 56; radius: 16
                    color: "transparent"
                    border { color: tap.pressed ? Theme.accent : "transparent"; width: 1 }
                    Image {
                        anchors.centerIn: parent
                        width: 32; height: 32
                        source: "qrc:/qt/qml/Revive/Shell/icons/" + slot.modelData.icon + ".svg"
                        sourceSize: Qt.size(96, 96)
                        fillMode: Image.PreserveAspectFit
                    }
                }
                TapHandler {
                    id: tap
                    onTapped: {
                        if (slot.appId !== "") {
                            dock.launch(slot.appId)
                        } else {
                            dock.toastText = slot.modelData.label + ": not installed"
                            toastTimer.restart()
                        }
                    }
                }
            }
        }
    }

    Rectangle {
        anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.top; bottomMargin: 8 }
        visible: dock.toastText !== ""
        width: toastLabel.implicitWidth + 24; height: toastLabel.implicitHeight + 12
        radius: 10; color: Theme.panel
        border { color: Theme.dim; width: 1 }
        Text {
            id: toastLabel
            anchors.centerIn: parent
            text: dock.toastText; color: Theme.text
            font { family: Theme.mono; pixelSize: 12 }
        }
    }
}

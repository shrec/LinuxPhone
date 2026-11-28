import QtQuick
import Revive.Shell

// Quick settings shade, Android style: this item fills the whole quick settings window. The panel
// hangs from the top edge and slides with `progress` (0 = hidden, 1 = fully open); the area below
// it is dimmed. The owner (Main.qml) drives `progress` from the drag gestures and the animation.
Item {
    id: shade

    // 0..1, how far the panel is pulled down.
    property real progress: 0
    // The window is mapped (dragging, animating or open): the model polls every 10 s meanwhile.
    property bool open: false
    // Network whose passphrase is being asked for (object path), and what was typed so far. They live
    // here, not in the delegate, so a rebuilt list does not lose them.
    property string askPath: ""
    property string passText: ""
    readonly property int maxHeight: 520
    readonly property real panelHeight: panel.height

    signal closeRequested()
    // A vertical drag on the panel or the dimmed area: start, moving (total dy in px), end (dy and the
    // release velocity in px/s).
    signal dragStarted()
    signal dragMoved(real dy)
    signal dragFinished(real dy, real vy)

    Binding { target: WifiModel; property: "active"; value: shade.open }
    onOpenChanged: if (!open) { askPath = ""; passText = "" }

    // Dimmed area below the panel: tap closes, drag up follows the finger. The dim fills the whole
    // window, so a tap that lands on the panel (including parts that take no presses) must not close.
    Rectangle {
        id: dim
        anchors.fill: parent
        color: "black"
        opacity: 0.5 * shade.progress
        TapHandler {
            onTapped: (eventPoint) => {
                if (eventPoint.position.y > panel.y + panel.height)
                    shade.closeRequested()
            }
        }
        DragHandler {
            target: null
            xAxis.enabled: false
            yAxis.enabled: true
            property real lastDy: 0
            property real lastVy: 0
            onActiveTranslationChanged: if (active) { lastDy = activeTranslation.y; shade.dragMoved(lastDy) }
            onCentroidChanged: if (active) lastVy = centroid.velocity.y
            onActiveChanged: if (active) { lastDy = 0; lastVy = 0; shade.dragStarted() } else shade.dragFinished(lastDy, lastVy)
        }
    }

    Rectangle {
        id: panel
        anchors { left: parent.left; right: parent.right }
        height: Math.min(shade.maxHeight, tilesBox.height + scanRow.height + info.height
                         + Math.max(list.contentHeight, 48) + closeRow.height)
        y: -height + shade.progress * height
        color: Theme.panel

        // Drag the panel up to close; off while the list is long enough to scroll (the list flicks).
        DragHandler {
            target: null
            xAxis.enabled: false
            yAxis.enabled: true
            enabled: list.contentHeight <= list.height
            property real lastDy: 0
            property real lastVy: 0
            onActiveTranslationChanged: if (active) { lastDy = activeTranslation.y; shade.dragMoved(lastDy) }
            onCentroidChanged: if (active) lastVy = centroid.velocity.y
            onActiveChanged: if (active) { lastDy = 0; lastVy = 0; shade.dragStarted() } else shade.dragFinished(lastDy, lastVy)
        }

        // Quick tiles. Only Wi-Fi for now; more tiles (brightness, Bluetooth, rotation) go next to it.
        Item {
            id: tilesBox
            anchors { top: parent.top; left: parent.left; right: parent.right }
            height: 76

            Row {
                id: tiles
                anchors { top: parent.top; topMargin: 12; left: parent.left; leftMargin: 12 }
                spacing: 8
                readonly property real tileWidth: (shade.width - 24 - 2 * spacing) / 3

                Rectangle {
                    id: wifiTile
                    width: tiles.tileWidth; height: 64; radius: 10
                    color: WifiModel.powered ? Theme.accent : Theme.background
                    border { width: 1; color: WifiModel.powered ? Theme.accent : Theme.dim }
                    opacity: WifiModel.available ? 1.0 : 0.4

                    Column {
                        anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; leftMargin: 10; rightMargin: 10 }
                        spacing: 4
                        Text {
                            width: parent.width
                            elide: Text.ElideRight
                            text: "wifi"
                            color: WifiModel.powered ? Theme.background : Theme.text
                            font { family: Theme.mono; pixelSize: 14; bold: true }
                        }
                        Text {
                            width: parent.width
                            elide: Text.ElideRight
                            text: !WifiModel.available ? "unavailable"
                                : !WifiModel.powered ? "off"
                                : WifiModel.connectedName !== "" ? WifiModel.connectedName
                                : "not connected"
                            color: WifiModel.powered ? Theme.background : Theme.dim
                            font { family: Theme.mono; pixelSize: 11 }
                        }
                    }
                    TapHandler {
                        enabled: WifiModel.available
                        onTapped: WifiModel.setPowered(!WifiModel.powered)
                    }
                }
            }
        }

        // Scan row.
        Item {
            id: scanRow
            anchors { top: tilesBox.bottom; left: parent.left; right: parent.right }
            height: 44

            Text {
                anchors { left: parent.left; leftMargin: 12; verticalCenter: parent.verticalCenter }
                text: "networks"; color: Theme.accent
                font { family: Theme.mono; pixelSize: 14; bold: true }
            }

            Rectangle {
                anchors { right: parent.right; rightMargin: 12; verticalCenter: parent.verticalCenter }
                width: scanLabel.width + 24; height: 32
                color: "transparent"
                border { width: 1; color: scanArea.enabled ? Theme.accent : Theme.dim }
                Text {
                    id: scanLabel
                    anchors.centerIn: parent
                    text: WifiModel.scanning ? "scanning..." : "scan"
                    color: scanArea.enabled ? Theme.accent : Theme.dim
                    font { family: Theme.mono; pixelSize: 13 }
                }
                MouseArea {
                    id: scanArea
                    anchors.fill: parent
                    enabled: WifiModel.available && WifiModel.powered && !WifiModel.scanning
                    onClicked: WifiModel.scan()
                }
            }
        }

        // Status line and the last error.
        Column {
            id: info
            anchors { top: scanRow.bottom; left: parent.left; right: parent.right; leftMargin: 12; rightMargin: 12 }
            spacing: 2
            Text {
                width: parent.width
                elide: Text.ElideRight
                color: Theme.text
                font { family: Theme.mono; pixelSize: 12 }
                text: !WifiModel.available ? "wifi unavailable"
                    : !WifiModel.powered ? "wifi off"
                    : WifiModel.connectingName !== "" ? "connecting to " + WifiModel.connectingName + "..."
                    : WifiModel.connectedName !== "" ? "connected: " + WifiModel.connectedName
                    : WifiModel.state
            }
            Text {
                visible: WifiModel.lastError !== ""
                width: parent.width
                elide: Text.ElideRight
                color: Theme.dim
                font { family: Theme.mono; pixelSize: 12 }
                text: WifiModel.lastError
            }
            Item { width: 1; height: 6 }
        }

        ListView {
            id: list
            anchors { top: info.bottom; bottom: closeRow.top; left: parent.left; right: parent.right }
            clip: true
            model: WifiModel

            delegate: Item {
                id: row
                required property int index
                required property string name
                required property string path
                required property int bars
                required property bool secured
                required property bool known
                required property bool connected
                readonly property bool asking: shade.askPath === path

                width: ListView.view.width
                height: 44 + (asking && formLoader.item ? formLoader.item.height : 0)

                Rectangle {
                    anchors { left: parent.left; right: parent.right; top: parent.top }
                    height: 44
                    color: row.connected ? Qt.rgba(0.22, 1.0, 0.53, 0.08) : "transparent"

                    // Signal strength, as in the status bar.
                    WifiIcon {
                        id: barRow
                        anchors { left: parent.left; leftMargin: 12; verticalCenter: parent.verticalCenter }
                        bars: row.bars
                    }

                    Text {
                        anchors { left: barRow.right; leftMargin: 12; right: tags.left; rightMargin: 8; verticalCenter: parent.verticalCenter }
                        text: row.name
                        elide: Text.ElideRight
                        color: row.connected ? Theme.accent : Theme.text
                        font { family: Theme.mono; pixelSize: 14 }
                    }

                    Row {
                        id: tags
                        anchors { right: parent.right; rightMargin: 12; verticalCenter: parent.verticalCenter }
                        spacing: 8
                        Text {
                            visible: row.connected
                            text: "connected"; color: Theme.accent
                            font { family: Theme.mono; pixelSize: 11 }
                        }
                        Text {
                            visible: row.secured
                            text: "[*]"; color: Theme.dim
                            font { family: Theme.mono; pixelSize: 11 }
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            if (row.connected)
                                return
                            if (!row.secured || row.known) {
                                shade.askPath = ""
                                WifiModel.connectTo(row.path, "")
                            } else if (row.asking) {
                                shade.askPath = ""
                            } else {
                                shade.passText = ""
                                shade.askPath = row.path
                            }
                        }
                        onPressAndHold: if (row.known) WifiModel.forget(row.path)
                    }
                }

                // Passphrase prompt for an unknown secured network.
                Loader {
                    id: formLoader
                    active: row.asking
                    anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                    sourceComponent: Item {
                        height: 92
                        Rectangle {
                            id: field
                            anchors { left: parent.left; right: parent.right; top: parent.top; leftMargin: 12; rightMargin: 12; topMargin: 4 }
                            height: 36
                            color: Theme.background
                            border { width: 1; color: Theme.dim }
                            TextInput {
                                id: input
                                anchors { fill: parent; leftMargin: 8; rightMargin: 8 }
                                verticalAlignment: TextInput.AlignVCenter
                                echoMode: TextInput.Password
                                inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                                color: Theme.text
                                selectionColor: Theme.accent
                                clip: true
                                font { family: Theme.mono; pixelSize: 14 }
                                Component.onCompleted: {
                                    text = shade.passText
                                    forceActiveFocus()
                                }
                                onTextChanged: shade.passText = text
                                onAccepted: if (text.length > 0) go.clicked()
                            }
                            Text {
                                visible: input.text.length === 0
                                anchors { left: parent.left; leftMargin: 8; verticalCenter: parent.verticalCenter }
                                text: "passphrase"; color: Theme.dim
                                font { family: Theme.mono; pixelSize: 14 }
                            }
                        }
                        Row {
                            anchors { left: parent.left; leftMargin: 12; top: field.bottom; topMargin: 8 }
                            spacing: 10
                            Rectangle {
                                width: 96; height: 32
                                color: "transparent"
                                border { width: 1; color: input.text.length > 0 ? Theme.accent : Theme.dim }
                                Text {
                                    anchors.centerIn: parent
                                    text: "Connect"
                                    color: input.text.length > 0 ? Theme.accent : Theme.dim
                                    font { family: Theme.mono; pixelSize: 13 }
                                }
                                MouseArea {
                                    id: go
                                    anchors.fill: parent
                                    enabled: input.text.length > 0
                                    onClicked: {
                                        // Read everything first: clearing askPath unloads this form (and this handler).
                                        const networkPath = row.path
                                        const pass = input.text
                                        WifiModel.connectTo(networkPath, pass)
                                        shade.askPath = ""
                                        shade.passText = ""
                                    }
                                }
                            }
                            Rectangle {
                                width: 96; height: 32
                                color: "transparent"
                                border { width: 1; color: Theme.dim }
                                Text {
                                    anchors.centerIn: parent
                                    text: "Cancel"; color: Theme.text
                                    font { family: Theme.mono; pixelSize: 13 }
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: { shade.askPath = ""; shade.passText = "" }
                                }
                            }
                        }
                    }
                }
            }
        }

        // Empty state: outside the (clipped) list, in the list's own area, which is at least 48 px.
        Text {
            visible: list.count === 0 && text !== ""
            anchors { top: list.top; left: parent.left; leftMargin: 12; topMargin: 8 }
            text: WifiModel.available && WifiModel.powered ? "no networks, try scan" : ""
            color: Theme.dim
            font { family: Theme.mono; pixelSize: 12 }
        }

        // Close row at the bottom; always draggable, also when the list scrolls.
        Rectangle {
            id: closeRow
            anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
            height: 36
            color: Theme.panel
            Rectangle {
                anchors { top: parent.top; left: parent.left; right: parent.right }
                height: 1; color: Theme.dim; opacity: 0.4
            }
            Text {
                anchors.centerIn: parent
                text: "^ close"; color: Theme.dim
                font { family: Theme.mono; pixelSize: 12 }
            }
            TapHandler { onTapped: shade.closeRequested() }
            DragHandler {
                target: null
                xAxis.enabled: false
                yAxis.enabled: true
                property real lastDy: 0
                property real lastVy: 0
                onActiveTranslationChanged: if (active) { lastDy = activeTranslation.y; shade.dragMoved(lastDy) }
                onCentroidChanged: if (active) lastVy = centroid.velocity.y
                onActiveChanged: if (active) { lastDy = 0; lastVy = 0; shade.dragStarted() } else shade.dragFinished(lastDy, lastVy)
            }
        }
    }
}

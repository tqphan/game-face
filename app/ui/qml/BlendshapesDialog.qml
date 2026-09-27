import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GameFace

// Live scores (0..100) for all 52 blendshapes.
Dialog {
    anchors.centerIn: Overlay.overlay
    width: Math.min(640, Overlay.overlay ? Overlay.overlay.width - 40 : 640)
    height: Overlay.overlay ? Overlay.overlay.height - 80 : 600
    modal: true
    title: qsTr("Blendshapes")
    standardButtons: Dialog.Close

    ListView {
        anchors.fill: parent
        clip: true
        model: App.blendshapes
        ScrollBar.vertical: ScrollBar {}

        delegate: RowLayout {
            required property string name
            required property real value
            width: ListView.view.width - 16
            height: 28
            spacing: 12

            Label {
                Layout.preferredWidth: 170
                horizontalAlignment: Text.AlignRight
                text: parent.name
            }
            ProgressBar {
                Layout.fillWidth: true
                from: 0
                to: 100
                value: parent.value
            }
            Label {
                Layout.preferredWidth: 32
                text: Math.round(parent.value)
            }
        }
    }
}

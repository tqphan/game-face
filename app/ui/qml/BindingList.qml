import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GameFace

ColumnLayout {
    spacing: 12

    Button {
        Layout.fillWidth: true
        visible: App.profiles.count > 0
        enabled: !App.settings.lockUi
        text: qsTr("Add binding")
        highlighted: true
        onClicked: App.addBinding()
    }

    ListView {
        id: list
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: App.profiles.count > 0
        clip: true
        spacing: 12
        model: App.bindings
        ScrollBar.vertical: ScrollBar {}

        delegate: BindingCard {
            width: ListView.view.width - 12
        }
    }

    Label {
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: App.profiles.count === 0
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        wrapMode: Text.WordWrap
        font.pixelSize: 18
        opacity: 0.7
        text: qsTr("Create a profile to start adding bindings.")
    }
}

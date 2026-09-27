import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import GameFace

AppDialog {
    id: dialog

    anchors.centerIn: Overlay.overlay
    width: 420
    modal: true
    title: qsTr("Create profile")
    footer: DialogButtonBox {
        AppButton {
            text: qsTr("Cancel")
            flat: true
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }
        AppButton {
            text: qsTr("Create")
            highlighted: true
            Material.accent: Theme.primaryButton
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
        }
    }

    onOpened: {
        nameField.text = ""
        nameField.forceActiveFocus()
    }
    onAccepted: App.createProfile(nameField.text)

    TextField {
        id: nameField
        width: parent.width
        placeholderText: qsTr("Profile name")
        onAccepted: dialog.accept()
    }
}

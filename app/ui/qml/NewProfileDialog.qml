import QtQuick
import QtQuick.Controls
import GameFace

Dialog {
    id: dialog

    anchors.centerIn: Overlay.overlay
    width: 420
    modal: true
    title: qsTr("Create profile")
    standardButtons: Dialog.Ok | Dialog.Cancel
    footer: AppDialogButtons {}

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

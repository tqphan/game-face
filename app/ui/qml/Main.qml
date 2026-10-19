import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import GameFace

ApplicationWindow {
    id: window

    width: 1280
    height: 800
    visible: true
    visibility: Window.Maximized
    title: "game-face"

    Material.theme: App.settings.theme === "light" ? Material.Light : Material.Dark
    Material.accent: Material.Teal

    property bool closeConfirmed: false

    onClosing: (close) => {
        if (closeConfirmed)
            return
        if (App.settings.confirmOnClose) {
            close.accepted = false
            closeDialog.open()
        } else {
            App.shutdown()
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12

        ProfilePanel {
            Layout.preferredWidth: Math.max(340, window.width / 3)
            Layout.fillHeight: true
            onNewProfileRequested: newProfileDialog.open()
            onSettingsRequested: settingsWindow.open()
            onBlendshapesRequested: blendshapesDialog.open()
        }

        BindingList {
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }

    NewProfileDialog { id: newProfileDialog }
    SettingsWindow { id: settingsWindow; transientParent: window }
    BlendshapesDialog { id: blendshapesDialog }

    CloseDialog {
        id: closeDialog
        onConfirmed: {
            App.shutdown()
            window.closeConfirmed = true
            window.close()
        }
    }
}

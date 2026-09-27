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
        if (!closeConfirmed) {
            close.accepted = false
            closeDialog.open()
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
            onSettingsRequested: settingsDialog.open()
            onBlendshapesRequested: blendshapesDialog.open()
        }

        BindingList {
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }

    NewProfileDialog { id: newProfileDialog }
    SettingsDialog { id: settingsDialog }
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

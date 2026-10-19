import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Dialogs
import QtQuick.Layouts
import GameFace

// A modal window of its own, not a popup inside the main window. Changes
// apply immediately; Save writes them to disk and restarts tracking so new
// confidence thresholds take effect.
ApplicationWindow {
    id: dialog

    width: 640
    height: transientParent ? Math.min(760, transientParent.height - 80) : 700
    minimumWidth: 400
    minimumHeight: 300
    flags: Qt.Dialog
    modality: Qt.ApplicationModal
    title: qsTr("Settings")

    // A separate window doesn't inherit the main window's Material settings.
    Material.theme: App.settings.theme === "light" ? Material.Light : Material.Dark
    Material.accent: Material.Teal

    // Centered over the main window, like the popup it replaces.
    function open() {
        if (transientParent) {
            x = transientParent.x + (transientParent.width - width) / 2
            y = transientParent.y + (transientParent.height - height) / 2
        }
        show()
        raise()
        requestActivate()
    }

    Shortcut {
        sequences: [StandardKey.Cancel]
        onActivated: dialog.close()
    }

    footer: AppDialogButtons {
        standardButtons: DialogButtonBox.Save | DialogButtonBox.Close
        onAccepted: {
            App.saveSettings()
            dialog.close()
        }
        onRejected: dialog.close()
    }

    readonly property var settings: App.settings

    component LabeledSlider: ColumnLayout {
        id: labeled
        property string label
        property real value
        signal moved(real value)
        Layout.fillWidth: true
        spacing: 0
        Label {
            text: labeled.label + ": " + labeled.value.toFixed(2)
            opacity: 0.8
        }
        Slider {
            Layout.fillWidth: true
            from: 0
            to: 1
            stepSize: 0.01
            value: labeled.value
            onMoved: labeled.moved(value)
        }
    }

    ScrollView {
        anchors.fill: parent
        anchors.margins: 24
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: parent.width
            spacing: 8

            Label { text: qsTr("Camera"); font.bold: true }
            ComboBox {
                Layout.fillWidth: true
                model: App.cameras
                textRole: "name"
                valueRole: "id"
                currentIndex: indexOfValue(dialog.settings.cameraId)
                displayText: currentIndex < 0 ? qsTr("Default camera") : currentText
                onActivated: dialog.settings.cameraId = currentValue
            }

            Label { text: qsTr("Theme"); font.bold: true }
            ComboBox {
                Layout.fillWidth: true
                model: [
                    { value: "dark", text: qsTr("Dark") },
                    { value: "light", text: qsTr("Light") }
                ]
                textRole: "text"
                valueRole: "value"
                currentIndex: indexOfValue(dialog.settings.theme)
                onActivated: dialog.settings.theme = currentValue
            }

            CheckBox {
                text: qsTr("Allow input simulation (send keys and clicks)")
                checked: dialog.settings.allowInputSimulation
                onToggled: dialog.settings.allowInputSimulation = checked
            }
            CheckBox {
                text: qsTr("Run stop commands when tracking stops or the camera drops out")
                ToolTip.visible: hovered
                ToolTip.delay: 500
                ToolTip.text: qsTr("Bindings run once as if every blendshape dropped to 0.")
                checked: dialog.settings.settleOnStop
                onToggled: dialog.settings.settleOnStop = checked
            }
            CheckBox {
                text: qsTr("Start tracking when game-face opens")
                checked: dialog.settings.autoStartTracking
                onToggled: dialog.settings.autoStartTracking = checked
            }
            CheckBox {
                text: qsTr("Ask for confirmation before closing")
                checked: dialog.settings.confirmOnClose
                onToggled: dialog.settings.confirmOnClose = checked
            }
            CheckBox {
                text: qsTr("Save settings automatically")
                checked: dialog.settings.autoSaveSettings
                onToggled: dialog.settings.autoSaveSettings = checked
            }
            CheckBox {
                text: qsTr("Save profiles automatically")
                checked: dialog.settings.autoSaveProfiles
                onToggled: dialog.settings.autoSaveProfiles = checked
            }

            LabeledSlider {
                label: qsTr("Webcam opacity")
                value: dialog.settings.webcamOpacity
                onMoved: (v) => dialog.settings.webcamOpacity = v
            }
            LabeledSlider {
                label: qsTr("Landmarks opacity")
                value: dialog.settings.landmarksOpacity
                onMoved: (v) => dialog.settings.landmarksOpacity = v
            }
            LabeledSlider {
                label: qsTr("Face detection confidence")
                value: dialog.settings.detectionConfidence
                onMoved: (v) => dialog.settings.detectionConfidence = v
            }
            LabeledSlider {
                label: qsTr("Face presence confidence")
                value: dialog.settings.presenceConfidence
                onMoved: (v) => dialog.settings.presenceConfidence = v
            }
            LabeledSlider {
                label: qsTr("Tracking confidence")
                value: dialog.settings.trackingConfidence
                onMoved: (v) => dialog.settings.trackingConfidence = v
            }

            MenuSeparator { Layout.fillWidth: true }

            Label { text: qsTr("Import from the original game-face"); font.bold: true }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                opacity: 0.8
                text: qsTr("Choose the folder with user.profiles.json and user.settings.json (src/assets/json). This replaces your current profiles and settings.")
            }
            RowLayout {
                AppButton {
                    text: qsTr("Import…")
                    onClicked: folderDialog.open()
                }
                Label {
                    id: importResult
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                opacity: 0.6
                font.pixelSize: 12
                text: qsTr("Settings are stored in %1").arg(App.configFolder)
            }
        }
    }

    FolderDialog {
        id: folderDialog
        title: qsTr("Folder with user.profiles.json")
        onAccepted: importResult.text = App.importLegacy(selectedFolder)
    }
}

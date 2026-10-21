import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtMultimedia
import GameFace

Pane {
    id: panel

    signal newProfileRequested()
    signal settingsRequested()
    signal blendshapesRequested()

    Material.elevation: 2
    padding: 16

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            Label {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignBaseline
                text: "game-face"
                font.pixelSize: 22
                font.bold: true
            }
            Label {
                Layout.alignment: Qt.AlignRight | Qt.AlignBaseline
                text: "v" + Qt.application.version
                opacity: 0.6
            }
        }

        RowLayout {
            Layout.fillWidth: true
            uniformCellSizes: true
            AppButton {
                Layout.fillWidth: true
                text: qsTr("Create profile")
                highlighted: true
                Material.accent: Theme.primaryButton
                enabled: !App.settings.lockUi
                onClicked: panel.newProfileRequested()
            }
            AppButton {
                Layout.fillWidth: true
                // Highlighted while there are unsaved changes; a failed save
                // keeps it that way and reports the error in the status line.
                text: App.profilesDirty ? qsTr("Save profiles") : qsTr("Saved")
                highlighted: App.profilesDirty
                Material.accent: Theme.primaryButton
                enabled: App.profilesDirty
                onClicked: App.saveProfiles()
            }
            AppButton {
                Layout.fillWidth: true
                text: qsTr("Remove profile")
                enabled: !App.settings.lockUi && App.profiles.count > 0
                Material.accent: Material.Red
                onClicked: removeConfirm.open()
            }
        }

        Frame {
            Layout.fillWidth: true
            Layout.preferredHeight: 180
            padding: 0

            ListView {
                id: profileList
                anchors.fill: parent
                clip: true
                model: App.profiles
                currentIndex: App.profileIndex
                ScrollBar.vertical: ScrollBar {}

                delegate: ItemDelegate {
                    required property int index
                    required property string name
                    width: ListView.view.width
                    text: name
                    highlighted: index === App.profileIndex
                    // Rows, not the list, so a locked list still scrolls.
                    enabled: !App.settings.lockUi
                    onClicked: App.profileIndex = index
                }

                Label {
                    anchors.centerIn: parent
                    visible: profileList.count === 0
                    text: qsTr("No profiles yet")
                    opacity: 0.6
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            // Equal widths; otherwise the longer label gets the wider button.
            uniformCellSizes: true
            AppButton {
                Layout.fillWidth: true
                text: qsTr("Settings")
                onClicked: panel.settingsRequested()
            }
            AppButton {
                Layout.fillWidth: true
                text: qsTr("Blendshapes")
                onClicked: panel.blendshapesRequested()
            }
        }

        AppButton {
            Layout.fillWidth: true
            text: App.tracking ? qsTr("Stop tracking") : qsTr("Start tracking")
            highlighted: App.tracking
            Material.accent: Theme.positiveButton
            enabled: !App.settings.lockUi
            onClicked: App.toggleTracking()
        }

        AppButton {
            Layout.fillWidth: true
            text: App.settings.lockUi ? qsTr("LOCKED") : qsTr("LOCK UI")
            checkable: true
            checked: App.settings.lockUi
            // Filled deep orange while locked, so it stands out from the other
            // (teal and green) buttons.
            highlighted: App.settings.lockUi
            Material.accent: Theme.lockedButton
            font.bold: App.settings.lockUi
            onToggled: App.settings.lockUi = checked
        }

        Label {
            Layout.fillWidth: true
            visible: !App.settings.allowInputSimulation
            wrapMode: Text.WordWrap
            color: Material.color(Material.Orange)
            text: qsTr("Input simulation is off: bindings are evaluated but no keys or clicks are sent. Turn it on in Settings.")
        }

        Label {
            Layout.fillWidth: true
            visible: App.statusMessage !== ""
            wrapMode: Text.WordWrap
            color: Material.color(Material.Red)
            text: App.statusMessage
        }

        // Camera preview, mirrored like a mirror (as in the original app).
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 160
            color: "black"
            radius: 4
            clip: true

            Item {
                id: mirror
                anchors.fill: parent
                transform: Scale { origin.x: mirror.width / 2; xScale: -1 }

                VideoOutput {
                    id: videoOutput
                    anchors.fill: parent
                    fillMode: VideoOutput.PreserveAspectFit
                    visible: App.tracking
                    opacity: App.settings.webcamOpacity
                    Component.onCompleted: App.videoOutput = videoOutput
                }

                LandmarkOverlay {
                    anchors.fill: parent
                    source: App
                    contentRect: videoOutput.contentRect
                    opacity: App.settings.landmarksOpacity
                }
            }

            Label {
                anchors.centerIn: parent
                visible: !App.tracking || !App.faceFound
                color: "white"
                opacity: 0.7
                text: App.tracking ? qsTr("Looking for a face…") : qsTr("Tracking is off")
            }
        }
    }

    AppDialog {
        id: removeConfirm
        anchors.centerIn: Overlay.overlay
        modal: true
        title: qsTr("Remove profile")
        standardButtons: Dialog.Yes | Dialog.No
        footer: AppDialogButtons {}
        Label {
            text: qsTr("Remove the selected profile and its bindings?")
        }
        onAccepted: App.removeProfile()
    }
}

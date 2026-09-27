import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import GameFace

// One binding. Edits are written straight to the model (`model.role = value`).
Pane {
    id: card

    required property int index
    required property var model
    required property bool bindingEnabled
    required property bool simplified
    required property string blendshape
    required property real threshold
    required property string simpleStartCommand
    required property string simpleStopCommand
    required property string simpleStartCommandError
    required property string simpleStopCommandError
    required property bool simpleActive
    required property real liveValue
    required property string startExpression
    required property string startExpressionError
    required property string startCommand
    required property string startCommandError
    required property real startDebounce
    required property bool startActive
    required property string stopExpression
    required property string stopExpressionError
    required property string stopCommand
    required property string stopCommandError
    required property real stopDebounce
    required property bool stopActive

    readonly property bool locked: App.settings.lockUi

    Material.elevation: 2
    padding: 14
    opacity: bindingEnabled ? 1 : 0.6

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        RowLayout {
            Layout.fillWidth: true

            Switch {
                text: card.simplified ? qsTr("Simple") : qsTr("Advanced")
                checked: card.simplified
                enabled: !card.locked
                onToggled: card.model.simplified = checked
            }
            CheckBox {
                text: qsTr("Enabled")
                checked: card.bindingEnabled
                enabled: !card.locked
                onToggled: card.model.bindingEnabled = checked
            }
            Item { Layout.fillWidth: true }
            ToolButton {
                text: "✕"
                enabled: !card.locked
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Remove binding")
                onClicked: App.removeBinding(card.index)
            }
        }

        // Simple: one blendshape against a threshold.
        ColumnLayout {
            Layout.fillWidth: true
            visible: card.simplified
            spacing: 8

            Item {
                Layout.fillWidth: true
                implicitHeight: thresholdSlider.implicitHeight

                ProgressBar {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: thresholdSlider.leftPadding
                    anchors.rightMargin: thresholdSlider.rightPadding
                    from: 0
                    to: 100
                    value: card.liveValue
                    Material.accent: card.simpleActive ? Material.Green : Material.Grey
                }
                Slider {
                    id: thresholdSlider
                    anchors.fill: parent
                    from: 0
                    to: 100
                    stepSize: 1
                    value: card.threshold
                    enabled: !card.locked
                    onMoved: card.model.threshold = value
                    // Only the thumb: the level bar underneath is the track.
                    background: Item {}
                    ToolTip.visible: pressed
                    ToolTip.text: qsTr("Threshold: %1").arg(Math.round(value))
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                ComboBox {
                    Layout.preferredWidth: 220
                    model: App.blendshapeNames
                    currentIndex: App.blendshapeNames.indexOf(card.blendshape)
                    enabled: !card.locked
                    onActivated: (i) => card.model.blendshape = App.blendshapeNames[i]
                }
                CommandField {
                    Layout.fillWidth: true
                    label: qsTr("Start command")
                    text: card.simpleStartCommand
                    error: card.simpleStartCommandError
                    readOnly: card.locked
                    onCommitted: (value) => card.model.simpleStartCommand = value
                }
                CommandField {
                    Layout.fillWidth: true
                    label: qsTr("Stop command")
                    text: card.simpleStopCommand
                    error: card.simpleStopCommandError
                    readOnly: card.locked
                    onCommitted: (value) => card.model.simpleStopCommand = value
                }
            }
        }

        // Advanced: start and stop expressions.
        RowLayout {
            Layout.fillWidth: true
            visible: !card.simplified
            spacing: 12

            TriggerEditor {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                title: qsTr("Start")
                active: card.startActive && App.tracking
                expression: card.startExpression
                expressionError: card.startExpressionError
                command: card.startCommand
                commandError: card.startCommandError
                debounce: card.startDebounce
                locked: card.locked
                onExpressionCommitted: (value) => card.model.startExpression = value
                onCommandCommitted: (value) => card.model.startCommand = value
                onDebounceMoved: (value) => card.model.startDebounce = value
            }
            TriggerEditor {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                title: qsTr("Stop")
                active: card.stopActive && App.tracking
                expression: card.stopExpression
                expressionError: card.stopExpressionError
                command: card.stopCommand
                commandError: card.stopCommandError
                debounce: card.stopDebounce
                locked: card.locked
                onExpressionCommitted: (value) => card.model.stopExpression = value
                onCommandCommitted: (value) => card.model.stopCommand = value
                onDebounceMoved: (value) => card.model.stopDebounce = value
            }
        }
    }
}

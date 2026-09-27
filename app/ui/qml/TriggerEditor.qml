import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Expression, command and debounce for one side (start or stop) of an
// advanced binding.
Frame {
    id: editor

    property string title
    property bool active
    property string expression
    property string expressionError
    property string command
    property string commandError
    property real debounce
    property bool locked

    signal expressionCommitted(string value)
    signal commandCommitted(string value)
    signal debounceMoved(real value)

    ColumnLayout {
        anchors.fill: parent
        spacing: 6

        Label {
            Layout.alignment: Qt.AlignHCenter
            text: editor.title
            font.bold: true
            color: editor.active ? Material.color(Material.Green) : Material.foreground
        }

        TextField {
            id: expressionField
            Layout.fillWidth: true
            placeholderText: qsTr("Expression, e.g. jawOpen > 40")
            text: editor.expression
            readOnly: editor.locked
            selectByMouse: true
            Material.accent: editor.expressionError !== "" ? Material.Red : Material.Teal
            onEditingFinished: if (text !== editor.expression) editor.expressionCommitted(text)
        }
        Label {
            Layout.fillWidth: true
            visible: editor.expressionError !== ""
            text: editor.expressionError
            wrapMode: Text.WordWrap
            font.pixelSize: 12
            color: Material.color(Material.Red)
        }

        CommandField {
            Layout.fillWidth: true
            label: qsTr("Command")
            text: editor.command
            error: editor.commandError
            readOnly: editor.locked
            onCommitted: (value) => editor.commandCommitted(value)
        }

        Label {
            text: qsTr("Debounce: %1 ms").arg(Math.round(debounceSlider.value))
            opacity: 0.8
        }
        Slider {
            id: debounceSlider
            Layout.fillWidth: true
            from: 0
            to: 1000
            stepSize: 50
            value: editor.debounce
            enabled: !editor.locked
            onMoved: editor.debounceMoved(value)
        }
    }
}

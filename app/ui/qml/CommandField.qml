import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// A command such as keyboard.press(KEY_A), with its validation error below.
ColumnLayout {
    id: root

    property string label
    property alias text: field.text
    property string error
    property bool readOnly

    signal committed(string value)

    spacing: 2

    TextField {
        id: field
        Layout.fillWidth: true
        placeholderText: root.label
        readOnly: root.readOnly
        selectByMouse: true
        font.family: "monospace"
        Material.accent: root.error !== "" ? Material.Red : Material.Teal
        onEditingFinished: root.committed(text)
        ToolTip.visible: hovered && text === ""
        ToolTip.text: qsTr("e.g. keyboard.press(KEY_A), mouse.click(BTN_LEFT)")
    }
    Label {
        Layout.fillWidth: true
        visible: root.error !== ""
        text: root.error
        wrapMode: Text.WordWrap
        font.pixelSize: 12
        color: Material.color(Material.Red)
    }
}

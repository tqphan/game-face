import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// One head mouse axis: its source expression, sensitivity, direction and
// range, with the live reading underneath. Uses no App properties, so tests
// can load it on its own.
ColumnLayout {
    id: editor

    required property var axis       // HeadMouseAxisObject
    property string label
    property var reading: ({})       // {active, raw, normalised}
    property bool locked: false
    property int smallTextSize: 12

    spacing: 10
    Layout.fillWidth: true

    Label {
        text: editor.label
        font.bold: true
    }

    TextField {
        id: sourceField
        objectName: "source"
        Layout.fillWidth: true
        text: editor.axis.source
        placeholderText: qsTr("Off (e.g. headYaw, headX, headRoll)")
        font.family: "monospace"
        enabled: !editor.locked
        Material.accent: editor.axis.error !== "" ? Material.Red : Material.Teal
        onEditingFinished: editor.axis.source = text.trim()
    }
    Label {
        objectName: "error"
        Layout.fillWidth: true
        visible: editor.axis.error !== ""
        text: editor.axis.error
        wrapMode: Text.WordWrap
        font.pixelSize: editor.smallTextSize
        color: Material.color(Material.Red)
    }

    RowLayout {
        Layout.fillWidth: true
        Label {
            text: qsTr("Sensitivity: %1").arg(editor.axis.gain.toFixed(2))
            Layout.preferredWidth: Math.max(130, implicitWidth)
        }
        Slider {
            objectName: "gain"
            Layout.fillWidth: true
            from: 0.1
            to: 5
            stepSize: 0.05
            value: editor.axis.gain
            enabled: !editor.locked
            onMoved: editor.axis.gain = value
        }
        CheckBox {
            objectName: "invert"
            text: qsTr("Reverse")
            checked: editor.axis.invert
            enabled: !editor.locked
            onToggled: editor.axis.invert = checked
        }
    }

    RowLayout {
        Layout.fillWidth: true
        Label { text: qsTr("Range below center") }
        TextField {
            objectName: "rangeNegative"
            Layout.preferredWidth: 100
            // 20, 4.5 or 0.12: three significant digits, no padding.
            text: String(Number(Number(editor.axis.rangeNegative).toPrecision(3)))
            validator: DoubleValidator { bottom: 0.0001 }
            enabled: !editor.locked
            onEditingFinished: editor.axis.rangeNegative = Number(text)
        }
        Label { text: qsTr("above") }
        TextField {
            objectName: "rangePositive"
            Layout.preferredWidth: 100
            text: String(Number(Number(editor.axis.rangePositive).toPrecision(3)))
            validator: DoubleValidator { bottom: 0.0001 }
            enabled: !editor.locked
            onEditingFinished: editor.axis.rangePositive = Number(text)
        }
        Item { Layout.fillWidth: true }
    }

    // Live: the source's value, its center, and how far along the range it is.
    RowLayout {
        Layout.fillWidth: true
        Label {
            objectName: "reading"
            Layout.preferredWidth: Math.max(220, implicitWidth)
            opacity: 0.8
            font.pixelSize: editor.smallTextSize
            text: !editor.reading || !editor.reading.active
                  ? qsTr("No reading")
                  : qsTr("Now %1, center %2")
                        .arg(Number(editor.reading.raw).toFixed(2))
                        .arg(editor.axis.hasNeutral ? Number(editor.axis.neutral).toFixed(2) : qsTr("not set"))
        }
        // -1 .. 1, center in the middle.
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 8
            radius: 4
            color: Material.dividerColor
            Rectangle {
                readonly property real n: editor.reading && editor.reading.active
                                          ? Math.max(-1, Math.min(1, editor.reading.normalised)) : 0
                x: parent.width / 2 + Math.min(0, n) * parent.width / 2
                width: Math.abs(n) * parent.width / 2
                height: parent.height
                radius: 4
                color: Material.accentColor
            }
            Rectangle {
                x: parent.width / 2 - 1
                width: 2
                height: parent.height
                color: Material.foreground
                opacity: 0.5
            }
        }
    }
}

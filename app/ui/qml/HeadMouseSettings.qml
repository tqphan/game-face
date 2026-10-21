import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// The head mouse settings page. Takes everything it shows as properties
// rather than reading App, so tests can load it with stand-in objects.
ColumnLayout {
    id: page

    required property var headMouse     // HeadMouseObject (App.settings.headMouse)
    property var readings: []           // App.headMouseAxes
    property bool calibrating: false
    property bool locked: false
    property bool tracking: false
    property var presets: []            // App.headMousePresets
    property var headSignalNames: []
    property int smallTextSize: 12

    signal recenterRequested()
    signal calibrateRequested()

    spacing: 16

    function reading(index) {
        return page.readings && page.readings.length > index ? page.readings[index] : ({})
    }

    CheckBox {
        objectName: "enabled"
        text: qsTr("Move the mouse with my head")
        checked: page.headMouse.enabled
        enabled: !page.locked
        onToggled: page.headMouse.enabled = checked
    }
    Label {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        opacity: 0.7
        font.pixelSize: page.smallTextSize
        text: qsTr("Needs \"Allow input simulation\" (Input). Bindings can also use headmouse.toggle(), "
                   + "headmouse.hold() / headmouse.release() (a clutch), headmouse.recenter() and "
                   + "headmouse.calibrate(), each as the binding's whole command.")
    }

    RowLayout {
        Layout.fillWidth: true
        Button {
            objectName: "recenter"
            text: qsTr("Recenter")
            enabled: !page.locked && page.tracking
            onClicked: page.recenterRequested()
            ToolTip.visible: hovered
            ToolTip.delay: 500
            ToolTip.text: qsTr("Use the head's current position as the center.")
        }
        Button {
            objectName: "calibrate"
            text: page.calibrating ? qsTr("Calibrating…") : qsTr("Calibrate ranges")
            enabled: !page.locked && page.tracking && !page.calibrating
            onClicked: page.calibrateRequested()
        }
        Item { Layout.fillWidth: true }
    }
    Label {
        objectName: "calibratingHint"
        Layout.fillWidth: true
        visible: page.calibrating
        wrapMode: Text.WordWrap
        text: qsTr("For the next 5 seconds, move your head as far as is comfortable in every direction "
                   + "you use. The mouse doesn't move meanwhile.")
        color: Material.accentColor
    }
    Label {
        Layout.fillWidth: true
        visible: !page.tracking
        wrapMode: Text.WordWrap
        opacity: 0.7
        text: qsTr("Start tracking to see live readings, recenter or calibrate.")
    }

    TabBar {
        id: groups
        Layout.fillWidth: true
        Layout.topMargin: 12
        TabButton { text: qsTr("Pointer") }
        TabButton { text: qsTr("Scroll") }
    }

    StackLayout {
        Layout.fillWidth: true
        currentIndex: groups.currentIndex
        Layout.preferredHeight: children[currentIndex] ? children[currentIndex].implicitHeight : 0

        GroupEditor {
            objectName: "pointerGroup"
            group: page.headMouse.pointer
            isPointer: true
            presets: page.presets
            locked: page.locked
            xReading: page.reading(0)
            yReading: page.reading(1)
            smallTextSize: page.smallTextSize
        }
        GroupEditor {
            objectName: "scrollGroup"
            group: page.headMouse.scroll
            isPointer: false
            presets: page.presets
            locked: page.locked
            xReading: page.reading(2)
            yReading: page.reading(3)
            smallTextSize: page.smallTextSize
        }
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.topMargin: 12
        Label {
            text: qsTr("Hold still after a click: %1 ms").arg(Math.round(page.headMouse.clickFreezeMs))
            Layout.preferredWidth: Math.max(220, implicitWidth)
        }
        Slider {
            objectName: "clickFreeze"
            Layout.fillWidth: true
            from: 0
            to: 500
            stepSize: 10
            value: page.headMouse.clickFreezeMs
            enabled: !page.locked
            onMoved: page.headMouse.clickFreezeMs = value
        }
    }

    Label {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        opacity: 0.6
        font.pixelSize: page.smallTextSize
        text: qsTr("Sources can use %1 and any blendshape, with arithmetic, e.g. headYaw + 2 * headX.")
                  .arg(page.headSignalNames.join(", "))
              + (Qt.platform.os === "windows"
                 ? " " + qsTr("On Windows, \"Enhance pointer precision\" (Mouse settings) changes how far "
                              + "relative movement goes; turn it off for consistent movement.")
                 : "")
    }

    component GroupEditor: ColumnLayout {
        id: editor
        required property var group     // HeadMouseGroupObject
        property bool isPointer
        property var presets: []
        property bool locked: false
        property var xReading: ({})
        property var yReading: ({})
        property int smallTextSize: 12
        spacing: 16

        RowLayout {
            Layout.fillWidth: true
            Label { text: qsTr("Move by") }
            ComboBox {
                objectName: "preset"
                Layout.fillWidth: true
                model: editor.presets.concat([{ id: "", name: qsTr("Custom") }])
                textRole: "name"
                valueRole: "id"
                // Reads count so it's worked out again once the model is in.
                currentIndex: count > 0 ? Math.max(0, indexOfValue(editor.group.preset)) : -1
                enabled: !editor.locked
                onActivated: {
                    if (currentValue !== "")
                        editor.group.applyPreset(currentValue)
                }
            }
            Label { text: qsTr("Mode") }
            ComboBox {
                objectName: "mode"
                Layout.preferredWidth: 160
                model: editor.isPointer
                       ? [{ value: "relative", text: qsTr("Relative") },
                          { value: "joystick", text: qsTr("Joystick") },
                          { value: "absolute", text: qsTr("Absolute") }]
                       : [{ value: "relative", text: qsTr("Relative") },
                          { value: "joystick", text: qsTr("Joystick") }]
                textRole: "text"
                valueRole: "value"
                currentIndex: count > 0 ? Math.max(0, indexOfValue(editor.group.mode)) : -1
                enabled: !editor.locked
                onActivated: editor.group.mode = currentValue
            }
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            opacity: 0.7
            font.pixelSize: editor.smallTextSize
            text: editor.group.mode === "joystick"
                  ? qsTr("Joystick: hold your head away from the center and the movement keeps going.")
                  : editor.group.mode === "absolute"
                    ? qsTr("Absolute: where your head points is where the pointer goes. Desktop only; games ignore it.")
                    : qsTr("Relative: the movement follows your head, like a trackpad.")
        }

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: qsTr("Deadzone: %1").arg(editor.group.deadzone.toFixed(2))
                Layout.preferredWidth: Math.max(130, implicitWidth)
            }
            Slider {
                objectName: "deadzone"
                Layout.fillWidth: true
                from: 0
                to: 0.5
                stepSize: 0.01
                value: editor.group.deadzone
                enabled: !editor.locked
                onMoved: editor.group.deadzone = value
            }
            Label {
                text: qsTr("Smoothing: %1").arg(editor.group.smoothing.toFixed(2))
                Layout.preferredWidth: Math.max(130, implicitWidth)
            }
            Slider {
                objectName: "smoothing"
                Layout.fillWidth: true
                from: 0
                to: 1
                stepSize: 0.05
                value: editor.group.smoothing
                enabled: !editor.locked
                onMoved: editor.group.smoothing = value
            }
        }

        HeadMouseAxisEditor {
            objectName: "xAxis"
            Layout.topMargin: 12
            axis: editor.group.x
            label: editor.isPointer ? qsTr("Left and right") : qsTr("Scroll sideways")
            reading: editor.xReading
            locked: editor.locked
            smallTextSize: editor.smallTextSize
        }
        HeadMouseAxisEditor {
            objectName: "yAxis"
            Layout.topMargin: 20
            axis: editor.group.y
            label: editor.isPointer ? qsTr("Up and down") : qsTr("Scroll up and down")
            reading: editor.yReading
            locked: editor.locked
            smallTextSize: editor.smallTextSize
        }
    }
}

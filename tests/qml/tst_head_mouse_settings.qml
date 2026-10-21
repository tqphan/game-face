import QtQuick
import QtQuick.Controls
import QtTest
import "../../app/ui/qml"

// The head mouse settings page with stand-ins for App's objects
// (docs/head-mouse-plan.md, 8.4).
Item {
    id: root
    width: 900
    height: 1400

    readonly property var presetList: [
        { id: "headTurn", name: "Head turn", x: "headYaw", y: "headPitch" },
        { id: "headPosition", name: "Head position", x: "headX", y: "headY" },
        { id: "nosePoint", name: "Nose point", x: "noseX", y: "noseY" },
        { id: "tiltAndNod", name: "Tilt and nod", x: "headRoll", y: "headPitch" }
    ]

    component MockAxis: QtObject {
        property string source: ""
        property string error: ""
        property bool hasNeutral: false
        property real neutral: 0
        property real rangeNegative: 20
        property real rangePositive: 20
        property real gain: 1
        property bool invert: false
        function clearNeutral() { hasNeutral = false }
    }

    component MockGroup: QtObject {
        id: group
        property string mode: "relative"
        property real deadzone: 0.05
        property real smoothing: 0.5
        property MockAxis x: MockAxis {}
        property MockAxis y: MockAxis {}
        property var applied: []
        readonly property string preset: {
            for (const p of root.presetList) {
                if (p.x === x.source && p.y === y.source)
                    return p.id
            }
            return ""
        }
        function applyPreset(id) {
            applied = applied.concat([id])
            for (const p of root.presetList) {
                if (p.id === id) {
                    x.source = p.x
                    y.source = p.y
                }
            }
        }
    }

    QtObject {
        id: mock
        property bool enabled: false
        property real clickFreezeMs: 150
        property MockGroup pointer: MockGroup {
            x: MockAxis { source: "headYaw" }
            y: MockAxis { source: "headPitch"; rangeNegative: 15; rangePositive: 15 }
        }
        property MockGroup scroll: MockGroup { mode: "joystick" }
    }

    HeadMouseSettings {
        id: page
        width: root.width
        headMouse: mock
        presets: root.presetList
        headSignalNames: ["headYaw", "headPitch", "headRoll", "headX", "headY", "headZ", "noseX", "noseY"]
        tracking: true
    }

    SignalSpy { id: recenterSpy; target: page; signalName: "recenterRequested" }
    SignalSpy { id: calibrateSpy; target: page; signalName: "calibrateRequested" }

    TestCase {
        name: "HeadMouseSettings"
        when: windowShown

        function init() {
            mock.enabled = false
            mock.pointer.x.source = "headYaw"
            mock.pointer.y.source = "headPitch"
            mock.pointer.x.error = ""
            mock.pointer.mode = "relative"
            mock.pointer.applied = []
            page.locked = false
            page.tracking = true
            page.calibrating = false
            page.readings = []
            recenterSpy.clear()
            calibrateSpy.clear()
        }

        function pointer() { return findChild(page, "pointerGroup") }
        function scroll() { return findChild(page, "scrollGroup") }

        function test_enabledCheckbox() {
            const box = findChild(page, "enabled")
            verify(box)
            compare(box.checked, false)
            mouseClick(box)
            compare(mock.enabled, true)
            mock.enabled = false
            compare(box.checked, false)
        }

        function test_presetShowsAndApplies() {
            const combo = findChild(pointer(), "preset")
            compare(combo.currentText, "Head turn")
            combo.currentIndex = 1
            combo.activated(1)
            compare(mock.pointer.applied, ["headPosition"])
            compare(mock.pointer.x.source, "headX")
            compare(combo.currentText, "Head position")
        }

        function test_customSourcesShowCustom() {
            mock.pointer.x.source = "headYaw + 2 * headX"
            compare(findChild(pointer(), "preset").currentText, "Custom")
        }

        function test_editingTheSource() {
            const field = findChild(findChild(pointer(), "xAxis"), "source")
            compare(field.text, "headYaw")
            field.text = "  headRoll "
            field.editingFinished()
            compare(mock.pointer.x.source, "headRoll")
        }

        function test_sourceError() {
            const error = findChild(findChild(pointer(), "xAxis"), "error")
            verify(!error.visible)
            mock.pointer.x.error = "unknown name 'headYa'"
            verify(error.visible)
            compare(error.text, "unknown name 'headYa'")
        }

        function test_modes() {
            compare(findChild(pointer(), "mode").count, 3)
            compare(findChild(scroll(), "mode").count, 2)  // no absolute scrolling
            compare(findChild(scroll(), "mode").currentValue, "joystick")
            const mode = findChild(pointer(), "mode")
            mode.currentIndex = 1
            mode.activated(1)
            compare(mock.pointer.mode, "joystick")
        }

        function test_sliders() {
            const gain = findChild(findChild(pointer(), "xAxis"), "gain")
            compare(gain.value, 1)
            gain.value = 2.5
            gain.moved()
            compare(mock.pointer.x.gain, 2.5)
            const invert = findChild(findChild(pointer(), "yAxis"), "invert")
            mouseClick(invert)
            compare(mock.pointer.y.invert, true)
            mock.pointer.y.invert = false
        }

        function test_ranges() {
            const below = findChild(findChild(pointer(), "yAxis"), "rangeNegative")
            compare(Number(below.text), 15)
            below.text = "9.5"
            below.editingFinished()
            compare(mock.pointer.y.rangeNegative, 9.5)
            mock.pointer.y.rangeNegative = 15
        }

        function test_readings() {
            const label = findChild(findChild(pointer(), "xAxis"), "reading")
            compare(label.text, "No reading")
            page.readings = [{ active: true, raw: 12.5, normalised: 0.6 },
                             { active: true, raw: -3, normalised: -0.2 },
                             { active: false, raw: 0, normalised: 0 },
                             { active: false, raw: 0, normalised: 0 }]
            verify(label.text.indexOf("12.50") >= 0, label.text)
            verify(label.text.indexOf("not set") >= 0, label.text)
            mock.pointer.x.hasNeutral = true
            mock.pointer.x.neutral = 1.25
            verify(label.text.indexOf("1.25") >= 0, label.text)
            mock.pointer.x.hasNeutral = false
        }

        function test_recenterAndCalibrate() {
            mouseClick(findChild(page, "recenter"))
            compare(recenterSpy.count, 1)
            mouseClick(findChild(page, "calibrate"))
            compare(calibrateSpy.count, 1)

            page.calibrating = true
            verify(findChild(page, "calibratingHint").visible)
            verify(!findChild(page, "calibrate").enabled)

            page.calibrating = false
            page.tracking = false  // nothing to recenter on
            verify(!findChild(page, "recenter").enabled)
            verify(!findChild(page, "calibrate").enabled)
        }

        function test_lockedDisablesEverything() {
            page.locked = true
            for (const name of ["enabled", "recenter", "calibrate", "clickFreeze"])
                verify(!findChild(page, name).enabled, name)
            for (const name of ["preset", "mode", "deadzone", "smoothing"])
                verify(!findChild(pointer(), name).enabled, name)
            const axis = findChild(pointer(), "xAxis")
            for (const name of ["source", "gain", "invert", "rangeNegative", "rangePositive"])
                verify(!findChild(axis, name).enabled, name)
        }

        function test_clickFreeze() {
            const slider = findChild(page, "clickFreeze")
            compare(slider.value, 150)
            slider.value = 300
            slider.moved()
            compare(mock.clickFreezeMs, 300)
        }
    }
}

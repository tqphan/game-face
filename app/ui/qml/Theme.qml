pragma Singleton
import QtQuick
import QtQuick.Controls.Material
import GameFace

// Fill colours for highlighted (filled) buttons. Material's dark theme uses
// light accent shades, which are too pale behind white button text; these
// darker shades keep white text at WCAG AA contrast (4.5:1 or better) in both
// themes. Flat buttons keep the regular accent, since their text sits on the
// window background.
QtObject {
    readonly property color primaryButton: Material.color(Material.Teal, Material.Shade700)   // 5.3:1
    readonly property color positiveButton: Material.color(Material.Green, Material.Shade800) // 5.1:1
    readonly property color dangerButton: Material.color(Material.Red, Material.Shade700)     // 5.0:1
    // Lock UI while locked: a colour no other button uses.
    readonly property color lockedButton: Material.color(Material.DeepOrange, Material.Shade900) // 5.6:1

    // Text sizes, scaled by Settings > Display > Text size. Windows set
    // textSize as their font, which every control inherits.
    readonly property real textScale: App.settings.textScale
    readonly property int textSize: Math.round(14 * textScale)
    readonly property int smallTextSize: Math.round(12 * textScale)
    readonly property int largeTextSize: Math.round(18 * textScale)
    readonly property int titleTextSize: Math.round(22 * textScale)

    // Layer behind modal dialogs in the dark theme (Material's is too light).
    readonly property color darkModalDim: Qt.rgba(0, 0, 0, 0.7)
}

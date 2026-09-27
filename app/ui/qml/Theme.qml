pragma Singleton
import QtQuick
import QtQuick.Controls.Material

// Fill colours for highlighted (filled) buttons. Material's dark theme uses
// light accent shades, which are too pale behind white button text; these
// darker shades keep white text at WCAG AA contrast (4.5:1 or better) in both
// themes. Flat buttons keep the regular accent, since their text sits on the
// window background.
QtObject {
    readonly property color primaryButton: Material.color(Material.Teal, Material.Shade700)   // 5.3:1
    readonly property color positiveButton: Material.color(Material.Green, Material.Shade800) // 5.1:1
    readonly property color dangerButton: Material.color(Material.Red, Material.Shade700)     // 5.0:1
}

import QtQuick.Controls
import QtQuick.Controls.Material

// Button with a small corner radius. Material's Button sets
// `Material.roundedScale: Material.FullScale` itself (pill shape), so the
// value can't be inherited from a parent; it has to be set on each button.
Button {
    Material.roundedScale: Material.ExtraSmallScale
}

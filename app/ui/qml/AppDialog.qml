pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// Dialog with a darker dim layer in the dark theme. Material sets
// Overlay.modal on each popup itself, so it can't be changed from the window,
// and its dark-theme dim colour is a light grey (#99fafafa) that brightens
// the window instead of dimming it.
Dialog {
    id: control

    Overlay.modal: Rectangle {
        color: control.Material.theme === Material.Dark ? Theme.darkModalDim
                                                        : control.Material.backgroundDimColor
        Behavior on opacity { NumberAnimation { duration: 150 } }
    }
}

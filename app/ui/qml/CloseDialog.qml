import QtQuick
import QtQuick.Controls

// Asks before closing; closes by itself after 10 seconds (as the original
// app does).
Dialog {
    id: dialog

    signal confirmed()

    property int secondsLeft: 10

    anchors.centerIn: Overlay.overlay
    modal: true
    title: qsTr("Close game-face?")
    standardButtons: Dialog.Yes | Dialog.No

    onOpened: {
        secondsLeft = 10
        countdown.start()
    }
    onClosed: countdown.stop()
    onAccepted: confirmed()

    Label {
        text: qsTr("game-face will close in %n second(s).", "", dialog.secondsLeft)
    }

    Timer {
        id: countdown
        interval: 1000
        repeat: true
        onTriggered: {
            dialog.secondsLeft -= 1
            if (dialog.secondsLeft <= 0)
                dialog.accept()
        }
    }
}

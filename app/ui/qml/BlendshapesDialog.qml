pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import GameFace

// Live scores (0..100) for all 52 blendshapes, in as many columns as fit, read
// top to bottom so the list keeps MediaPipe's order down each column.
AppDialog {
    anchors.centerIn: Overlay.overlay
    width: Overlay.overlay ? Overlay.overlay.width - 40 : 640
    height: Overlay.overlay ? Overlay.overlay.height - 40 : 600
    modal: true
    title: qsTr("Blendshapes")
    standardButtons: Dialog.Close
    footer: AppDialogButtons {}

    Flickable {
        id: flick
        anchors.fill: parent
        clip: true
        contentWidth: width
        contentHeight: grid.height
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        readonly property int minColumnWidth: 320
        readonly property int targetRowHeight: 40
        readonly property int minRowHeight: 28
        readonly property int maxRowHeight: 64
        readonly property int count: repeater.count
        // The fewest columns whose rows fit the height at about targetRowHeight
        // (more only if they'd be shorter than minRowHeight), but none narrower
        // than minColumnWidth; rows then grow to fill the height. Scrolls only
        // when even the most columns don't fit.
        readonly property int maxColumns: Math.max(1, Math.floor((width - 16) / minColumnWidth))
        readonly property int columns: Math.max(1, Math.min(maxColumns,
            Math.ceil(count / Math.max(1, Math.floor(height / targetRowHeight)))))
        readonly property int rows: Math.max(1, Math.ceil(count / columns))
        readonly property real rowHeight: Math.max(minRowHeight, Math.min(maxRowHeight, height / rows))

        Grid {
            id: grid
            width: flick.width - 16
            flow: Grid.TopToBottom
            rows: flick.rows
            columnSpacing: 24

            Repeater {
                id: repeater
                model: App.blendshapes

                delegate: RowLayout {
                    required property string name
                    required property real value
                    width: (grid.width - grid.columnSpacing * (flick.columns - 1)) / flick.columns
                    height: flick.rowHeight
                    spacing: 12

                    Label {
                        Layout.preferredWidth: 160
                        horizontalAlignment: Text.AlignRight
                        elide: Text.ElideRight
                        text: parent.name
                    }
                    // Material's bar is a 4px line; this one thickens with the row.
                    ProgressBar {
                        id: bar
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.round(flick.rowHeight * 0.4)
                        from: 0
                        to: 100
                        value: parent.value
                        background: Rectangle {
                            radius: height / 2
                            color: Qt.alpha(bar.Material.accentColor, 0.2)
                        }
                        contentItem: Item {
                            Rectangle {
                                width: bar.visualPosition * parent.width
                                height: parent.height
                                radius: height / 2
                                color: bar.Material.accentColor
                            }
                        }
                    }
                    Label {
                        Layout.preferredWidth: 28
                        horizontalAlignment: Text.AlignRight
                        text: Math.round(parent.value)
                    }
                }
            }
        }
    }
}

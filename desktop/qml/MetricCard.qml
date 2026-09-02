import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    required property string title
    required property string value
    required property string detail
    required property real progress
    property color accent: "#5eead4"

    color: "#151b2d"
    radius: 24
    border.color: "#27314d"
    border.width: 1

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 28
        spacing: 14

        Label {
            text: root.title
            color: "#91a0bd"
            font.pixelSize: 15
            font.weight: Font.DemiBold
        }

        Label {
            text: root.value
            color: "#f8fafc"
            font.pixelSize: 44
            font.weight: Font.Bold
        }

        Label {
            text: root.detail
            color: "#9aa8c1"
            font.pixelSize: 14
        }

        Item { Layout.fillHeight: true }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 10
            radius: 5
            color: "#27314d"

            Rectangle {
                width: parent.width * Math.max(0, Math.min(1, root.progress))
                height: parent.height
                radius: parent.radius
                color: root.accent

                Behavior on width {
                    NumberAnimation { duration: 320; easing.type: Easing.OutCubic }
                }
            }
        }
    }
}

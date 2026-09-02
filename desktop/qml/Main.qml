import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window

    width: 1180
    height: 720
    minimumWidth: 900
    minimumHeight: 620
    visible: true
    title: "AI PC Guardian"
    color: "#0b1020"

    SystemMetrics {
        id: metrics
    }

    Rectangle {
        anchors.fill: parent
        color: "#0b1020"

        Rectangle {
            width: 440
            height: 440
            radius: 220
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.rightMargin: -170
            anchors.topMargin: -210
            color: "#152c4f"
            opacity: 0.52
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 44
            spacing: 26

            RowLayout {
                Layout.fillWidth: true
                spacing: 18

                ColumnLayout {
                    spacing: 5

                    Label {
                        text: "AI PC GUARDIAN"
                        color: "#5eead4"
                        font.pixelSize: 13
                        font.weight: Font.Bold
                        font.letterSpacing: 2.2
                    }

                    Label {
                        text: "Состояние компьютера"
                        color: "#f8fafc"
                        font.pixelSize: 31
                        font.weight: Font.Bold
                    }
                }

                Item { Layout.fillWidth: true }

                Rectangle {
                    implicitWidth: statusRow.implicitWidth + 32
                    implicitHeight: 42
                    radius: 21
                    color: "#162438"
                    border.color: "#29415b"

                    RowLayout {
                        id: statusRow
                        anchors.centerIn: parent
                        spacing: 10

                        Rectangle {
                            implicitWidth: 9
                            implicitHeight: 9
                            radius: 5
                            color: !metrics.agentAvailable
                                   ? "#fb7185"
                                   : metrics.refreshing || metrics.dataStale
                                     ? "#fbbf24"
                                     : "#5eead4"
                        }

                        Label {
                            text: metrics.statusText
                            color: "#dbeafe"
                            font.pixelSize: 13
                        }
                    }
                }
            }

            GridLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 275
                columns: 2
                columnSpacing: 22

                MetricCard {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    title: "ПРОЦЕССОР"
                    value: metrics.cpuUsage.toFixed(1) + "%"
                    detail: "Общая загрузка CPU"
                    progress: metrics.cpuUsage / 100
                    accent: metrics.cpuUsage >= 85 ? "#fb7185" : "#5eead4"
                }

                MetricCard {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    title: "ОПЕРАТИВНАЯ ПАМЯТЬ"
                    value: metrics.memoryUsage.toFixed(1) + "%"
                    detail: metrics.memoryUsedGiB.toFixed(1) + " из "
                            + metrics.memoryTotalGiB.toFixed(1) + " ГиБ"
                    progress: metrics.memoryUsage / 100
                    accent: metrics.memoryUsage >= 90 ? "#fb7185" : "#60a5fa"
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 170
                radius: 24
                color: "#11182a"
                border.color: "#27314d"

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 28
                    spacing: 24

                    Rectangle {
                        implicitWidth: 58
                        implicitHeight: 58
                        radius: 18
                        color: healthy ? "#123d3a" : "#4a2430"

                        property bool healthy: metrics.cpuUsage < 85
                                               && metrics.memoryUsage < 90

                        Label {
                            anchors.centerIn: parent
                            text: parent.healthy ? "OK" : "!"
                            color: parent.healthy ? "#5eead4" : "#fb7185"
                            font.pixelSize: 17
                            font.weight: Font.Bold
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Label {
                            text: metrics.cpuUsage < 85 && metrics.memoryUsage < 90
                                  ? "Критических отклонений не видно"
                                  : "Обнаружена высокая нагрузка"
                            color: "#f8fafc"
                            font.pixelSize: 20
                            font.weight: Font.DemiBold
                        }

                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: "Это первый локальный диагностический экран. "
                                  + "Следующие этапы добавят историю, причины "
                                  + "отклонений и безопасные рекомендации."
                            color: "#91a0bd"
                            font.pixelSize: 14
                            lineHeight: 1.25
                        }
                    }

                    Button {
                        id: refreshButton

                        implicitWidth: 132
                        implicitHeight: 44
                        text: metrics.refreshing ? "Обновление..." : "Обновить"
                        enabled: metrics.agentAvailable && !metrics.refreshing
                        onClicked: metrics.refresh()

                        contentItem: Label {
                            text: refreshButton.text
                            color: refreshButton.enabled ? "#07111f" : "#64748b"
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            radius: 14
                            color: !refreshButton.enabled
                                   ? "#27314d"
                                   : refreshButton.down
                                     ? "#2dd4bf"
                                     : refreshButton.hovered
                                       ? "#99f6e4"
                                       : "#5eead4"
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true

                Label {
                    text: metrics.dataStale
                          ? "Данные Agent устарели"
                          : "Локальные метрики через Agent IPC"
                    color: "#64748b"
                    font.pixelSize: 12
                }

                Item { Layout.fillWidth: true }

                Label {
                    text: "Обновлено: " + metrics.lastUpdated
                    color: "#64748b"
                    font.pixelSize: 12
                }
            }
        }
    }
}

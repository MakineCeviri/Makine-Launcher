import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * ScanStep.qml - Second step of onboarding wizard
 * Scans Steam, Epic, GOG libraries for installed games.
 */
Item {
    id: scanStep

    signal nextStep()
    signal previousStep()

    // Scan state
    property bool isScanning: false
    property bool scanDone: false
    property int foundGames: 0
    property int scanStage: 0  // 0=Steam, 1=Epic, 2=GOG, 3=done

    readonly property var scanStageTexts: [
        qsTr("Steam k\u00fct\u00fcphanesi aran\u0131yor..."),
        qsTr("Epic Games aran\u0131yor..."),
        qsTr("GOG aran\u0131yor...")
    ]

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 0
        width: Math.min(parent.width, 460)

        Text {
            textFormat: Text.PlainText
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Oyunlar\u0131m\u0131 Bul")
            font.pixelSize: 24
            font.weight: Font.Bold
            font.letterSpacing: -0.3
            color: "#FFFFFF"
        }

        Item { Layout.preferredHeight: 8 }

        Text {
            textFormat: Text.PlainText
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Kurulu oyunlar\u0131 tespit et")
            font.pixelSize: 14
            color: Qt.rgba(1, 1, 1, 0.5)
        }

        Item { Layout.preferredHeight: 32 }

        // Scan button or result
        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 360
            Layout.preferredHeight: scanContent.implicitHeight + 40
            radius: 12
            color: Qt.rgba(1, 1, 1, 0.03)
            border.color: Qt.rgba(1, 0.42, 0.62, 0.12)
            border.width: 1

            ColumnLayout {
                id: scanContent
                anchors.centerIn: parent
                width: parent.width - 40
                spacing: 16

                // Not scanned yet
                ColumnLayout {
                    visible: !scanStep.isScanning && !scanStep.scanDone
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 12

                    Text {
                        textFormat: Text.PlainText
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("Steam, Epic ve GOG oyunlar\u0131n\u0131 arar.")
                        font.pixelSize: 13
                        color: Qt.rgba(1, 1, 1, 0.5)
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                    }

                    Button {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: 180
                        Layout.preferredHeight: 40

                        contentItem: Text {
                            textFormat: Text.PlainText
                            text: qsTr("Taramay\u0131 Ba\u015flat")
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                            color: "#FFFFFF"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            radius: 8
                            color: parent.parent.hovered ? "#E04898" : "#D63D8C"
                        }

                        onClicked: scanStep.startScan()
                    }
                }

                // Scanning in progress
                ColumnLayout {
                    visible: scanStep.isScanning
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 12

                    BusyIndicator {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                        running: scanStep.isScanning
                    }

                    Text {
                        textFormat: Text.PlainText
                        Layout.alignment: Qt.AlignHCenter
                        text: scanStep.scanStage < scanStep.scanStageTexts.length
                              ? scanStep.scanStageTexts[scanStep.scanStage]
                              : scanStep.scanStageTexts[scanStep.scanStageTexts.length - 1]
                        font.pixelSize: 14
                        color: Qt.rgba(1, 1, 1, 0.5)

                        Behavior on text {
                            SequentialAnimation {
                                NumberAnimation { target: parent; property: "opacity"; to: 0; duration: 120 }
                                PropertyAction {}
                                NumberAnimation { target: parent; property: "opacity"; to: 1; duration: 120 }
                            }
                        }
                    }
                }

                // Scan completed
                ColumnLayout {
                    visible: scanStep.scanDone
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 8

                    Text {
                        textFormat: Text.PlainText
                        Layout.alignment: Qt.AlignHCenter
                        text: scanStep.foundGames > 0
                              ? qsTr("%1 oyun tespit edildi").arg(scanStep.foundGames)
                              : qsTr("Oyun bulunamad\u0131")
                        font.pixelSize: 18
                        font.weight: Font.Bold
                        color: scanStep.foundGames > 0 ? Theme.success : Qt.rgba(1, 1, 1, 0.5)
                    }

                    Text {
                        textFormat: Text.PlainText
                        Layout.alignment: Qt.AlignHCenter
                        Layout.maximumWidth: 300
                        text: scanStep.foundGames > 0
                              ? qsTr("Ana ekrandan T\u00fcrk\u00e7e \u00e7evirisi olanlar\u0131 g\u00f6rebilirsin.")
                              : qsTr("Sorun de\u011fil, ana ekrandan klas\u00f6r olarak da ekleyebilirsin.")
                        font.pixelSize: 13
                        color: Qt.rgba(1, 1, 1, 0.5)
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }

        Item { Layout.preferredHeight: 32 }

        // Navigation row
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 12

            Button {
                Layout.preferredWidth: 100
                Layout.preferredHeight: 40
                flat: true

                contentItem: Text {
                    textFormat: Text.PlainText
                    text: qsTr("Geri")
                    font.pixelSize: 14
                    color: parent.hovered ? "#FFFFFF" : Qt.rgba(1, 1, 1, 0.5)
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    radius: 8
                    color: parent.parent.hovered ? Qt.rgba(1, 1, 1, 0.06) : "transparent"
                }

                onClicked: scanStep.previousStep()
            }

            Button {
                Layout.preferredWidth: 160
                Layout.preferredHeight: 40

                contentItem: Text {
                    textFormat: Text.PlainText
                    text: qsTr("Devam Et")
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                    color: "#FFFFFF"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    radius: 8
                    color: parent.parent.hovered ? "#E04898" : "#D63D8C"
                }

                onClicked: scanStep.nextStep()
            }
        }
    }

    // Scan logic — extracted to function, not triggered in Component.onCompleted
    function startScan() {
        isScanning = true
        scanStage = 0
        scanStageTimer.start()
        scanTimeoutTimer.start()
        if (typeof GameService !== "undefined") {
            GameService.scanAllLibraries()
        } else {
            isScanning = false
            scanDone = true
            foundGames = 0
            scanStageTimer.stop()
        }
    }

    // Ensure timers stop when leaving this step
    StackLayout.onIsCurrentItemChanged: {
        if (!StackLayout.isCurrentItem) {
            scanStageTimer.stop()
            scanTimeoutTimer.stop()
        }
    }

    // Timer to cycle through scan stages visually
    Timer {
        id: scanStageTimer
        interval: 1200
        repeat: true
        onTriggered: {
            if (scanStep.scanStage < 2)
                scanStep.scanStage++
        }
    }

    // Safety timeout — if scan hangs for 30s, let user proceed
    Timer {
        id: scanTimeoutTimer
        interval: 30000
        repeat: false
        onTriggered: {
            if (scanStep.isScanning) {
                scanStageTimer.stop()
                scanStep.isScanning = false
                scanStep.scanDone = true
                scanStep.foundGames = 0
            }
        }
    }

    Connections {
        target: typeof GameService !== "undefined" ? GameService : null
        function onScanCompleted(count) {
            scanStageTimer.stop()
            scanTimeoutTimer.stop()
            scanStep.isScanning = false
            scanStep.scanDone = true
            scanStep.foundGames = count
        }
    }
}

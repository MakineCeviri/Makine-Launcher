import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0

/**
 * WelcomeStep.qml - First step of onboarding wizard
 * Shows logo, tagline, and feature cards.
 */
Item {
    id: welcomeStep

    signal nextStep()

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 0
        width: Math.min(parent.width, 500)

        // Logo
        Image {
            Layout.alignment: Qt.AlignHCenter
            source: "qrc:/qt/qml/MakineAI/resources/images/logo.png"
            sourceSize: Qt.size(80, 80)
            fillMode: Image.PreserveAspectFit
        }

        Item { Layout.preferredHeight: 24 }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "MakineAI"
            font.pixelSize: 32
            font.weight: Font.Bold
            font.letterSpacing: -0.5
            color: Theme.textPrimary
        }

        Item { Layout.preferredHeight: 8 }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Oyunlar\u0131n\u0131 T\u00fcrk\u00e7e oyna")
            font.pixelSize: 16
            color: Theme.textSecondary
        }

        Item { Layout.preferredHeight: 40 }

        // Feature cards
        Repeater {
            model: [
                {
                    useTrBadge: true,
                    title: qsTr("T\u00fcrk\u00e7e Yama"),
                    desc: qsTr("Steam, Epic, GOG \u2014 hangisini kullan\u0131yorsan")
                },
                {
                    useTrBadge: false,
                    icon: "qrc:/qt/qml/MakineAI/resources/icons/arrow_right.svg",
                    title: qsTr("T\u00fcrk\u00e7e yapar"),
                    desc: qsTr("Topluluk \u00e7evirilerini kurar, orijinalleri yedekler")
                },
                {
                    useTrBadge: false,
                    icon: "qrc:/qt/qml/MakineAI/resources/icons/shield-check.svg",
                    title: qsTr("Geri al\u0131n\u0131r"),
                    desc: qsTr("Be\u011fenmediysen kald\u0131r, hi\u00e7bir \u015fey bozulmaz")
                }
            ]

            delegate: Rectangle {
                required property var modelData
                required property int index

                Layout.fillWidth: true
                Layout.preferredHeight: 64
                Layout.topMargin: index > 0 ? 8 : 0
                radius: 10
                color: Theme.withAlpha(Theme.surface, 0.5)
                border.color: Theme.glassBorder
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    spacing: 14

                    // TR badge or icon
                    Rectangle {
                        Layout.preferredWidth: 36
                        Layout.preferredHeight: 36
                        radius: 8
                        color: modelData.useTrBadge
                               ? "transparent"
                               : Theme.withAlpha(Theme.primary, 0.08)

                        TurkishFlagBadge {
                            visible: modelData.useTrBadge
                            anchors.centerIn: parent
                            flagWidth: 30; flagHeight: 20
                            radius: 4
                        }

                        Image {
                            visible: !modelData.useTrBadge
                            anchors.centerIn: parent
                            source: modelData.icon || ""
                            sourceSize: Qt.size(18, 18)
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Text {
                            text: modelData.title
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                            color: Theme.textPrimary
                        }
                        Text {
                            Layout.fillWidth: true
                            text: modelData.desc
                            font.pixelSize: 12
                            color: Theme.textSecondary
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }
        }

        Item { Layout.preferredHeight: 36 }

        // Start button
        Button {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 220
            Layout.preferredHeight: 44

            contentItem: Text {
                text: qsTr("Devam Et")
                font.pixelSize: 15
                font.weight: Font.DemiBold
                color: Theme.textOnColor
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            background: Rectangle {
                radius: 8
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: parent.parent.hovered ? Theme.primaryHover : Theme.primary }
                    GradientStop { position: 1.0; color: parent.parent.hovered ? Theme.accentHover : Theme.accent }
                }
            }

            scale: pressed ? 0.97 : 1.0
            Behavior on scale { NumberAnimation { duration: 80 } }

            onClicked: welcomeStep.nextStep()
        }
    }
}

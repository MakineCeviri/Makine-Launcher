import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import MakineAI 1.0

/**
 * DropZoneOverlay.qml - Visual overlay for drag-and-drop file operations
 *
 * Shows an animated overlay when files are dragged over the application window.
 * Supports .mkpkg translation packages, .zip archives, and game folders.
 */
Item {
    id: root

    property bool active: false
    property var droppedUrls: []
    property string dropType: "unknown" // package, archive, folder, unknown

    signal filesDropped(var urls)

    visible: active
    z: 100

    // Determine drop type from file extensions
    function classifyDrop(urls) {
        if (!urls || urls.length === 0) return "unknown"

        for (var i = 0; i < urls.length; i++) {
            var url = urls[i].toString().toLowerCase()
            if (url.endsWith(".mkpkg")) return "package"
            if (url.endsWith(".zip") || url.endsWith(".rar") || url.endsWith(".7z")) return "archive"
        }

        // Check if it might be a folder (no extension in the URL)
        var first = urls[0].toString()
        if (first.indexOf(".") === -1 || first.endsWith("/") || first.endsWith("\\")) {
            return "folder"
        }

        return "unknown"
    }

    // Background dimmer
    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.6)
        opacity: root.active ? 1.0 : 0.0

        Behavior on opacity {
            NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
        }
    }

    // Central drop zone
    Item {
        anchors.centerIn: parent
        width: 400
        height: 280
        scale: root.active ? 1.0 : 0.9
        opacity: root.active ? 1.0 : 0.0

        Behavior on scale {
            NumberAnimation { duration: 250; easing.type: Easing.OutBack }
        }
        Behavior on opacity {
            NumberAnimation { duration: 200 }
        }

        // Dashed border
        Rectangle {
            anchors.fill: parent
            radius: Dimensions.radiusLG
            color: Theme.withAlpha(Theme.surface, 0.9)
            border.color: Theme.withAlpha(Theme.primary, 0.5)
            border.width: 2

            // Animated border glow
            Rectangle {
                anchors.fill: parent
                anchors.margins: -2
                radius: parent.radius + 2
                color: "transparent"
                border.color: Theme.withAlpha(Theme.primary, pulseAnim.opacity * 0.3)
                border.width: 3

                NumberAnimation on opacity {
                    id: pulseAnim
                    property real opacity: 0
                    running: root.active
                    loops: Animation.Infinite
                    from: 0.3; to: 1.0
                    duration: 1200
                    easing.type: Easing.InOutSine
                }
            }
        }

        ColumnLayout {
            anchors.centerIn: parent
            spacing: 16

            // Drop icon
            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 64
                Layout.preferredHeight: 64
                radius: 16
                color: Theme.withAlpha(Theme.primary, 0.1)

                Text {
                    anchors.centerIn: parent
                    font.pixelSize: 28
                    text: {
                        switch(root.dropType) {
                            case "package": return "\uD83D\uDCE6" // package
                            case "archive": return "\uD83D\uDDDC" // file cabinet
                            case "folder": return "\uD83D\uDCC2" // open folder
                            default: return "\u2B07" // down arrow
                        }
                    }

                    SequentialAnimation on y {
                        running: root.active
                        loops: Animation.Infinite
                        NumberAnimation { from: -2; to: 2; duration: 800; easing.type: Easing.InOutSine }
                        NumberAnimation { from: 2; to: -2; duration: 800; easing.type: Easing.InOutSine }
                    }
                }
            }

            // Title
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: {
                    switch(root.dropType) {
                        case "package": return qsTr("Çeviri Paketini Yükle")
                        case "archive": return qsTr("Arşivi Aç ve Analiz Et")
                        case "folder": return qsTr("Oyun Klasörünü Ekle")
                        default: return qsTr("Dosyayı Buraya Bırakın")
                    }
                }
                font.pixelSize: 18
                font.weight: Font.DemiBold
                color: Theme.textPrimary
            }

            // Description
            Text {
                Layout.alignment: Qt.AlignHCenter
                Layout.maximumWidth: 300
                horizontalAlignment: Text.AlignHCenter
                text: {
                    switch(root.dropType) {
                        case "package": return qsTr(".mkpkg çeviri paketi algılandı.\nBırakarak yüklemeyi başlatın.")
                        case "archive": return qsTr("Arşiv dosyası algılandı.\nİçerik analiz edilecek.")
                        case "folder": return qsTr("Oyun klasörü algılandı.\nOtomatik algılama yapılacak.")
                        default: return qsTr("Çeviri paketi, arşiv veya\noyun klasörünü sürükleyin.")
                    }
                }
                font.pixelSize: 12
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
                lineHeight: 1.3
            }

            // Supported formats badge
            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 8

                Repeater {
                    model: [".mkpkg", ".zip", ".rar"]

                    Rectangle {
                        width: formatText.implicitWidth + 12
                        height: 22
                        radius: Dimensions.radiusXS
                        color: Theme.withAlpha(Theme.primary, 0.08)
                        border.color: Theme.withAlpha(Theme.primary, 0.2)
                        border.width: 1

                        Text {
                            id: formatText
                            anchors.centerIn: parent
                            text: modelData
                            font.pixelSize: 10
                            font.weight: Font.Medium
                            font.family: "monospace"
                            color: Theme.primary
                        }
                    }
                }
            }
        }
    }

    // The actual DropArea
    DropArea {
        id: dropArea
        anchors.fill: parent

        onEntered: function(drag) {
            root.active = true
            root.dropType = root.classifyDrop(drag.urls)
        }

        onExited: {
            root.active = false
            root.dropType = "unknown"
        }

        onDropped: function(drop) {
            root.active = false
            if (drop.urls && drop.urls.length > 0) {
                root.droppedUrls = drop.urls
                root.filesDropped(drop.urls)
                drop.accept()
            }
            root.dropType = "unknown"
        }
    }
}

import QtQuick
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

Item {
    id: root

    property string gameId: ""
    property string gameName: ""
    property string steamAppId: ""
    property string installPath: ""

    signal clicked()

    readonly property real _aspectRatio: 130.0 / 185.0
    readonly property int _minHeight: 100
    width: Math.round(height * _aspectRatio)
    height: Math.max(_minHeight, Dimensions.cardHeight)

    // Resolved file:// URL from ImageCacheManager
    property string _src: ""

    function _resolve() {
        var id = steamAppId || gameId
        if (!id) { _src = ""; return }
        _src = ImageCache.resolve(id)
    }

    onSteamAppIdChanged: _resolve()
    Component.onCompleted: _resolve()
    ListView.onPooled: _src = ""
    ListView.onReused: _resolve()

    // Listen for download completions — only when image not yet cached
    Connections {
        target: root._src === "" ? ImageCache : null
        function onImageReady(readyId) {
            if (readyId === (root.steamAppId || root.gameId))
                root._resolve()
        }
    }

    // Placeholder — only for games with no image
    Rectangle {
        anchors.fill: parent
        color: Theme.surfaceLight
        visible: !root._src || img.status === Image.Error
    }

    Image {
        id: img
        anchors.fill: parent
        source: root._src
        fillMode: Image.PreserveAspectCrop
        sourceSize: Qt.size(260, 370)
        asynchronous: true
        mipmap: true
    }

    scale: cardHover.hovered ? 1.03 : 1.0
    Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

    HoverHandler {
        id: cardHover
        cursorShape: Qt.PointingHandCursor
    }
}

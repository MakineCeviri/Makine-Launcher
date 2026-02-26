import QtQuick
import MakineAI 1.0
pragma ComponentBehavior: Bound

Item {
    id: root

    property string gameId: ""
    property string gameName: ""
    property string steamAppId: ""
    property string installPath: ""

    signal clicked()

    readonly property real _aspectRatio: 130.0 / 185.0
    width: Math.round(height * _aspectRatio)
    height: Dimensions.cardHeight

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

    Rectangle {
        anchors.fill: parent
        radius: Dimensions.cardBorderRadius
        color: Theme.surfaceLight
        clip: true

        Image {
            anchors.fill: parent
            source: root._src
            fillMode: Image.PreserveAspectCrop
            sourceSize: Qt.size(260, 370)
            asynchronous: true
            cache: false
        }
    }

    scale: _hovered ? 1.03 : 1.0
    Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

    property bool _hovered: false

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
        onContainsMouseChanged: root._hovered = containsMouse
    }
}

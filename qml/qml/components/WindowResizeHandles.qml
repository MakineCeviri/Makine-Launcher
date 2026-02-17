import QtQuick
import MakineAI 1.0

/**
 * WindowResizeHandles - 8 resize MouseAreas for frameless window edges and corners
 */
Item {
    id: root

    required property var windowRef
    property int resizeMargin: 6

    // Right edge
    MouseArea {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: root.resizeMargin * 2
        anchors.bottomMargin: root.resizeMargin * 2
        width: root.resizeMargin
        cursorShape: Qt.SizeHorCursor
        z: Dimensions.zDialog
        onPressed: root.windowRef.startSystemResize(Qt.RightEdge)
    }

    // Bottom edge
    MouseArea {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: root.resizeMargin * 2
        anchors.rightMargin: root.resizeMargin * 2
        height: root.resizeMargin
        cursorShape: Qt.SizeVerCursor
        z: Dimensions.zDialog
        onPressed: root.windowRef.startSystemResize(Qt.BottomEdge)
    }

    // Left edge
    MouseArea {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: root.resizeMargin * 2
        anchors.bottomMargin: root.resizeMargin * 2
        width: root.resizeMargin
        cursorShape: Qt.SizeHorCursor
        z: Dimensions.zDialog
        onPressed: root.windowRef.startSystemResize(Qt.LeftEdge)
    }

    // Top edge
    MouseArea {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: root.resizeMargin * 2
        anchors.rightMargin: root.resizeMargin * 2
        height: root.resizeMargin
        cursorShape: Qt.SizeVerCursor
        z: Dimensions.zDialog
        onPressed: root.windowRef.startSystemResize(Qt.TopEdge)
    }

    // Bottom-right corner
    MouseArea {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        width: root.resizeMargin * 2
        height: root.resizeMargin * 2
        cursorShape: Qt.SizeFDiagCursor
        z: Dimensions.zWindowControls
        onPressed: root.windowRef.startSystemResize(Qt.RightEdge | Qt.BottomEdge)
    }

    // Bottom-left corner
    MouseArea {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        width: root.resizeMargin * 2
        height: root.resizeMargin * 2
        cursorShape: Qt.SizeBDiagCursor
        z: Dimensions.zWindowControls
        onPressed: root.windowRef.startSystemResize(Qt.LeftEdge | Qt.BottomEdge)
    }

    // Top-right corner
    MouseArea {
        anchors.right: parent.right
        anchors.top: parent.top
        width: root.resizeMargin * 2
        height: root.resizeMargin * 2
        cursorShape: Qt.SizeBDiagCursor
        z: Dimensions.zWindowControls
        onPressed: root.windowRef.startSystemResize(Qt.RightEdge | Qt.TopEdge)
    }

    // Top-left corner
    MouseArea {
        anchors.left: parent.left
        anchors.top: parent.top
        width: root.resizeMargin * 2
        height: root.resizeMargin * 2
        cursorShape: Qt.SizeFDiagCursor
        z: Dimensions.zWindowControls
        onPressed: root.windowRef.startSystemResize(Qt.LeftEdge | Qt.TopEdge)
    }
}

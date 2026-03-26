import QtQuick
import QtQuick.Layouts
import "../theme"
pragma ComponentBehavior: Bound

/**
 * SectionContainer.qml - Reusable glassmorphic section wrapper
 *
 * Provides a dark rounded container with subtle border and ambient glow.
 * All section headers should use this for visual consistency.
 */
Rectangle {
    id: container

    // Content goes here via default property
    default property alias content: contentLayout.data

    // Glow origin: "top-right" or "bottom-left"
    property string glowPosition: "top-right"

    // Expose inner layout for external anchoring/sizing
    property alias contentLayout: contentLayout
    property alias contentSpacing: contentLayout.spacing

    Layout.fillWidth: true
    implicitHeight: contentLayout.implicitHeight + 2 * _padding

    readonly property int _padding: Dimensions.paddingXL
    readonly property int _radius: Dimensions.radiusSection

    radius: _radius
    color: Qt.rgba(0.027, 0.027, 0.04, 0.85)

    GradientBorder { cornerRadius: container._radius }

    // Ambient accent glow
    AmbientGlow {
        anchors.fill: parent
        position: container.glowPosition
    }

    ColumnLayout {
        id: contentLayout
        anchors.fill: parent
        anchors.margins: container._padding
        spacing: 8
    }

    // Edge fades — inline component named SectionEdgeFade to avoid shadowing CatalogSection's inline EdgeFade
    component SectionEdgeFade: Rectangle {
        property bool mirror: false
        anchors { top: parent.top; bottom: parent.bottom; topMargin: 40 }
        width: 28; z: 10; rotation: mirror ? 180 : 0
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: Qt.rgba(0.05, 0.05, 0.065, 0.85) }
            GradientStop { position: 0.4; color: Qt.rgba(0.05, 0.05, 0.065, 0.22) }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }
    SectionEdgeFade { anchors.left: parent.left }
    SectionEdgeFade { anchors.right: parent.right; mirror: true }
}

import QtQuick
import MakineAI 1.0

/**
 * TranslationPhaseBadge.qml - Native Qt TranslationPhaseBadge birebir port
 * Kaynak: ui/src/widgets/translationphasebadge.cpp
 *
 * Features:
 * - Different badge colors per phase
 * - Icon + text
 * - Compact size
 */
Rectangle {
    id: root

    // Translation phases - Native Qt enum
    enum Phase {
        Idle,           // Bekleniyor
        Detecting,      // Tespit ediliyor
        Preparing,      // Hazirlaniyor
        Translating,    // Cevriliyor
        Applying,       // Uygulanıyor
        Completed,      // Tamamlandi
        Failed          // Hata
    }

    property int phase: TranslationPhaseBadge.Phase.Idle

    // Get phase color
    property color phaseColor: {
        switch (phase) {
            case TranslationPhaseBadge.Phase.Idle: return Theme.textMuted
            case TranslationPhaseBadge.Phase.Detecting: return Theme.info
            case TranslationPhaseBadge.Phase.Preparing: return Theme.warning
            case TranslationPhaseBadge.Phase.Translating: return Theme.primary
            case TranslationPhaseBadge.Phase.Applying: return Theme.secondary
            case TranslationPhaseBadge.Phase.Completed: return Theme.success
            case TranslationPhaseBadge.Phase.Failed: return Theme.error
            default: return Theme.textMuted
        }
    }

    // Get phase text
    property string phaseText: {
        switch (phase) {
            case TranslationPhaseBadge.Phase.Idle: return "Bekleniyor"
            case TranslationPhaseBadge.Phase.Detecting: return "Tespit ediliyor"
            case TranslationPhaseBadge.Phase.Preparing: return "Hazirlaniyor"
            case TranslationPhaseBadge.Phase.Translating: return "Cevriliyor"
            case TranslationPhaseBadge.Phase.Applying: return "Uygulanıyor"
            case TranslationPhaseBadge.Phase.Completed: return "Tamamlandi"
            case TranslationPhaseBadge.Phase.Failed: return "Hata"
            default: return "Bilinmiyor"
        }
    }

    // Get phase icon
    property string phaseIcon: {
        switch (phase) {
            case TranslationPhaseBadge.Phase.Idle: return "\u23F3"        // Hourglass
            case TranslationPhaseBadge.Phase.Detecting: return "\uD83D\uDD0D"  // Magnifier
            case TranslationPhaseBadge.Phase.Preparing: return "\u2699"   // Gear
            case TranslationPhaseBadge.Phase.Translating: return "\uD83C\uDF10"  // Globe
            case TranslationPhaseBadge.Phase.Applying: return "\u2B07"    // Down arrow
            case TranslationPhaseBadge.Phase.Completed: return "\u2713"   // Checkmark
            case TranslationPhaseBadge.Phase.Failed: return "\u2717"      // X
            default: return "\u2753"  // Question mark
        }
    }

    implicitWidth: badgeRow.width + 20
    implicitHeight: 26
    radius: Dimensions.radiusStandard
    color: Theme.withAlpha(phaseColor, 0.15)

    Behavior on color { ColorAnimation { duration: 200 } }

    Row {
        id: badgeRow
        anchors.centerIn: parent
        spacing: 6

        Text {
            text: root.phaseIcon
            font.pixelSize: 12
            color: root.phaseColor
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            text: root.phaseText
            font.pixelSize: 11
            font.weight: Font.DemiBold
            color: root.phaseColor
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}

# QML Types Referansi

MakineAI QML tiplerinin ve bilesenlerinin referansi.

---

## Singleton'lar

### Theme

Renk ve stil tanimlari:

```qml
pragma Singleton

QtObject {
    // Ana renkler
    readonly property color primary: "#6366F1"
    readonly property color secondary: "#8B5CF6"
    readonly property color accent: "#F59E0B"

    // Arka plan
    readonly property color background: "#0F0F23"
    readonly property color surface: "#1A1A2E"
    readonly property color surfaceHover: "#252542"
    readonly property color surfaceActive: "#2D2D4A"

    // Metin
    readonly property color textPrimary: "#FFFFFF"
    readonly property color textSecondary: "#A0AEC0"
    readonly property color textMuted: "#718096"

    // Durum renkleri
    readonly property color success: "#10B981"
    readonly property color warning: "#F59E0B"
    readonly property color error: "#EF4444"
    readonly property color info: "#3B82F6"

    // Gradient
    readonly property var primaryGradient: Gradient {
        GradientStop { position: 0.0; color: "#6366F1" }
        GradientStop { position: 1.0; color: "#8B5CF6" }
    }
}
```

**Kullanim:**
```qml
import "theme"

Rectangle {
    color: Theme.surface
}

Text {
    color: Theme.textPrimary
}
```

### Dimensions

Boyut ve spacing tanimlari:

```qml
pragma Singleton

QtObject {
    // Spacing
    readonly property int spacingXS: 4
    readonly property int spacingSM: 8
    readonly property int spacingMD: 16
    readonly property int spacingLG: 24
    readonly property int spacingXL: 32
    readonly property int spacingXXL: 48

    // Border radius
    readonly property int radiusSM: 4
    readonly property int radiusMD: 8
    readonly property int radiusLG: 12
    readonly property int radiusXL: 16
    readonly property int radiusRound: 9999

    // Font sizes
    readonly property int fontXS: 10
    readonly property int fontSM: 12
    readonly property int fontMD: 14
    readonly property int fontLG: 16
    readonly property int fontXL: 20
    readonly property int fontXXL: 24
    readonly property int fontDisplay: 32

    // Component sizes
    readonly property int buttonHeight: 40
    readonly property int inputHeight: 44
    readonly property int iconSM: 16
    readonly property int iconMD: 24
    readonly property int iconLG: 32
}
```

**Kullanim:**
```qml
import "theme"

Rectangle {
    radius: Dimensions.radiusMD

    Text {
        font.pixelSize: Dimensions.fontMD
    }
}

Column {
    spacing: Dimensions.spacingMD
}
```

---

## Components

### GameCard

Oyun karti bileseni:

```qml
GameCard {
    // Properties
    property string gameId
    property string gameName
    property string engineType
    property string coverImage
    property bool hasTranslation: false
    property bool isSelected: false

    // Signals
    signal clicked()
    signal doubleClicked()
    signal contextMenuRequested()
}
```

### GlassCard

Cam efektli kart:

```qml
GlassCard {
    property real blurRadius: 20
    property color backgroundColor: Theme.surface
    property real backgroundOpacity: 0.8
    property int borderRadius: Dimensions.radiusMD
}
```

### NavBar

Navigasyon cubugu:

```qml
NavBar {
    property int currentIndex: 0
    property var items: []  // [{icon, label}]

    signal itemClicked(int index)
}
```

### ActionButton

Aksiyon butonu:

```qml
ActionButton {
    property string text
    property string icon
    property bool primary: true
    property bool loading: false

    signal clicked()
}
```

### ToggleSetting

Toggle ayar bileseni:

```qml
ToggleSetting {
    property string label
    property string description
    property bool checked

    signal toggled(bool value)
}
```

### TranslationProgressBar

Ceviri ilerleme cubugu:

```qml
TranslationProgressBar {
    property real progress: 0  // 0.0 - 1.0
    property string status: ""
    property string phase: "downloading"  // downloading, applying, verifying
}
```

---

## Models

### GameListModel

Oyun listesi modeli:

```cpp
class GameListModel : public QAbstractListModel {
    Q_OBJECT

    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        PathRole,
        EngineRole,
        PlatformRole,
        HasTranslationRole,
        CoverImageRole
    };

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void filter(const QString& query);
    Q_INVOKABLE void sort(const QString& field, bool ascending = true);
};
```

**QML'de Kullanim:**
```qml
ListView {
    model: GameListModel { }

    delegate: GameCard {
        gameName: model.name
        engineType: model.engine
        hasTranslation: model.hasTranslation
    }
}
```

### TranslationModel

Ceviri paketi modeli:

```cpp
class TranslationModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        GameIdRole,
        VersionRole,
        AuthorRole,
        QualityRole,
        SizeRole,
        DownloadedRole
    };

    Q_INVOKABLE void fetchForGame(const QString& gameId);
};
```

---

## Dialogs

### GameDetectorDialog

Oyun algilama dialog'u:

```qml
GameDetectorDialog {
    property var detectedGame: null
    property bool scanning: false

    signal gameAccepted(var game)
    signal gameDismissed()
}
```

### AllGamesDialog

Tum oyunlar dialog'u:

```qml
AllGamesDialog {
    property var games: []
    property string filterQuery: ""

    signal gameSelected(string gameId)
}
```

### QAResultsDialog

Kalite kontrol sonuclari:

```qml
QAResultsDialog {
    property var results: []  // [{type, message, severity}]
    property int warningCount: 0
    property int errorCount: 0
}
```

---

## Ornek Uygulama

```qml
import QtQuick
import QtQuick.Controls
import MakineAI 1.0
import "theme"
import "components"

ApplicationWindow {
    id: root
    width: 1280
    height: 720
    color: Theme.background

    // Services
    GameService { id: gameService }
    SettingsManager { id: settings }

    // Navigation
    NavBar {
        id: navBar
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 240

        items: [
            { icon: "home", label: "Ana Sayfa" },
            { icon: "settings", label: "Ayarlar" }
        ]

        onItemClicked: stackView.currentIndex = index
    }

    // Content
    StackLayout {
        id: stackView
        anchors.left: navBar.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom

        // Home Screen
        HomeScreen {
            games: gameService.games
            onGameClicked: (gameId) => {
                gameService.applyTranslation(gameId, "latest")
            }
        }

        // Settings Screen
        SettingsScreen {
            settings: settings
        }
    }
}
```

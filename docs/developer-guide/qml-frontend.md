# QML Arayüz

Makine-Launcher Qt QML arayüzünün detaylı açıklaması.

---

## Genel Bakış

Makine-Launcher, modern ve performanslı bir arayüz için Qt 6 QML kullanır.

**Teknolojiler:**
- Qt 6.10+ Quick
- Qt Quick Controls
- Custom theme system
- Native Qt components

---

## Klasör Yapısı

```
qml/
├── src/
│   ├── main.cpp              # Giris noktasi
│   ├── services/             # Qt servisleri
│   │   ├── corebridge.h
│   │   ├── gameservice.h
│   │   ├── settingsmanager.h
│   │   ├── backupmanager.h
│   │   ├── localpackagemanager.h
│   │   ├── processscanner.h
│   │   ├── systemtraymanager.h
│   │   ├── integrityservice.h
│   │   ├── batchoperationservice.h
│   │   └── updatedetectionservice.h
│   └── models/               # Qt veri modelleri
│
├── qml/
│   ├── Main.qml              # Ana pencere
│   ├── HomeScreen.qml        # Ana ekran
│   ├── SettingsScreen.qml    # Ayarlar
│   ├── GameDetailScreen.qml  # Oyun detay
│   │
│   ├── controllers/          # QML logic controllers
│   │   └── InstallFlowController.qml
│   │
│   ├── theme/                # Tema sistemi
│   │   ├── Theme.qml         # Renkler (singleton)
│   │   └── Dimensions.qml    # Boyutlar (singleton)
│   │
│   ├── components/           # Yeniden kullanilabilir
│   │   ├── GameCard.qml
│   │   ├── NavItem.qml
│   │   └── ...
│   │
│   └── dialogs/              # Dialog pencereleri
│       ├── AllGamesDialog.qml
│       └── ...
│
└── resources/                # Statik kaynaklar
    ├── icons/
    └── fonts/
```

---

## Theme Sistemi

### Theme.qml (Singleton)

Tüm renkler burada tanımlanır:

```qml
pragma Singleton
import QtQuick

QtObject {
    // Ana renkler
    readonly property color primary: "#6366F1"
    readonly property color secondary: "#8B5CF6"
    readonly property color accent: "#F59E0B"

    // Arka plan
    readonly property color bgPrimary: "#0F0F23"
    readonly property color surface: "#1A1A2E"
    readonly property color surfaceHover: "#252542"

    // Metin
    readonly property color textPrimary: "#FFFFFF"
    readonly property color textSecondary: "#A0AEC0"
    readonly property color textMuted: "#718096"

    // Durum
    readonly property color success: "#10B981"
    readonly property color warning: "#F59E0B"
    readonly property color error: "#EF4444"
}
```

### Dimensions.qml (Singleton)

Tüm boyutlar burada:

```qml
pragma Singleton
import QtQuick

QtObject {
    // Spacing
    readonly property int spacingXS: 4
    readonly property int spacingSM: 8
    readonly property int spacingMD: 16
    readonly property int spacingLG: 24
    readonly property int spacingXL: 32

    // Border radius
    readonly property int radiusSM: 4
    readonly property int radiusMD: 8
    readonly property int radiusLG: 12
    readonly property int radiusXL: 16

    // Font sizes
    readonly property int fontSM: 12
    readonly property int fontMD: 14
    readonly property int fontLG: 16
    readonly property int fontXL: 20
    readonly property int fontXXL: 24
}
```

### Kullanım

```qml
import "theme"

Rectangle {
    color: Theme.surface
    radius: Dimensions.radiusMD

    Text {
        color: Theme.textPrimary
        font.pixelSize: Dimensions.fontMD
    }
}
```

---

## Component Örnekleri

### GameCard.qml

```qml
import QtQuick
import QtQuick.Controls
import "../theme"

Item {
    id: root

    property string gameName
    property string engineType
    property string coverImage
    property bool hasTranslation: false

    signal clicked()

    width: 200
    height: 280

    Rectangle {
        anchors.fill: parent
        color: mouseArea.containsMouse ? Theme.surfaceHover : Theme.surface
        radius: Dimensions.radiusLG

        // Cover image
        Image {
            id: cover
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 200
            source: root.coverImage
            fillMode: Image.PreserveAspectCrop
        }

        // Info
        Column {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: Dimensions.spacingMD
            spacing: Dimensions.spacingXS

            Text {
                text: root.gameName
                color: Theme.textPrimary
                font.pixelSize: Dimensions.fontMD
                font.weight: Font.Medium
                elide: Text.ElideRight
                width: parent.width
            }

            Text {
                text: root.engineType
                color: Theme.textSecondary
                font.pixelSize: Dimensions.fontSM
            }
        }

        // Translation badge
        Rectangle {
            visible: root.hasTranslation
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: Dimensions.spacingSM
            width: 24
            height: 24
            radius: 12
            color: Theme.success

            Text {
                anchors.centerIn: parent
                text: "TR"
                color: "white"
                font.pixelSize: 10
                font.bold: true
            }
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
```

---

## Qt Services

### GameService

```cpp
class GameService : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList games READ games NOTIFY gamesChanged)
    Q_PROPERTY(bool scanning READ isScanning NOTIFY scanningChanged)

public:
    Q_INVOKABLE void scanGames();
    Q_INVOKABLE void applyTranslation(const QString& gameId, const QString& packageId);
    Q_INVOKABLE void removeTranslation(const QString& gameId);

signals:
    void gamesChanged();
    void scanningChanged();
    void translationApplied(const QString& gameId);
    void error(const QString& message);
};
```

### QML'de Kullanım

```qml
import MakineLauncher 1.0

Item {
    GameService {
        id: gameService
        onGamesChanged: gameList.model = games
        onError: errorDialog.show(message)
    }

    Button {
        text: "Tara"
        onClicked: gameService.scanGames()
    }

    ListView {
        id: gameList
        delegate: GameCard {
            gameName: model.name
            onClicked: gameService.applyTranslation(model.id, selectedPackage)
        }
    }
}
```

---

## State Management

### Property Binding

```qml
Rectangle {
    // Reactive binding
    color: gameService.scanning ? Theme.surfaceHover : Theme.surface

    Text {
        text: gameService.games.length + " oyun bulundu"
    }
}
```

### Signal/Slot

```qml
Connections {
    target: gameService

    function onGamesChanged() {
        console.log("Games updated:", gameService.games.length)
    }

    function onError(message) {
        errorDialog.show(message)
    }
}
```

---

## Best Practices

### 1. Theme/Dimensions Kullan

```qml
// YANLIS
Rectangle {
    color: "#1A1A2E"
    radius: 8
}

// DOGRU
Rectangle {
    color: Theme.surface       // veya Theme.bgPrimary
    radius: Dimensions.radiusMD
}
// NOT: Theme.background YOKTUR — Theme.bgPrimary kullanin
```

### 2. ID Kullanımı

```qml
// Sadece gerekli yerlerde
Item {
    id: root  // Root icin kullan

    Rectangle {
        // id gereksiz
    }

    MouseArea {
        id: mouseArea  // Reference edilecekse kullan
    }
}
```

### 3. Anchors vs Layout

```qml
// Basit durumlar icin anchors
Rectangle {
    anchors.fill: parent
    anchors.margins: Dimensions.spacingMD
}

// Kompleks layout'lar icin RowLayout/ColumnLayout
RowLayout {
    spacing: Dimensions.spacingMD

    Button { Layout.fillWidth: true }
    Button { Layout.preferredWidth: 100 }
}
```

---

## Debugging

### Console Log

```qml
Button {
    onClicked: {
        console.log("Button clicked")
        console.log("Games:", JSON.stringify(gameService.games))
    }
}
```

### Qt Creator QML Debugger

1. Debug mode'da çalıştır
2. Debug > Start QML Profiler
3. Binding loops ve performance sorunlarını gör

---

## Sonraki Adımlar

- [Build Sistemi](build-system.md)
- [Test Yazma](testing.md)
- [API Referansı](../api-reference/services-api.md)

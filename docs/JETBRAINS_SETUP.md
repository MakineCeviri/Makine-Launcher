# JetBrains IDE Kurulumu - MakineAI

**Tarih:** 2026-01-19
**Platform:** Windows Desktop Application (Flutter)

---

## Kurulan IDE'ler

| **IDE** | **Versiyon** | **Kullanim** | **Durum** |
|---------|-------------|--------------|-----------|
| JetBrains Toolbox | 3.2.0 | IDE yonetimi | ✅ Kuruldu |
| IntelliJ IDEA Ultimate | 2025.2.5 | Ana Flutter IDE | ✅ Kuruldu |
| Android Studio | 2025.2.3.9 | Flutter SDK + Emulator | ✅ Kuruldu |
| DataGrip | 2025.3.3 | SQLite veritabani yonetimi | ⏳ Kuruluyor |
| WebStorm | 2025.3.1.1 | Web/Electron projeler | ⏳ Kuruluyor |

---

## Gerekli Plugin'ler (IntelliJ IDEA)

### Zorunlu
1. **Flutter** - Flutter development
2. **Dart** - Dart language support

### Onerilen
3. **Database Tools** - SQLite support (built-in Ultimate)
4. **GitToolBox** - Enhanced Git integration
5. **Rainbow Brackets** - Bracket matching
6. **Material Theme UI** - Modern UI
7. **Key Promoter X** - Kisayol ogrenme

---

## Plugin Kurulum Adimlari

1. IntelliJ IDEA'yi ac
2. File → Settings → Plugins
3. Marketplace sekmesinde ara:
   - "Flutter" → Install
   - "Dart" → Install
4. IDE'yi yeniden baslat
5. File → Settings → Languages & Frameworks → Flutter
   - Flutter SDK path: `C:\flutter` veya kurulu dizin

---

## Proje Acma

```
File → Open → C:\cedra\MakineAI
```

Flutter SDK otomatik tanimlanacak.

---

## Faydali Kisayollar (IntelliJ IDEA)

| **Islem** | **Kisayol** |
|-----------|-------------|
| Run | Shift+F10 |
| Debug | Shift+F9 |
| Hot Reload | Ctrl+\ |
| Hot Restart | Ctrl+Shift+\ |
| Format Code | Ctrl+Alt+L |
| Find Usages | Alt+F7 |
| Go to Definition | Ctrl+B |
| Refactor Rename | Shift+F6 |
| Search Everywhere | Double Shift |
| Find in Files | Ctrl+Shift+F |

---

## DataGrip SQLite Baglantisi

1. DataGrip'i ac
2. + → Data Source → SQLite
3. File: `C:\cedra\MakineAI\data\makine.db`
4. Test Connection → Apply

---

## Neden JetBrains?

### VS Code'a Gore Avantajlar:
- Daha guclu refactoring
- Dahili database tooling
- Daha iyi debugging deneyimi
- Kapsamli code analysis
- Native profiling tools
- Superior autocomplete

### MakineAI Icin Ozel Faydalar:
- SQLite browser (DataGrip)
- Multi-module project support
- Advanced Git integration
- Memory profiler (desktop apps)

---

## Lisans

JetBrains All Products Pack veya:
- IntelliJ IDEA Ultimate
- DataGrip
- WebStorm

Hepsi ayni lisansla gelir.

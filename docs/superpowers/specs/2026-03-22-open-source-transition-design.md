# Open Source Transition — Makine-Launcher

**Date:** 2026-03-22
**Status:** Approved
**Author:** jlceaser + Claude

## Summary

Makine-Launcher repository'sini açık kaynak olarak public yapma hazırlığı. Mevcut repo korunur, history temizlenir, topluluk dosyaları eklenir.

## Decisions

| Karar | Seçim |
|-------|-------|
| Lisans | AGPL-3.0 + Commons Clause (değişiklik yok) |
| Repo | Aynı repo (`MakineCeviri/Makine-Launcher`) — yeni repo yok |
| History | Korunur, sadece hassas dosyalar `git filter-repo` ile silinir |
| Katkı modeli | Açık — CONTRIBUTING.md, issue/PR template, CODE_OF_CONDUCT |
| README dili | Başta kısa İngilizce, geri kalan Türkçe |
| README stili | Minimal — logo, açıklama, build, lisans |
| Zamanlama | Bugün hazırla, kullanıcı review edip public yapmaya karar verir |

## Step 1: Git History Temizliği

### Silinecek dosyalar/pattern'ler
- `scripts/.encryption_key`
- `scripts/generate_key_header.py` (XOR obfuscation şemasını açığa çıkarır)
- `qml/src/services/encryption_key.h` (history'de varsa)
- `*.pem`, `*.pfx`, `*.key` (repo genelinde)
- `.env*` (history'de varsa)

### Ön hazırlık
- Mevcut worktree branch'larını temizle (`worktree-agent-*`)
- Stash'leri yedekle (1 stash mevcut)

### Yöntem
```bash
# 1. Backup (repo dizini içinden)
git clone --mirror "$(git remote get-url origin)" ../Makine-Launcher-backup.git

# 2. Filter
pip install git-filter-repo
git filter-repo --invert-paths \
  --path scripts/.encryption_key \
  --path scripts/generate_key_header.py \
  --path qml/src/services/encryption_key.h \
  --path-glob '*.pem' \
  --path-glob '*.pfx' \
  --path-glob '*.key' \
  --path-glob '.env*'

# 3. Remote'u yeniden ekle (filter-repo remote'ları siler)
git remote add origin https://github.com/MakineCeviri/Makine-Launcher.git

# 4. Tüm branch ve tag'leri force push
git push origin --all --force-with-lease
git push origin --tags --force
```

### Sonrasında
- Yeni encryption key üret (local, `.gitignore`'da zaten var)
- `encryption_key.h` working tree'de olmalı (build için gerekli, commit edilmez)
- Tag hash'leri değişecek — release referansları güncellenmeli
- Tüm collaborator'lara re-clone bildirimi

## Step 2: Topluluk Dosyaları

### CONTRIBUTING.md
- Build prerequisites (Qt6, CMake, vcpkg, MinGW)
- Geliştirme ortamı kurulumu
- Commit convention (Conventional Commits)
- PR süreci
- Kod stili kuralları (C++ snake_case/camelCase, QML PascalCase)
- Issue raporlama rehberi

### CODE_OF_CONDUCT.md
- Contributor Covenant v2.1 (Türkçe)

### SECURITY.md
- Güvenlik açığı bildirimi süreci
- Sorumlu ifşa (responsible disclosure) politikası
- İletişim: security email veya GitHub Security Advisories

### .github/ISSUE_TEMPLATE/bug_report.yml
- YAML form formatı (eski markdown değil)
- Alanlar: açıklama, adımlar, beklenen/gerçekleşen davranış, ortam bilgisi

### .github/ISSUE_TEMPLATE/feature_request.yml
- YAML form formatı
- Alanlar: özellik açıklaması, motivasyon, alternatifler

### .github/PULL_REQUEST_TEMPLATE.md
- Değişiklik özeti
- Test planı
- Checklist (build, lint, convention)

## Step 3: README Güncellemesi

### Yapı
```markdown
<!-- 2-3 satır İngilizce -->
> **Makine Launcher** — Turkish game translation launcher...
> For English speakers: brief description here.

---

<!-- Türkçe ana içerik -->
# Makine Launcher

Kısa açıklama...

## Kurulum
## Derleme
## Lisans
```

### İçerik (minimal)
- Logo (varsa)
- Tek paragraf proje açıklaması
- Build komutları (`just dev`, `just run`)
- Prerequisites listesi
- Lisans badge'i
- Katkı linki (CONTRIBUTING.md'ye)

## Step 4: Son Kontrol

- [ ] History'de hassas dosya kalmadığını doğrula
- [ ] Build çalışıyor mu kontrol et
- [ ] Tüm yeni dosyaları kullanıcı review eder
- [ ] Kullanıcı onaylarsa GitHub Settings > Danger Zone > Make public

## Out of Scope

- CI/CD workflow değişiklikleri (zaten güvenli)
- Lisans değişikliği
- Kod refactoring
- Yeni feature ekleme

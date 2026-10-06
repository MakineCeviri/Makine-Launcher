# Makine-Launcher — Claude bağlamı

<!-- Build, mimari, kurallar, tuzaklar → proje kökündeki CLAUDE.md (o da her oturum yüklenir; burada tekrarlama). -->

> **Durum (2026-09-23):** v0.1.5-beta kodu hazır, ZIP üretildi, **yayınlanmadı** · sonraki: v0.2.0 (büyük güncelleme, `docs/v0.2.0-todo.md`)
> **Devralan oturum önce oku: [`docs/handoff-2026-09-23.md`](../docs/handoff-2026-09-23.md)** — durum, kalan yayın adımları, tuzaklar.
> **Blocker:** MSIX imzalı test + Store gönderimi (sertifika yalnız sahibinde).
> **Eski depo:** Makine-Launcher-Dev arşivli (read-only yedek, 2026-05-20).

- **Telemetri:** iki kanal — sayım (`/api/v2/telemetry` → D1) + Sentry `makineceviri/makine-launcher` (Sponsored Team, 50K/ay) · günlük körlük bekçisi (`main`'deki iş akışı) · `just telemetry-check` / `just telemetry-watch` · ayrıntı `docs/telemetry.md`.
- **Operasyonel notlar** (kimlik bilgisi düzeni, D1, push takılması, Sentry kotası…) → auto-memory (`MEMORY.md` dizini her oturum yüklenir).
- **Eski ekip notları** (`.makine` paketleme + R2/D1/CDN yayın süreci, MSIX kimlik/sürümleme, encryption key değişmezliği, Sentry sessiz hata modları — 20 not) → brain `team-memory` katmanı. SCFramework'te öğrenilen Qt tuzakları (`qt-automoc-stale-cache`, `qt-extra-column-proxy-pattern`) burada da ısırır — konuyu ara.
- Yeni öğrenilen → bu dosyaya, kök `CLAUDE.md`'ye ya da `docs/`'a yaz, sonra `bash C:/Workspace/ops/cedra-brain/refresh-brain.sh Makine-Launcher`.

## Bu projede açık araçlar
- **Skill'ler** (`.claude/settings.local.json` yalnız burada açar): `/build` `/test` `/verify` `/format` `/lint` · `/inspect` `/ui-check` `/security-scan` `/deps` `/makine-doctor` · `/changelog` `/manifest-check` `/scan-refs` · `/deploy` `/run` `/release-prep` `/perf-check` (son dördü kullanıcı-tetikli). Qt API: `qt-*` skill'leri (qt-development-skills eklentisi bu projede açık).
- **Agent'lar:** `core-dev` (C++ core, CMake, vcpkg) · `ui-dev` (QML, servisler, tema) · `qa` (yayın öncesi build → test → lint, salt okunur).
- **Korumalar:** `post-edit-check.sh` (QML/C++/CMake düzenlemelerinde otomatik denetim) → git `pre-commit` → `pre-push`. Secret dosyaları (`.env*`, `encryption_key.h`, `scripts/certs/**`, `*.pem` `*.pfx` `*.key` `*.p12`, `credentials.json`) okunmaz, yazılmaz.

# ADR-0008: ManifestSync Service Decomposition

## Status

Accepted

## Date

2026-03-25

## Context

ManifestSyncService was a 675-line monolith handling four distinct responsibilities:
catalog data persistence, per-game detail fetching, telemetry, and sync orchestration.

Code review identified:
- Critical bug: `handleFullCatalogResponse()` missing `m_syncing = false` (sync locked forever)
- Security: `appId` from server response used in file paths without validation
- Quality: sync completion sequence duplicated 3x, CatalogEntry parsing duplicated 2x
- Architecture: coupled telemetry, untestable without network, single growing class

Party Mode debate (Product Champion, Architect, Devil's Advocate) evaluated three approaches:
1. Ship as-is with minor fixes (Product Champion)
2. Full decomposition into 5 classes with Strategy pattern (Architect)
3. Hardening-first, pragmatic refactor (Devil's Advocate)

## Decision

Decompose into 4 classes — Architect's proposal simplified (no Strategy pattern):

| Class | Type | Responsibility | Lines |
|-------|------|----------------|-------|
| `CatalogStore` | Pure C++ | Catalog data, version, ETag, persistence | ~200 |
| `DetailFetcher` | QObject | Per-game detail fetch + 3-tier cache | ~150 |
| `TelemetryService` | QObject | Fire-and-forget session metrics | ~40 |
| `ManifestSyncService` | QObject (facade) | Sync orchestration, state, retry | ~250 |

Strategy pattern rejected: 3 sync paths differ too much to share an interface meaningfully.
The cost (3 extra classes + virtual dispatch) exceeded the benefit for a single-developer project.

Hardening applied:
- `appId` validation via `makine::path::containsTraversalPattern()` in CatalogStore + DetailFetcher
- Delta fail-fast: unknown changeType rejects entire delta, falls back to full catalog
- Atomic version writes: temp file + rename prevents corruption on crash
- 30s sync timeout timer: safety net against `m_syncing` deadlock
- Empty appId guard: skip + warn instead of silent corruption

## Consequences

**Positive:**
- CatalogStore testable with pure unit tests (no QObject, no network mock needed)
- QML API completely unchanged (zero migration cost for consumers)
- Each class under 250 lines — easier to review, reason about, and maintain
- Hardening prevents all 7 failure modes identified by Devil's Advocate review
- Three separate QNetworkAccessManagers enable true parallel requests

**Negative:**
- More files (6 new files: 3 headers + 3 implementations)
- CatalogStore is not QObject — if signals are needed later, promotion required
- Slight memory overhead from 3 QNAM instances (negligible in practice)

## Alternatives Considered

1. **Keep monolith with minor fixes** — Product Champion recommendation. Rejected because
   the security hardening and testability improvements justify decomposition.

2. **Full Strategy pattern with DI** — Architect recommendation. Rejected because the
   3 sync paths (delta, full, legacy) have fundamentally different flows that don't
   share a clean interface. Over-engineering for a single-developer project.

#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Makine Çeviri

"""
telemetry_coverage.py -- Which failure signals never reach Sentry.

Telemetry is captured at the signal level: a service connects its own error
signal to CrashReporter::reportFailure once, and every emit site behind that
signal is covered for free. The weakness of that design is silence — add a new
error signal and nothing complains that it goes nowhere. The failure looks
exactly like a code path that simply never fires.

This walks the service headers for error signals, then checks whether each one
is wired to reportFailure somewhere in the sources. Signals that are
deliberately out of scope are listed in EXEMPT with a reason, so "not covered"
always means either a decision or a bug — never an oversight.

Usage:
    python scripts/telemetry_coverage.py           # report
    python scripts/telemetry_coverage.py --strict  # non-zero exit if uncovered
"""

import argparse
import re
import sys
from pathlib import Path

for _stream in (sys.stdout, sys.stderr):
    if hasattr(_stream, "reconfigure"):
        _stream.reconfigure(encoding="utf-8", errors="replace")

SERVICES_DIR = Path(__file__).parent.parent / "qml" / "src" / "services"

# A signal counts as a failure signal when its name says so. Matching on the
# name rather than the payload keeps this honest: a signal called
# `somethingFailed` that reports nothing is a gap regardless of its arguments.
FAILURE_NAME = re.compile(r"(Error|Failed|Failure)$")

# How far after a connect() the reportFailure call may appear and still count
# as belonging to it. Connect handlers in this codebase are short lambdas; 40
# lines is generous enough for the longest one and short enough that an
# unrelated call further down the file is not miscounted as coverage.
HANDLER_WINDOW = 40

# Calls that hand an event to telemetry. GameService wraps reportFailure in its
# own reportOperationFailure to attach the game name first, so matching only
# the bare name would report every install and uninstall path as uncovered —
# a false alarm that trains people to ignore this check.
TELEMETRY_CALLS = ("reportFailure", "reportOperationFailure")

# Signals intentionally outside telemetry, with the reason. Anything not listed
# here and not wired is reported as a gap.
EXEMPT = {
    "ocrError": "OCR is an optional plugin; failures are a plugin concern, "
                "not a launcher defect",
    "pluginError": "plugin load failures are third-party code; reporting them "
                   "would attribute other people's bugs to us",
    "detailsFetchError": "Steam store metadata is cosmetic — a failure degrades "
                         "the card, it does not block the user",
    "steamDetailsFetchError": "GameService only forwards detailsFetchError; "
                              "same cosmetic failure, exempt for the same reason",
    "batchError": "all three emit sites are batch UI state (already running, "
                  "empty selection) — the install failures behind a batch are "
                  "reported through CoreBridge::packageInstallError",
    "installError": "reported at the emit sites instead: two are non-HTTPS "
                    "catalog links (reported directly in installflowservice.cpp), "
                    "the third forwards downloadError, which "
                    "TranslationDownloader already reports",
}


def parse_signals(header: Path) -> list[str]:
    """Collect failure-signal names declared in a header's signals: block."""
    text = header.read_text(encoding="utf-8", errors="replace")
    signals: list[str] = []
    in_signals = False

    for raw in text.splitlines():
        line = raw.strip()

        if re.match(r"^(signals|Q_SIGNALS)\s*:", line):
            in_signals = True
            continue
        # Any other access specifier ends the block.
        if re.match(r"^(public|private|protected|public slots|private slots)\s*:", line):
            in_signals = False
            continue
        if not in_signals:
            continue

        match = re.match(r"^void\s+([A-Za-z_]\w*)\s*\(", line)
        if match and FAILURE_NAME.search(match.group(1)):
            signals.append(match.group(1))

    return signals


def find_coverage(sources: dict[Path, list[str]], signal: str) -> list[str]:
    """Return "file:line" sites where `signal` is connected to a telemetry call."""
    sites: list[str] = []
    pattern = re.compile(rf"::{re.escape(signal)}\b")

    for path, lines in sources.items():
        for index, line in enumerate(lines):
            if not pattern.search(line):
                continue
            # Only a connect() binds a signal to a handler; a plain mention
            # (a doc comment, an emit) is not coverage.
            window_start = max(0, index - 3)
            if "connect" not in "\n".join(lines[window_start:index + 3]):
                continue
            handler = "\n".join(lines[index:index + HANDLER_WINDOW])
            if any(call in handler for call in TELEMETRY_CALLS):
                sites.append(f"{path.name}:{index + 1}")

    return sites


def main() -> None:
    parser = argparse.ArgumentParser(description="Telemetry coverage of failure signals")
    parser.add_argument(
        "--strict",
        action="store_true",
        help="Exit non-zero when an unexempted signal has no telemetry",
    )
    args = parser.parse_args()

    if not SERVICES_DIR.is_dir():
        print(f"ERROR: services directory not found: {SERVICES_DIR}", file=sys.stderr)
        sys.exit(1)

    sources = {
        path: path.read_text(encoding="utf-8", errors="replace").splitlines()
        for path in sorted(SERVICES_DIR.glob("*.cpp"))
    }

    covered: list[tuple[str, str, list[str]]] = []
    gaps: list[tuple[str, str]] = []
    exempt_hits: list[tuple[str, str, str]] = []

    for header in sorted(SERVICES_DIR.glob("*.h")):
        for signal in parse_signals(header):
            sites = find_coverage(sources, signal)
            if sites:
                covered.append((header.name, signal, sites))
            elif signal in EXEMPT:
                exempt_hits.append((header.name, signal, EXEMPT[signal]))
            else:
                gaps.append((header.name, signal))

    line = "=" * 78
    print(line)
    print("  TELEMETRI KAPSAMI — hata sinyalleri")
    print(line)

    print(f"\n  Kapsanan ({len(covered)}):")
    for header, signal, sites in covered:
        print(f"    + {signal:26s} {header:28s} -> {', '.join(sites)}")

    if exempt_hits:
        print(f"\n  Bilincli kapsam disi ({len(exempt_hits)}):")
        for header, signal, reason in exempt_hits:
            print(f"    - {signal:26s} {header}")
            print(f"        {reason}")

    if gaps:
        print(f"\n  !! KAPSAM DISI ({len(gaps)}) — hata olusuyor, kimse gormuyor:")
        for header, signal in gaps:
            print(f"    ! {signal:26s} {header}")
        print("\n     Ya servisin kendi hata sinyaline CrashReporter::reportFailure")
        print("     bagla, ya da gerekcesiyle EXEMPT listesine ekle.")
    else:
        print("\n  Kapsam disi sinyal yok.")

    print()
    print(line)

    if args.strict and gaps:
        sys.exit(1)


if __name__ == "__main__":
    main()

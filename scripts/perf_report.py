#!/usr/bin/env python3
"""
perf_report.py - Format Makine-Launcher performance report JSON into markdown.

Reads: %LOCALAPPDATA%/Makine/logs/perf_report.json
Output: Formatted markdown to stdout (Claude Code can read this directly)

Usage:
    python scripts/perf_report.py                  # Latest report
    python scripts/perf_report.py path/to/file.json # Specific file
"""

import json
import os
import sys


def find_report(path=None):
    """Find the perf report JSON file."""
    if path and os.path.exists(path):
        return path

    # Qt AppLocalDataLocation = %LOCALAPPDATA%/<OrgName>/<AppName>
    default = os.path.join(
        os.environ.get("LOCALAPPDATA", ""),
        "MakineCeviri", "Makine-Launcher", "logs", "perf_report.json"
    )
    if os.path.exists(default):
        return default

    return None


def format_ms(ms):
    """Format milliseconds with appropriate precision."""
    if ms >= 1000:
        return f"{ms / 1000:.2f}s"
    if ms >= 1:
        return f"{ms:.1f}ms"
    return f"{ms * 1000:.0f}us"


def format_report(data):
    """Format JSON data into markdown report."""
    lines = []
    lines.append("# Makine-Launcher Performance Report")
    lines.append("")
    lines.append(f"**Timestamp:** {data.get('timestamp', 'N/A')}")
    lines.append(f"**Uptime:** {format_ms(data.get('uptime_ms', 0))}")
    lines.append(f"**Zones:** {data.get('zone_count', 0)}")
    lines.append(f"**Total entries:** {data.get('total_entries', 0):,}")
    lines.append("")

    # Warnings
    warnings = data.get("warnings", [])
    if warnings:
        lines.append("## Warnings (main thread > 16ms frame budget)")
        lines.append("")
        for w in warnings:
            lines.append(f"- {w}")
        lines.append("")

    # Zone table
    zones = data.get("zones", [])
    if zones:
        lines.append("## Zone Statistics (sorted by total time)")
        lines.append("")
        lines.append("| Zone | Count | Total | Mean | Min | Max | Main |")
        lines.append("|------|------:|------:|-----:|----:|----:|:----:|")

        for z in zones:
            name = z.get("name", "?")
            count = z.get("count", 0)
            total = format_ms(z.get("total_ms", 0))
            mean = format_ms(z.get("mean_ms", 0))
            min_v = format_ms(z.get("min_ms", 0))
            max_v = format_ms(z.get("max_ms", 0))
            main = "!!!" if z.get("main_thread") and z.get("max_ms", 0) > 16 else \
                   "Y" if z.get("main_thread") else ""
            lines.append(f"| {name} | {count} | {total} | {mean} | {min_v} | {max_v} | {main} |")

        lines.append("")

    # Thread table
    threads = data.get("threads", {})
    if threads:
        lines.append("## Thread Statistics")
        lines.append("")
        lines.append("| Thread | Entries | Total Time |")
        lines.append("|--------|--------:|-----------:|")

        for name, stats in sorted(threads.items(), key=lambda x: x[1].get("total_ms", 0), reverse=True):
            entries = stats.get("entries", 0)
            total = format_ms(stats.get("total_ms", 0))
            lines.append(f"| {name} | {entries:,} | {total} |")

        lines.append("")

    # Summary
    if zones:
        main_zones = [z for z in zones if z.get("main_thread")]
        main_total = sum(z.get("total_ms", 0) for z in main_zones)
        main_max = max((z.get("max_ms", 0) for z in main_zones), default=0)
        jank_count = sum(1 for z in main_zones if z.get("max_ms", 0) > 16)

        lines.append("## Summary")
        lines.append("")
        lines.append(f"- **Main thread total:** {format_ms(main_total)}")
        lines.append(f"- **Main thread worst single call:** {format_ms(main_max)}")
        lines.append(f"- **Jank zones (>16ms on main):** {jank_count}")
        lines.append("")

    return "\n".join(lines)


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else None
    report_file = find_report(path)

    if not report_file:
        print("ERROR: No performance report found.")
        print(f"Expected at: %LOCALAPPDATA%/MakineCeviri/Makine-Launcher/logs/perf_report.json")
        print("Run 'just profile-auto' to generate one.")
        sys.exit(1)

    with open(report_file, "r", encoding="utf-8") as f:
        data = json.load(f)

    print(format_report(data))


if __name__ == "__main__":
    main()

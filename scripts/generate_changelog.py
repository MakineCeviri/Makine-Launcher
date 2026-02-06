#!/usr/bin/env python3
"""
Changelog Generator for MakineAI
Copyright (c) 2026 MakineAI Team

Generates CHANGELOG.md from git commit history using Conventional Commits.

Commit types:
- feat: New feature
- fix: Bug fix
- docs: Documentation
- style: Code style (formatting, etc.)
- refactor: Refactoring
- perf: Performance improvement
- test: Adding tests
- chore: Maintenance tasks
- build: Build system changes
- ci: CI/CD changes

Usage:
    python scripts/generate_changelog.py [--from TAG] [--to TAG] [--output FILE]

Examples:
    python scripts/generate_changelog.py
    python scripts/generate_changelog.py --from v0.0.8 --to HEAD
    python scripts/generate_changelog.py --output CHANGELOG.md
"""

import argparse
import subprocess
import re
from datetime import datetime
from collections import defaultdict
from pathlib import Path
from typing import Dict, List, Optional, NamedTuple

# =============================================================================
# CONFIGURATION
# =============================================================================

COMMIT_TYPES = {
    'feat': ('Features', '✨'),
    'fix': ('Bug Fixes', '🐛'),
    'docs': ('Documentation', '📚'),
    'style': ('Code Style', '💄'),
    'refactor': ('Refactoring', '♻️'),
    'perf': ('Performance', '⚡'),
    'test': ('Tests', '✅'),
    'chore': ('Maintenance', '🔧'),
    'build': ('Build System', '📦'),
    'ci': ('CI/CD', '👷'),
    'revert': ('Reverts', '⏪'),
    'security': ('Security', '🔒'),
}

BREAKING_CHANGE_PATTERN = re.compile(r'BREAKING CHANGE[S]?:', re.IGNORECASE)

# =============================================================================
# DATA STRUCTURES
# =============================================================================

class Commit(NamedTuple):
    hash: str
    short_hash: str
    type: str
    scope: Optional[str]
    description: str
    body: str
    breaking: bool
    date: str
    author: str

# =============================================================================
# GIT OPERATIONS
# =============================================================================

def run_git(args: List[str]) -> str:
    """Run a git command and return output"""
    try:
        result = subprocess.run(
            ['git'] + args,
            capture_output=True,
            text=True,
            check=True
        )
        return result.stdout.strip()
    except subprocess.CalledProcessError as e:
        print(f"Git error: {e.stderr}")
        return ""

def get_tags() -> List[str]:
    """Get all version tags sorted by date"""
    output = run_git(['tag', '--sort=-creatordate', '-l', 'v*'])
    return output.split('\n') if output else []

def get_commits(from_ref: Optional[str], to_ref: str) -> List[str]:
    """Get commit hashes between two refs"""
    if from_ref:
        range_spec = f"{from_ref}..{to_ref}"
    else:
        range_spec = to_ref

    output = run_git(['log', range_spec, '--format=%H', '--reverse'])
    return output.split('\n') if output else []

def parse_commit(commit_hash: str) -> Optional[Commit]:
    """Parse a commit into structured data"""
    # Get commit info
    format_str = '%H%n%h%n%s%n%b%n%ad%n%an'
    output = run_git(['show', '-s', f'--format={format_str}', '--date=short', commit_hash])

    if not output:
        return None

    lines = output.split('\n')
    if len(lines) < 5:
        return None

    full_hash = lines[0]
    short_hash = lines[1]
    subject = lines[2]
    body = '\n'.join(lines[3:-2]).strip()
    date = lines[-2]
    author = lines[-1]

    # Parse conventional commit format: type(scope): description
    pattern = re.compile(r'^(\w+)(?:\(([^)]+)\))?!?:\s*(.+)$')
    match = pattern.match(subject)

    if not match:
        # Not a conventional commit, skip or use as-is
        return Commit(
            hash=full_hash,
            short_hash=short_hash,
            type='other',
            scope=None,
            description=subject,
            body=body,
            breaking=False,
            date=date,
            author=author
        )

    commit_type = match.group(1).lower()
    scope = match.group(2)
    description = match.group(3)

    # Check for breaking changes
    breaking = '!' in subject or BREAKING_CHANGE_PATTERN.search(body) is not None

    return Commit(
        hash=full_hash,
        short_hash=short_hash,
        type=commit_type,
        scope=scope,
        description=description,
        body=body,
        breaking=breaking,
        date=date,
        author=author
    )

# =============================================================================
# CHANGELOG GENERATION
# =============================================================================

def group_commits(commits: List[Commit]) -> Dict[str, List[Commit]]:
    """Group commits by type"""
    grouped = defaultdict(list)
    for commit in commits:
        grouped[commit.type].append(commit)
    return dict(grouped)

def format_commit(commit: Commit, include_hash: bool = True) -> str:
    """Format a single commit for changelog"""
    parts = []

    # Scope prefix
    if commit.scope:
        parts.append(f"**{commit.scope}:** ")

    # Description
    parts.append(commit.description)

    # Hash link
    if include_hash:
        parts.append(f" ([{commit.short_hash}](../../commit/{commit.hash}))")

    # Breaking change indicator
    if commit.breaking:
        parts.append(" 💥 **BREAKING**")

    return ''.join(parts)

def generate_version_changelog(
    version: str,
    date: str,
    commits: List[Commit],
    compare_url: Optional[str] = None
) -> str:
    """Generate changelog for a single version"""
    lines = []

    # Version header
    if compare_url:
        lines.append(f"## [{version}]({compare_url}) ({date})")
    else:
        lines.append(f"## {version} ({date})")
    lines.append("")

    # Group commits
    grouped = group_commits(commits)

    # Breaking changes section (always first)
    breaking = [c for c in commits if c.breaking]
    if breaking:
        lines.append("### 💥 BREAKING CHANGES")
        lines.append("")
        for commit in breaking:
            lines.append(f"- {format_commit(commit)}")
            if commit.body:
                # Include body for breaking changes
                for body_line in commit.body.split('\n')[:3]:
                    if body_line.strip():
                        lines.append(f"  {body_line}")
        lines.append("")

    # Other sections by type
    for commit_type, (section_name, emoji) in COMMIT_TYPES.items():
        type_commits = grouped.get(commit_type, [])
        type_commits = [c for c in type_commits if not c.breaking]  # Already shown

        if type_commits:
            lines.append(f"### {emoji} {section_name}")
            lines.append("")
            for commit in type_commits:
                lines.append(f"- {format_commit(commit)}")
            lines.append("")

    # Other commits (non-conventional)
    other = grouped.get('other', [])
    if other:
        lines.append("### Other Changes")
        lines.append("")
        for commit in other:
            lines.append(f"- {commit.description} ([{commit.short_hash}](../../commit/{commit.hash}))")
        lines.append("")

    return '\n'.join(lines)

def generate_full_changelog(from_ref: Optional[str], to_ref: str) -> str:
    """Generate full changelog"""
    lines = []

    # Header
    lines.append("# Changelog")
    lines.append("")
    lines.append("All notable changes to MakineAI will be documented in this file.")
    lines.append("")
    lines.append("The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),")
    lines.append("and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).")
    lines.append("")

    # Get commits
    commit_hashes = get_commits(from_ref, to_ref)
    commits = []
    for hash in commit_hashes:
        commit = parse_commit(hash)
        if commit:
            commits.append(commit)

    if not commits:
        lines.append("No commits found.")
        return '\n'.join(lines)

    # Determine version
    if to_ref == 'HEAD':
        version = '[Unreleased]'
        date = datetime.now().strftime('%Y-%m-%d')
    else:
        version = to_ref
        date = commits[-1].date if commits else datetime.now().strftime('%Y-%m-%d')

    # Generate version section
    compare_url = None
    if from_ref:
        compare_url = f"../../compare/{from_ref}...{to_ref}"

    lines.append(generate_version_changelog(version, date, commits, compare_url))

    return '\n'.join(lines)

# =============================================================================
# MAIN
# =============================================================================

def main():
    parser = argparse.ArgumentParser(description='Generate changelog from git commits')
    parser.add_argument('--from', dest='from_ref', help='Starting reference (tag/commit)')
    parser.add_argument('--to', default='HEAD', help='Ending reference (default: HEAD)')
    parser.add_argument('--output', '-o', type=Path, help='Output file (default: stdout)')
    parser.add_argument('--append', '-a', action='store_true', help='Append to existing file')
    args = parser.parse_args()

    # Generate changelog
    changelog = generate_full_changelog(args.from_ref, args.to)

    # Output
    if args.output:
        mode = 'a' if args.append else 'w'
        args.output.write_text(changelog, encoding='utf-8')
        print(f"Changelog written to: {args.output}")
    else:
        print(changelog)

if __name__ == '__main__':
    main()

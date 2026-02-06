#!/usr/bin/env python3
"""
Code Quality Audit Script for MakineAI
Copyright (c) 2026 MakineAI Team

Performs static analysis checks:
- const-correctness: Identifies functions that should be const
- noexcept audit: Identifies functions that should be noexcept
- Include analysis: Identifies potentially unnecessary includes
- Code complexity: Reports complex functions

Usage:
    python scripts/code_quality_audit.py [--verbose] [--fix]
"""

import argparse
import os
import re
import sys
from pathlib import Path
from dataclasses import dataclass, field
from typing import List, Set, Dict, Optional
from collections import defaultdict

# =============================================================================
# DATA CLASSES
# =============================================================================

@dataclass
class Issue:
    """Represents a code quality issue"""
    file: Path
    line: int
    category: str
    severity: str  # error, warning, info
    message: str
    suggestion: Optional[str] = None

@dataclass
class AuditResult:
    """Results of the audit"""
    issues: List[Issue] = field(default_factory=list)
    files_scanned: int = 0
    lines_scanned: int = 0

    def add(self, issue: Issue):
        self.issues.append(issue)

    def by_category(self) -> Dict[str, List[Issue]]:
        result = defaultdict(list)
        for issue in self.issues:
            result[issue.category].append(issue)
        return dict(result)

    def by_severity(self) -> Dict[str, int]:
        result = defaultdict(int)
        for issue in self.issues:
            result[issue.severity] += 1
        return dict(result)

# =============================================================================
# PATTERNS
# =============================================================================

# Functions that should typically be const
CONST_CANDIDATES_PATTERN = re.compile(
    r'^\s*(?:\[\[nodiscard\]\]\s*)?'
    r'(?:(?:virtual|static|inline|constexpr)\s+)*'
    r'(\w+(?:::\w+)*(?:<[^>]+>)?)\s+'  # Return type
    r'(\w+)\s*\('  # Function name
    r'([^)]*)\)'  # Parameters
    r'\s*(?:override\s*)?'
    r'(?!const)'  # NOT followed by const
    r'(?:\{|;)',  # Body or declaration end
    re.MULTILINE
)

# Getter patterns - should almost always be const
GETTER_PATTERN = re.compile(
    r'^\s*(?:\[\[nodiscard\]\]\s*)?'
    r'(?:const\s+)?'
    r'(\w+(?:::\w+)*(?:<[^>]+>)?[&*]?)\s+'
    r'(get\w*|is\w*|has\w*|can\w*|should\w*|name|type|size|count|length|value|data|begin|end)\s*\('
    r'[^)]*\)'
    r'\s*(?!const)',
    re.MULTILINE | re.IGNORECASE
)

# Functions that should typically be noexcept
NOEXCEPT_CANDIDATES = [
    'destructor',
    'move_constructor',
    'move_assignment',
    'swap',
    'size',
    'empty',
    'data',
    'begin',
    'end',
    'cbegin',
    'cend',
]

NOEXCEPT_PATTERN = re.compile(
    r'^\s*(?:\[\[nodiscard\]\]\s*)?'
    r'(?:(?:virtual|static|inline|constexpr)\s+)*'
    r'(?:\w+(?:::\w+)*(?:<[^>]+>)?[&*]?\s+)?'  # Optional return type
    r'(~?\w+)\s*\('  # Function name (including destructor)
    r'([^)]*)\)'  # Parameters
    r'\s*(?:const\s*)?'
    r'(?!noexcept)'  # NOT followed by noexcept
    r'(?:\{|;|=)',
    re.MULTILINE
)

# Include patterns
INCLUDE_PATTERN = re.compile(r'^\s*#include\s*[<"]([^>"]+)[>"]', re.MULTILINE)

# Potentially heavy includes
HEAVY_INCLUDES = {
    '<iostream>': 'Consider <iosfwd> for declarations',
    '<fstream>': 'Consider forward declaration if only using references',
    '<sstream>': 'Consider <string> if only using std::string',
    '<regex>': 'Heavy header - ensure it\'s needed',
    '<algorithm>': 'Consider specific headers like <ranges>',
    '<nlohmann/json.hpp>': 'Consider <makineai/fwd.hpp> + forward declaration',
    '<boost/filesystem.hpp>': 'Consider <filesystem> from C++17',
}

# =============================================================================
# AUDITORS
# =============================================================================

def audit_const_correctness(content: str, file_path: Path, result: AuditResult):
    """Check for functions that should be const"""
    lines = content.split('\n')

    # Track class context
    in_class = False
    class_depth = 0

    for i, line in enumerate(lines, 1):
        # Track class boundaries (simplified)
        if re.match(r'^\s*class\s+\w+', line):
            in_class = True
            class_depth = 0

        if in_class:
            class_depth += line.count('{') - line.count('}')
            if class_depth <= 0:
                in_class = False

        if not in_class:
            continue

        # Check getter patterns
        match = GETTER_PATTERN.search(line)
        if match:
            func_name = match.group(2)
            # Skip if it's a definition in header (implementation might modify)
            if '{' in line and '}' in line:
                # Single-line implementation
                if 'this->' not in line and 'member_' not in line:
                    continue

            result.add(Issue(
                file=file_path,
                line=i,
                category='const-correctness',
                severity='warning',
                message=f"Getter function '{func_name}' may need 'const' qualifier",
                suggestion=f"Add 'const' after the parameter list: {func_name}(...) const"
            ))

def audit_noexcept(content: str, file_path: Path, result: AuditResult):
    """Check for functions that should be noexcept"""
    lines = content.split('\n')

    for i, line in enumerate(lines, 1):
        # Check destructors
        if '~' in line and 'noexcept' not in line:
            match = re.search(r'~(\w+)\s*\(\s*\)', line)
            if match:
                result.add(Issue(
                    file=file_path,
                    line=i,
                    category='noexcept',
                    severity='warning',
                    message=f"Destructor '~{match.group(1)}' should be noexcept",
                    suggestion="Add 'noexcept' after the parameter list"
                ))

        # Check move operations
        if 'operator=' in line and '&&' in line and 'noexcept' not in line:
            result.add(Issue(
                file=file_path,
                line=i,
                category='noexcept',
                severity='warning',
                message="Move assignment operator should be noexcept",
                suggestion="Add 'noexcept' for move semantics optimization"
            ))

        # Check move constructors
        if re.search(r'\w+\s*\(\s*\w+\s*&&', line) and 'noexcept' not in line:
            if 'operator' not in line:  # Not an operator
                result.add(Issue(
                    file=file_path,
                    line=i,
                    category='noexcept',
                    severity='info',
                    message="Move constructor may benefit from noexcept",
                    suggestion="Consider adding 'noexcept' for move semantics"
                ))

        # Check swap functions
        if 'void swap' in line.lower() and 'noexcept' not in line:
            result.add(Issue(
                file=file_path,
                line=i,
                category='noexcept',
                severity='warning',
                message="swap() function should be noexcept",
                suggestion="Add 'noexcept' - swap should never throw"
            ))

def audit_includes(content: str, file_path: Path, result: AuditResult):
    """Check for potentially unnecessary or heavy includes"""
    includes = INCLUDE_PATTERN.findall(content)

    for i, line in enumerate(content.split('\n'), 1):
        match = INCLUDE_PATTERN.match(line)
        if match:
            include = match.group(1)

            # Check for heavy includes
            for heavy, suggestion in HEAVY_INCLUDES.items():
                if heavy.strip('<>"') in include:
                    result.add(Issue(
                        file=file_path,
                        line=i,
                        category='include-optimization',
                        severity='info',
                        message=f"Heavy include: {include}",
                        suggestion=suggestion
                    ))
                    break

    # Check for unused includes (simplified - just count symbol usage)
    for include in includes:
        # Extract likely symbol from include
        symbol = Path(include).stem
        if symbol and symbol not in content.replace(f'#include', ''):
            # Very simplified check - might have false positives
            pass  # Skip for now, needs proper analysis

def audit_complexity(content: str, file_path: Path, result: AuditResult):
    """Check for overly complex functions"""
    # Simple cyclomatic complexity approximation
    # Count: if, else, for, while, case, &&, ||, ?

    func_pattern = re.compile(
        r'(\w+)\s*\([^)]*\)\s*(?:const\s*)?(?:noexcept\s*)?(?:override\s*)?\{',
        re.MULTILINE
    )

    lines = content.split('\n')
    in_function = False
    func_name = ""
    func_start = 0
    brace_depth = 0
    complexity = 1

    for i, line in enumerate(lines, 1):
        if not in_function:
            match = func_pattern.search(line)
            if match:
                in_function = True
                func_name = match.group(1)
                func_start = i
                brace_depth = 0
                complexity = 1

        if in_function:
            brace_depth += line.count('{') - line.count('}')

            # Count complexity indicators
            complexity += line.count(' if ') + line.count(' if(')
            complexity += line.count(' else ') + line.count('}else')
            complexity += line.count(' for ') + line.count(' for(')
            complexity += line.count(' while ') + line.count(' while(')
            complexity += line.count(' case ')
            complexity += line.count(' && ')
            complexity += line.count(' || ')
            complexity += line.count(' ? ')

            if brace_depth <= 0:
                in_function = False
                if complexity > 15:
                    result.add(Issue(
                        file=file_path,
                        line=func_start,
                        category='complexity',
                        severity='warning' if complexity > 20 else 'info',
                        message=f"Function '{func_name}' has high complexity ({complexity})",
                        suggestion="Consider breaking into smaller functions"
                    ))

# =============================================================================
# MAIN
# =============================================================================

def scan_file(file_path: Path, result: AuditResult, verbose: bool = False):
    """Scan a single file for issues"""
    try:
        content = file_path.read_text(encoding='utf-8')
    except Exception as e:
        if verbose:
            print(f"  Warning: Could not read {file_path}: {e}")
        return

    result.files_scanned += 1
    result.lines_scanned += content.count('\n')

    audit_const_correctness(content, file_path, result)
    audit_noexcept(content, file_path, result)
    audit_includes(content, file_path, result)
    audit_complexity(content, file_path, result)

def scan_directory(root: Path, result: AuditResult, verbose: bool = False):
    """Recursively scan directory for C++ files"""
    for path in root.rglob('*'):
        if path.suffix in ['.hpp', '.h', '.cpp', '.cc']:
            if 'build' in path.parts or 'vcpkg' in path.parts:
                continue
            if verbose:
                print(f"Scanning: {path}")
            scan_file(path, result, verbose)

def print_report(result: AuditResult):
    """Print audit report"""
    print("\n" + "=" * 60)
    print("CODE QUALITY AUDIT REPORT")
    print("=" * 60)

    print(f"\nFiles scanned: {result.files_scanned}")
    print(f"Lines scanned: {result.lines_scanned:,}")
    print(f"Total issues:  {len(result.issues)}")

    severity_counts = result.by_severity()
    print(f"\nBy severity:")
    for sev, count in sorted(severity_counts.items()):
        print(f"  {sev}: {count}")

    by_category = result.by_category()
    print(f"\nBy category:")
    for cat, issues in sorted(by_category.items()):
        print(f"\n  [{cat.upper()}] ({len(issues)} issues)")
        for issue in issues[:5]:  # Show first 5
            print(f"    {issue.file.name}:{issue.line} - {issue.message}")
        if len(issues) > 5:
            print(f"    ... and {len(issues) - 5} more")

    print("\n" + "=" * 60)

    # Return exit code based on errors
    return 1 if severity_counts.get('error', 0) > 0 else 0

def main():
    parser = argparse.ArgumentParser(description='MakineAI Code Quality Audit')
    parser.add_argument('--verbose', '-v', action='store_true', help='Verbose output')
    parser.add_argument('--path', '-p', type=Path, default=Path('.'), help='Path to scan')
    parser.add_argument('--json', '-j', action='store_true', help='Output as JSON')
    args = parser.parse_args()

    # Find project root
    root = args.path
    if not (root / 'core').exists():
        # Try parent directories
        for parent in root.parents:
            if (parent / 'core').exists():
                root = parent
                break

    include_dir = root / 'core' / 'include'
    src_dir = root / 'core' / 'src'

    if not include_dir.exists():
        print(f"Error: Could not find core/include directory in {root}")
        return 1

    print("MakineAI Code Quality Audit")
    print("-" * 40)

    result = AuditResult()

    if include_dir.exists():
        print(f"Scanning: {include_dir}")
        scan_directory(include_dir, result, args.verbose)

    if src_dir.exists():
        print(f"Scanning: {src_dir}")
        scan_directory(src_dir, result, args.verbose)

    return print_report(result)

if __name__ == '__main__':
    sys.exit(main())

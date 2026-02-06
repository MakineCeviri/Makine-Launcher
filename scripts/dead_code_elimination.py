#!/usr/bin/env python3
"""
Dead Code Elimination Analysis Tool for MakineAI

Detects potentially unused code in the codebase:
- Unused functions and methods
- Unused classes and structs
- Unused variables and constants
- Unused includes
- Unused macros

Usage:
    python scripts/dead_code_elimination.py [--path core/] [--format text|json]
    python scripts/dead_code_elimination.py --fix  # Attempt to fix (dry-run first!)

Requirements:
    - Python 3.9+
    - No external dependencies for basic analysis

Note: This is a heuristic tool. Always verify findings before deletion.
"""

import argparse
import json
import os
import re
from collections import defaultdict
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Set, Tuple


@dataclass
class Symbol:
    """Represents a code symbol (function, class, variable, etc.)"""
    name: str
    kind: str  # function, class, struct, variable, constant, macro
    file: str
    line: int
    scope: str = ""  # namespace::class::
    references: int = 0
    is_public: bool = True
    is_virtual: bool = False
    is_override: bool = False
    is_test: bool = False


@dataclass
class UnusedInclude:
    """Represents a potentially unused include"""
    file: str
    line: int
    include: str
    reason: str


@dataclass
class AnalysisResult:
    """Complete analysis result"""
    unused_functions: List[Symbol] = field(default_factory=list)
    unused_classes: List[Symbol] = field(default_factory=list)
    unused_variables: List[Symbol] = field(default_factory=list)
    unused_constants: List[Symbol] = field(default_factory=list)
    unused_macros: List[Symbol] = field(default_factory=list)
    unused_includes: List[UnusedInclude] = field(default_factory=list)
    statistics: Dict[str, int] = field(default_factory=dict)

    def to_dict(self) -> dict:
        return {
            "unused_functions": [vars(s) for s in self.unused_functions],
            "unused_classes": [vars(s) for s in self.unused_classes],
            "unused_variables": [vars(s) for s in self.unused_variables],
            "unused_constants": [vars(s) for s in self.unused_constants],
            "unused_macros": [vars(s) for s in self.unused_macros],
            "unused_includes": [vars(i) for i in self.unused_includes],
            "statistics": self.statistics,
        }


class DeadCodeAnalyzer:
    """Analyzes C++ code for potentially unused symbols"""

    # File extensions to analyze
    CPP_EXTENSIONS = {".cpp", ".hpp", ".h", ".cc", ".cxx", ".hxx"}

    # Patterns that indicate a symbol might be used externally
    EXTERNAL_PATTERNS = {
        "public",
        "Q_INVOKABLE",
        "Q_PROPERTY",
        "Q_SIGNAL",
        "Q_SLOT",
        "extern",
        "EXPORT",
        "DLL",
        "API",
    }

    # Test-related patterns
    TEST_PATTERNS = {
        "TEST",
        "test_",
        "_test",
        "Test",
        "BENCHMARK",
        "FUZZ",
        "Mock",
        "Fake",
    }

    def __init__(self, root_path: Path, verbose: bool = False):
        self.root_path = root_path
        self.verbose = verbose
        self.symbols: Dict[str, Symbol] = {}
        self.references: Dict[str, int] = defaultdict(int)
        self.file_contents: Dict[str, str] = {}
        self.includes_per_file: Dict[str, List[Tuple[int, str]]] = defaultdict(list)

    def log(self, msg: str):
        if self.verbose:
            print(f"[ANALYZE] {msg}")

    def find_cpp_files(self) -> List[Path]:
        """Find all C++ source and header files"""
        files = []
        for ext in self.CPP_EXTENSIONS:
            files.extend(self.root_path.rglob(f"*{ext}"))

        # Filter out build directories and generated files
        filtered = []
        for f in files:
            path_str = str(f).lower()
            if any(skip in path_str for skip in ["build", "cmake-build", ".vs", "__pycache__", "generated"]):
                continue
            filtered.append(f)

        return filtered

    def read_file(self, filepath: Path) -> str:
        """Read and cache file content"""
        key = str(filepath)
        if key not in self.file_contents:
            try:
                self.file_contents[key] = filepath.read_text(encoding="utf-8", errors="replace")
            except Exception as e:
                self.log(f"Error reading {filepath}: {e}")
                self.file_contents[key] = ""
        return self.file_contents[key]

    def extract_symbols(self, filepath: Path):
        """Extract symbol definitions from a file"""
        content = self.read_file(filepath)
        if not content:
            return

        file_str = str(filepath.relative_to(self.root_path))
        lines = content.split("\n")

        # Track current scope
        current_namespace = ""
        current_class = ""
        brace_depth = 0
        scope_stack = []

        # Regex patterns for symbol detection
        patterns = {
            "namespace": re.compile(r"^\s*namespace\s+(\w+)\s*\{?"),
            "class": re.compile(r"^\s*(?:class|struct)\s+(?:MAKINEAI_EXPORT\s+)?(\w+)(?:\s*:\s*.*)?(?:\s*\{)?"),
            "function": re.compile(r"^\s*(?:(?:inline|static|virtual|explicit|constexpr|nodiscard)\s+)*(?:\w+(?:::\w+)*(?:<[^>]*>)?\s+)+(\w+)\s*\([^)]*\)(?:\s*(?:const|noexcept|override|final))*(?:\s*\{|\s*;|\s*=)"),
            "variable": re.compile(r"^\s*(?:static\s+)?(?:const\s+)?(?:inline\s+)?(?:\w+(?:::\w+)*(?:<[^>]*>)?\s+)+(\w+)\s*(?:=|;|\{)"),
            "constant": re.compile(r"^\s*(?:static\s+)?(?:constexpr|const)\s+(?:\w+(?:::\w+)*(?:<[^>]*>)?\s+)+(\w+)\s*(?:=|;|\{)"),
            "macro": re.compile(r"^\s*#define\s+(\w+)(?:\(|\s|$)"),
            "using": re.compile(r"^\s*using\s+(\w+)\s*="),
            "typedef": re.compile(r"^\s*typedef\s+.*?\s+(\w+)\s*;"),
        }

        for line_num, line in enumerate(lines, 1):
            # Track braces for scope
            brace_depth += line.count("{") - line.count("}")

            # Update scope tracking
            while scope_stack and scope_stack[-1][1] >= brace_depth:
                scope_stack.pop()

            # Check for namespace
            match = patterns["namespace"].match(line)
            if match:
                ns_name = match.group(1)
                scope_stack.append((f"{ns_name}::", brace_depth))
                continue

            # Check for class/struct
            match = patterns["class"].match(line)
            if match:
                class_name = match.group(1)
                scope = "".join(s[0] for s in scope_stack)
                scope_stack.append((f"{class_name}::", brace_depth))

                # Skip forward declarations
                if "{" in line or any(c in line for c in [":", "final"]):
                    symbol = Symbol(
                        name=class_name,
                        kind="class" if "class" in line else "struct",
                        file=file_str,
                        line=line_num,
                        scope=scope,
                        is_public="public" in line or "struct" in line,
                        is_test=any(p in class_name for p in self.TEST_PATTERNS),
                    )
                    key = f"{scope}{class_name}"
                    self.symbols[key] = symbol
                continue

            # Check for functions
            if "(" in line and ")" in line and not line.strip().startswith("//"):
                # Try to extract function name
                func_match = re.search(r"\b(\w+)\s*\([^)]*\)", line)
                if func_match and not any(kw in line for kw in ["if", "while", "for", "switch", "catch", "sizeof", "decltype"]):
                    func_name = func_match.group(1)
                    if func_name and func_name[0].isupper() or func_name[0].islower():
                        scope = "".join(s[0] for s in scope_stack)
                        is_definition = "{" in line or (line_num < len(lines) and "{" in lines[line_num])

                        if is_definition or ";" in line:  # Declaration or definition
                            symbol = Symbol(
                                name=func_name,
                                kind="function",
                                file=file_str,
                                line=line_num,
                                scope=scope,
                                is_public=any(p in line for p in self.EXTERNAL_PATTERNS),
                                is_virtual="virtual" in line,
                                is_override="override" in line,
                                is_test=any(p in func_name for p in self.TEST_PATTERNS),
                            )
                            key = f"{scope}{func_name}"
                            if key not in self.symbols:  # Don't overwrite
                                self.symbols[key] = symbol

            # Check for macros
            match = patterns["macro"].match(line)
            if match:
                macro_name = match.group(1)
                # Skip include guards and common macros
                if not macro_name.endswith("_HPP") and not macro_name.endswith("_H"):
                    symbol = Symbol(
                        name=macro_name,
                        kind="macro",
                        file=file_str,
                        line=line_num,
                    )
                    self.symbols[macro_name] = symbol

    def extract_includes(self, filepath: Path):
        """Extract includes from a file"""
        content = self.read_file(filepath)
        file_str = str(filepath.relative_to(self.root_path))

        include_pattern = re.compile(r'^\s*#include\s*[<"]([^>"]+)[>"]')

        for line_num, line in enumerate(content.split("\n"), 1):
            match = include_pattern.match(line)
            if match:
                include = match.group(1)
                self.includes_per_file[file_str].append((line_num, include))

    def count_references(self, filepath: Path):
        """Count references to symbols in a file"""
        content = self.read_file(filepath)
        if not content:
            return

        # Create a set of all symbol names for faster lookup
        symbol_names = set()
        for key, sym in self.symbols.items():
            symbol_names.add(sym.name)
            # Also add fully qualified names
            if sym.scope:
                symbol_names.add(key)

        # Remove comments and strings for cleaner analysis
        content = re.sub(r'//.*$', '', content, flags=re.MULTILINE)
        content = re.sub(r'/\*.*?\*/', '', content, flags=re.DOTALL)
        content = re.sub(r'"(?:[^"\\]|\\.)*"', '""', content)
        content = re.sub(r"'(?:[^'\\]|\\.)*'", "''", content)

        # Count word occurrences
        words = re.findall(r'\b(\w+)\b', content)
        word_counts = defaultdict(int)
        for word in words:
            word_counts[word] += 1

        # Update reference counts
        for key, sym in self.symbols.items():
            count = word_counts.get(sym.name, 0)
            # Subtract 1 for the definition itself (approximate)
            self.references[key] = max(0, count - 1)

    def detect_unused_includes(self, filepath: Path):
        """Detect potentially unused includes"""
        file_str = str(filepath.relative_to(self.root_path))
        content = self.read_file(filepath)

        # Remove preprocessor directives except includes
        content_clean = re.sub(r'^\s*#(?!include).*$', '', content, flags=re.MULTILINE)
        content_clean = re.sub(r'//.*$', '', content_clean, flags=re.MULTILINE)
        content_clean = re.sub(r'/\*.*?\*/', '', content_clean, flags=re.DOTALL)

        unused = []
        for line_num, include in self.includes_per_file.get(file_str, []):
            # Extract potential symbols from include path
            include_name = Path(include).stem

            # Check if include name appears in the file
            if include_name not in content_clean:
                # Additional check: look for common patterns
                # e.g., <vector> should have std::vector
                common_usage = {
                    "vector": "vector",
                    "string": "string",
                    "map": "map",
                    "set": "set",
                    "memory": "unique_ptr|shared_ptr|make_unique|make_shared",
                    "optional": "optional|nullopt",
                    "filesystem": "filesystem|fs::",
                    "fstream": "ifstream|ofstream|fstream",
                    "sstream": "stringstream|istringstream|ostringstream",
                    "algorithm": "sort|find|transform|copy|remove|replace|for_each",
                    "chrono": "chrono::",
                    "thread": "thread|jthread",
                    "mutex": "mutex|lock_guard|unique_lock",
                    "future": "future|promise|async",
                    "functional": "function|bind|ref",
                    "utility": "pair|move|forward|swap",
                    "tuple": "tuple|get<|make_tuple",
                    "array": "array<",
                    "span": "span<",
                    "variant": "variant|visit|get_if",
                    "any": "any_cast|any<",
                    "expected": "expected|unexpected",
                }

                # Check if common usage pattern exists
                pattern = common_usage.get(include_name, include_name)
                if not re.search(pattern, content_clean):
                    unused.append(UnusedInclude(
                        file=file_str,
                        line=line_num,
                        include=include,
                        reason=f"No usage of '{include_name}' found in file",
                    ))

        return unused

    def analyze(self) -> AnalysisResult:
        """Run complete analysis"""
        result = AnalysisResult()

        # Find all files
        files = self.find_cpp_files()
        self.log(f"Found {len(files)} C++ files")

        # Phase 1: Extract symbols
        self.log("Phase 1: Extracting symbols...")
        for f in files:
            self.extract_symbols(f)
            self.extract_includes(f)
        self.log(f"Found {len(self.symbols)} symbols")

        # Phase 2: Count references
        self.log("Phase 2: Counting references...")
        for f in files:
            self.count_references(f)

        # Phase 3: Detect unused
        self.log("Phase 3: Detecting unused code...")

        for key, symbol in self.symbols.items():
            refs = self.references.get(key, 0)

            # Skip symbols that are likely used externally
            if symbol.is_public:
                continue
            if symbol.is_virtual or symbol.is_override:
                continue
            if symbol.is_test:
                continue
            if symbol.name.startswith("_"):  # Private/internal
                continue
            if symbol.name in ["main", "instance", "getInstance"]:
                continue

            # Check for low references
            if refs < 1:
                if symbol.kind == "function":
                    result.unused_functions.append(symbol)
                elif symbol.kind in ["class", "struct"]:
                    result.unused_classes.append(symbol)
                elif symbol.kind == "variable":
                    result.unused_variables.append(symbol)
                elif symbol.kind == "constant":
                    result.unused_constants.append(symbol)
                elif symbol.kind == "macro":
                    result.unused_macros.append(symbol)

        # Phase 4: Detect unused includes
        self.log("Phase 4: Detecting unused includes...")
        for f in files:
            unused_inc = self.detect_unused_includes(f)
            result.unused_includes.extend(unused_inc)

        # Statistics
        result.statistics = {
            "total_files": len(files),
            "total_symbols": len(self.symbols),
            "unused_functions": len(result.unused_functions),
            "unused_classes": len(result.unused_classes),
            "unused_variables": len(result.unused_variables),
            "unused_constants": len(result.unused_constants),
            "unused_macros": len(result.unused_macros),
            "unused_includes": len(result.unused_includes),
        }

        return result


def format_text(result: AnalysisResult) -> str:
    """Format result as human-readable text"""
    lines = []
    lines.append("=" * 70)
    lines.append("DEAD CODE ANALYSIS REPORT")
    lines.append("=" * 70)
    lines.append("")

    # Statistics
    lines.append("STATISTICS")
    lines.append("-" * 40)
    for key, value in result.statistics.items():
        lines.append(f"  {key.replace('_', ' ').title()}: {value}")
    lines.append("")

    # Unused functions
    if result.unused_functions:
        lines.append(f"POTENTIALLY UNUSED FUNCTIONS ({len(result.unused_functions)})")
        lines.append("-" * 40)
        for sym in sorted(result.unused_functions, key=lambda s: (s.file, s.line)):
            lines.append(f"  {sym.file}:{sym.line}")
            lines.append(f"    {sym.scope}{sym.name}()")
        lines.append("")

    # Unused classes
    if result.unused_classes:
        lines.append(f"POTENTIALLY UNUSED CLASSES ({len(result.unused_classes)})")
        lines.append("-" * 40)
        for sym in sorted(result.unused_classes, key=lambda s: (s.file, s.line)):
            lines.append(f"  {sym.file}:{sym.line}")
            lines.append(f"    {sym.kind} {sym.scope}{sym.name}")
        lines.append("")

    # Unused macros
    if result.unused_macros:
        lines.append(f"POTENTIALLY UNUSED MACROS ({len(result.unused_macros)})")
        lines.append("-" * 40)
        for sym in sorted(result.unused_macros, key=lambda s: (s.file, s.line)):
            lines.append(f"  {sym.file}:{sym.line}")
            lines.append(f"    #define {sym.name}")
        lines.append("")

    # Unused includes
    if result.unused_includes:
        lines.append(f"POTENTIALLY UNUSED INCLUDES ({len(result.unused_includes)})")
        lines.append("-" * 40)
        for inc in sorted(result.unused_includes, key=lambda i: (i.file, i.line)):
            lines.append(f"  {inc.file}:{inc.line}")
            lines.append(f"    #include <{inc.include}>")
            lines.append(f"    Reason: {inc.reason}")
        lines.append("")

    lines.append("=" * 70)
    lines.append("NOTE: This is heuristic analysis. Verify findings before deletion.")
    lines.append("      False positives may occur for:")
    lines.append("        - Code used via templates or macros")
    lines.append("        - Code used only in external libraries")
    lines.append("        - Code used via reflection (Qt signals/slots)")
    lines.append("=" * 70)

    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(
        description="Dead Code Elimination Analysis for MakineAI"
    )
    parser.add_argument(
        "--path",
        type=str,
        default="core/",
        help="Path to analyze (default: core/)"
    )
    parser.add_argument(
        "--format",
        choices=["text", "json"],
        default="text",
        help="Output format (default: text)"
    )
    parser.add_argument(
        "--verbose",
        "-v",
        action="store_true",
        help="Verbose output"
    )
    parser.add_argument(
        "--output",
        "-o",
        type=str,
        help="Output file (default: stdout)"
    )

    args = parser.parse_args()

    # Find project root
    script_dir = Path(__file__).parent
    project_root = script_dir.parent
    analyze_path = project_root / args.path

    if not analyze_path.exists():
        print(f"Error: Path '{analyze_path}' does not exist")
        return 1

    # Run analysis
    analyzer = DeadCodeAnalyzer(analyze_path, verbose=args.verbose)
    result = analyzer.analyze()

    # Format output
    if args.format == "json":
        output = json.dumps(result.to_dict(), indent=2)
    else:
        output = format_text(result)

    # Write output
    if args.output:
        Path(args.output).write_text(output, encoding="utf-8")
        print(f"Report written to {args.output}")
    else:
        print(output)

    # Return non-zero if issues found
    total_issues = (
        len(result.unused_functions) +
        len(result.unused_classes) +
        len(result.unused_includes)
    )
    return 1 if total_issues > 0 else 0


if __name__ == "__main__":
    exit(main())

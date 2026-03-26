#!/usr/bin/env python3
"""
Makine Memory Translation Extractor
General-purpose tool for extracting translation data from running game processes.

Supports:
  - RAGE Engine (GTA, RDR2): GXT2 hash tables, ~z~ dialogue markers
  - Unreal Engine 4/5: FText patterns (planned)
  - Unity: I2 Localization (planned)
  - Generic: Turkish character fingerprinting

Usage:
  python memory_extractor.py --process RDR2.exe --engine rage
  python memory_extractor.py --process game.exe --engine auto
  python memory_extractor.py --pid 12345 --engine generic
"""
import os, sys, ctypes, ctypes.wintypes, struct, json, time, argparse, re
from pathlib import Path
from dataclasses import dataclass, field, asdict
from typing import Optional
from collections import defaultdict

os.environ['PYTHONIOENCODING'] = 'utf-8'
sys.stdout.reconfigure(encoding='utf-8')

# ──────────────────────────────────────────────
# Windows API
# ──────────────────────────────────────────────
PROCESS_VM_READ = 0x0010
PROCESS_QUERY_INFORMATION = 0x0400
MEM_COMMIT = 0x1000
PAGE_READABLE = {0x02, 0x04, 0x06, 0x20, 0x40, 0x80}

kernel32 = ctypes.windll.kernel32

class MEMORY_BASIC_INFORMATION(ctypes.Structure):
    _fields_ = [
        ("BaseAddress", ctypes.c_void_p),
        ("AllocationBase", ctypes.c_void_p),
        ("AllocationProtect", ctypes.wintypes.DWORD),
        ("RegionSize", ctypes.c_size_t),
        ("State", ctypes.wintypes.DWORD),
        ("Protect", ctypes.wintypes.DWORD),
        ("Type", ctypes.wintypes.DWORD),
    ]

# ──────────────────────────────────────────────
# Data types
# ──────────────────────────────────────────────
@dataclass
class TranslationEntry:
    hash_hex: str
    meta_hex: str
    text: str
    raw_text: str
    category: str       # dialogue, ui, general
    encoding: str       # utf-8, utf-16-le
    address: str        # hex address
    length: int

@dataclass
class ExtractionStats:
    total_regions: int = 0
    total_bytes: int = 0
    scan_duration_s: float = 0
    raw_dialogue: int = 0
    raw_general: int = 0
    unique_dialogue: int = 0
    unique_general: int = 0
    encoding_fixes: int = 0

# ──────────────────────────────────────────────
# Turkish character constants
# ──────────────────────────────────────────────
TURKISH_CHARS = set('İıŞşÇçĞğÖöÜü')

TURKISH_UTF8_BYTES = [
    b'\xc4\xb0',  # İ
    b'\xc4\xb1',  # ı
    b'\xc5\x9f',  # ş
    b'\xc5\x9e',  # Ş
    b'\xc3\xa7',  # ç
    b'\xc3\x87',  # Ç
    b'\xc4\x9f',  # ğ
    b'\xc4\x9e',  # Ğ
    b'\xc3\xb6',  # ö
    b'\xc3\x96',  # Ö
    b'\xc3\xbc',  # ü
    b'\xc3\x9c',  # Ü
]

# ──────────────────────────────────────────────
# Known encoding obfuscations
# ──────────────────────────────────────────────
KNOWN_OBFUSCATIONS = {
    'criminal_deftones': {
        '\u01D4': 'i',   # ǔ → dotted i
        '\u01D3': 'İ',   # Ǔ → İ
    },
}

def detect_obfuscation(texts: list[str]) -> dict:
    """Auto-detect encoding obfuscation by statistical analysis"""
    char_freq = defaultdict(int)
    for text in texts[:1000]:  # Sample first 1000
        for c in text:
            char_freq[c] += 1

    fixes = {}

    # Check for known pattern: ǔ replacing i
    if char_freq.get('\u01D4', 0) > 100 and char_freq.get('i', 0) < char_freq.get('\u01D4', 0):
        fixes['\u01D4'] = 'i'
    if char_freq.get('\u01D3', 0) > 10:
        fixes['\u01D3'] = 'İ'

    return fixes

def apply_encoding_fix(text: str, fixes: dict) -> str:
    for old, new in fixes.items():
        text = text.replace(old, new)
    return text

# ──────────────────────────────────────────────
# Process utilities
# ──────────────────────────────────────────────
def find_process(name: str) -> Optional[int]:
    """Find process ID by name"""
    import subprocess
    result = subprocess.run(
        ['tasklist', '/FI', f'IMAGENAME eq {name}', '/FO', 'CSV', '/NH'],
        capture_output=True, text=True
    )
    for line in result.stdout.strip().split('\n'):
        if name.lower() in line.lower():
            parts = line.strip('"').split('","')
            if len(parts) >= 2:
                return int(parts[1].strip('"'))
    return None

def enumerate_regions(handle) -> list[tuple]:
    """Enumerate readable committed memory regions"""
    mbi = MEMORY_BASIC_INFORMATION()
    address = 0
    regions = []

    while kernel32.VirtualQueryEx(handle, ctypes.c_void_p(address),
                                   ctypes.byref(mbi), ctypes.sizeof(mbi)):
        base = mbi.BaseAddress if mbi.BaseAddress is not None else 0
        size = mbi.RegionSize if mbi.RegionSize else 0

        if size == 0:
            break

        if (mbi.State == MEM_COMMIT and
            mbi.Protect in PAGE_READABLE and
            0 < size < 500 * 1024 * 1024):
            regions.append((base, size))

        address = base + size
        if address >= 0x7FFFFFFFFFFF:
            break

    return regions

def read_region(handle, base: int, size: int) -> Optional[bytes]:
    """Read a memory region"""
    buf = ctypes.create_string_buffer(size)
    bytes_read = ctypes.c_size_t(0)
    success = kernel32.ReadProcessMemory(
        handle, ctypes.c_void_p(base), buf, size, ctypes.byref(bytes_read)
    )
    if success and bytes_read.value > 0:
        return buf.raw[:bytes_read.value]
    return None

# ──────────────────────────────────────────────
# Engine modules
# ──────────────────────────────────────────────
class BaseEngineModule:
    """Base class for engine-specific extraction"""
    name = "generic"

    def detect(self, data: bytes) -> bool:
        return False

    def extract_entries(self, data: bytes, base_addr: int) -> list[dict]:
        return []

class RAGEModule(BaseEngineModule):
    """RAGE Engine (GTA, RDR2) — GXT2 hash table extraction"""
    name = "rage"

    ZZ_MARKER = b'~z~'

    def detect(self, data: bytes) -> bool:
        return b'GXT2' in data or b'2TXG' in data or self.ZZ_MARKER in data

    def extract_entries(self, data: bytes, base_addr: int) -> list[dict]:
        results = []
        data_len = len(data)
        i = 0

        while i < data_len - 12:
            if data[i:i+3] == self.ZZ_MARKER and i >= 8:
                hash_val = struct.unpack_from('<I', data, i - 8)[0]
                meta_val = struct.unpack_from('<I', data, i - 4)[0]

                if hash_val != 0 and meta_val != 0:
                    end = i
                    while end < data_len and data[end] != 0:
                        end += 1

                    try:
                        text = data[i:end].decode('utf-8', errors='strict')
                        results.append({
                            'hash': hash_val,
                            'hash_hex': f'0x{hash_val:08x}',
                            'meta': meta_val,
                            'meta_hex': f'0x{meta_val:08x}',
                            'text': text,
                            'category': 'dialogue',
                            'encoding': 'utf-8',
                            'address': f'0x{(base_addr + i):x}',
                            'length': end - i,
                        })
                    except (UnicodeDecodeError, ValueError):
                        pass

                i += 3
            else:
                i += 1

        return results

class UnrealModule(BaseEngineModule):
    """Unreal Engine 4/5 — FText/LocRes pattern extraction"""
    name = "ue4"

    def detect(self, data: bytes) -> bool:
        # LocRes magic or common UE patterns
        return (b'\x0E\x14\x74\x75' in data or  # LocRes magic
                b'GAME_' in data or
                b'NSLocText' in data or
                b'.uasset' in data or
                b'.umap' in data)

    def extract_entries(self, data: bytes, base_addr: int) -> list[dict]:
        """Extract FText-like strings from UE memory"""
        results = []
        data_len = len(data)
        i = 0

        # UE4 FText in memory: [int32 length] [utf16 data] [0x0000]
        # Also: UTF-8 null-terminated strings near known patterns
        while i < data_len - 8:
            # Check for int32 length prefix followed by readable text
            if i + 4 < data_len:
                str_len = struct.unpack_from('<i', data, i)[0]

                # Positive length, reasonable size (UTF-16 = 2 bytes per char)
                if 4 <= str_len <= 2000:
                    # Try UTF-16LE (UE4 uses wchar_t internally)
                    byte_len = str_len * 2
                    if i + 4 + byte_len + 2 <= data_len:
                        raw = data[i + 4 : i + 4 + byte_len]
                        # Check for null terminator
                        term = struct.unpack_from('<H', data, i + 4 + byte_len)[0]
                        if term == 0:
                            try:
                                text = raw.decode('utf-16-le', errors='strict')
                                has_turkish = any(c in text for c in TURKISH_CHARS)
                                printable = sum(1 for c in text if c.isprintable())
                                if has_turkish and len(text) >= 3 and printable / max(len(text), 1) > 0.8:
                                    # Try to find FNV hash from preceding bytes
                                    hash_val = 0
                                    if i >= 4:
                                        hash_val = struct.unpack_from('<I', data, i - 4)[0]

                                    results.append({
                                        'hash': hash_val,
                                        'hash_hex': f'0x{hash_val:08x}',
                                        'meta': str_len,
                                        'meta_hex': f'0x{str_len:08x}',
                                        'text': text,
                                        'category': self._categorize(text),
                                        'encoding': 'utf-16-le',
                                        'address': f'0x{(base_addr + i + 4):x}',
                                        'length': byte_len,
                                    })
                                    i += 4 + byte_len + 2
                                    continue
                            except (UnicodeDecodeError, ValueError):
                                pass

                # Try negative length (UTF-8 in UE4: negative = UTF-8)
                if -2000 <= str_len <= -4:
                    utf8_len = -str_len
                    if i + 4 + utf8_len + 1 <= data_len and data[i + 4 + utf8_len] == 0:
                        raw = data[i + 4 : i + 4 + utf8_len]
                        try:
                            text = raw.decode('utf-8', errors='strict')
                            has_turkish = any(c in text for c in TURKISH_CHARS)
                            if has_turkish and len(text) >= 3:
                                printable = sum(1 for c in text if c.isprintable())
                                if printable / max(len(text), 1) > 0.8:
                                    hash_val = 0
                                    if i >= 4:
                                        hash_val = struct.unpack_from('<I', data, i - 4)[0]
                                    results.append({
                                        'hash': hash_val,
                                        'hash_hex': f'0x{hash_val:08x}',
                                        'meta': utf8_len,
                                        'meta_hex': f'0x{utf8_len:08x}',
                                        'text': text,
                                        'category': self._categorize(text),
                                        'encoding': 'utf-8',
                                        'address': f'0x{(base_addr + i + 4):x}',
                                        'length': utf8_len,
                                    })
                                    i += 4 + utf8_len + 1
                                    continue
                        except (UnicodeDecodeError, ValueError):
                            pass
            i += 1

        return results

    def _categorize(self, text: str) -> str:
        if len(text) > 100:
            return 'narrative'
        if text.isupper() or (len(text) < 60 and text[0:1].isupper()):
            return 'ui'
        return 'dialogue'


class UnityModule(BaseEngineModule):
    """Unity Engine — I2 Localization / general string extraction"""
    name = "unity"

    def detect(self, data: bytes) -> bool:
        return (b'I2Languages' in data or
                b'TextMeshPro' in data or
                b'UnityEngine' in data or
                b'Assembly-CSharp' in data)

    def extract_entries(self, data: bytes, base_addr: int) -> list[dict]:
        """Extract strings from Unity memory (C# managed heap strings)"""
        results = []
        data_len = len(data)
        i = 0

        # .NET/Mono strings: [int32 length] [utf16 data] (no null term required)
        # System.String layout: [vtable(8)] [int32 length] [char16[] data]
        while i < data_len - 12:
            str_len = struct.unpack_from('<i', data, i)[0]

            if 3 <= str_len <= 2000:
                byte_len = str_len * 2
                if i + 4 + byte_len <= data_len:
                    raw = data[i + 4 : i + 4 + byte_len]
                    try:
                        text = raw.decode('utf-16-le', errors='strict')
                        has_turkish = any(c in text for c in TURKISH_CHARS)
                        printable = sum(1 for c in text if c.isprintable())

                        if has_turkish and printable / max(len(text), 1) > 0.8:
                            results.append({
                                'hash': 0,
                                'hash_hex': '0x00000000',
                                'meta': str_len,
                                'meta_hex': f'0x{str_len:08x}',
                                'text': text,
                                'category': 'general',
                                'encoding': 'utf-16-le',
                                'address': f'0x{(base_addr + i + 4):x}',
                                'length': byte_len,
                            })
                    except (UnicodeDecodeError, ValueError):
                        pass
            i += 2  # Scan every 2 bytes for alignment

        return results


class GenericModule(BaseEngineModule):
    """Generic Turkish string extraction — works for ANY engine"""
    name = "generic"

    def detect(self, data: bytes) -> bool:
        return any(marker in data for marker in TURKISH_UTF8_BYTES)

    def extract_entries(self, data: bytes, base_addr: int) -> list[dict]:
        """
        Universal string extraction via Turkish character fingerprinting.
        Scans for both UTF-8 and UTF-16LE strings.
        Works regardless of game engine or text format.
        """
        results = []
        data_len = len(data)

        # ── UTF-8 scan ──
        i = 0
        while i < data_len:
            if 0x20 <= data[i] <= 0x7E or data[i] >= 0xC0:
                end = i
                while end < data_len and data[end] != 0:
                    end += 1

                if end - i >= 4:
                    try:
                        text = data[i:end].decode('utf-8', errors='strict')
                        has_turkish = any(c in text for c in TURKISH_CHARS)
                        # Also check for common obfuscation chars
                        has_obfuscated = '\u01D4' in text or '\u01D3' in text

                        if (has_turkish or has_obfuscated) and len(text) >= 4:
                            printable = sum(1 for c in text if c.isprintable() or c in '\n\r\t')
                            letters = sum(1 for c in text if c.isalpha())
                            if printable / len(text) > 0.8 and letters >= 3:
                                # Try to detect hash from preceding bytes
                                hash_val = 0
                                meta_val = 0
                                if i >= 8:
                                    hash_val = struct.unpack_from('<I', data, i - 8)[0]
                                    meta_val = struct.unpack_from('<I', data, i - 4)[0]

                                results.append({
                                    'hash': hash_val,
                                    'hash_hex': f'0x{hash_val:08x}',
                                    'meta': meta_val,
                                    'meta_hex': f'0x{meta_val:08x}',
                                    'text': text,
                                    'category': self._categorize(text),
                                    'encoding': 'utf-8',
                                    'address': f'0x{(base_addr + i):x}',
                                    'length': end - i,
                                })
                        i = end + 1
                        continue
                    except (UnicodeDecodeError, ValueError):
                        pass
                i = end + 1
            else:
                i += 1

        # ── UTF-16LE scan (strict filtering to avoid false positives) ──
        i = 0
        while i < data_len - 5:
            char_val = struct.unpack_from('<H', data, i)[0]
            if (0x20 <= char_val <= 0x7E) or (0xC0 <= char_val <= 0x017F):
                end = i
                while end < data_len - 1:
                    val = struct.unpack_from('<H', data, end)[0]
                    if val == 0:
                        break
                    end += 2

                byte_len = end - i
                char_count = byte_len // 2
                if char_count >= 6:  # Stricter minimum for UTF-16
                    raw = data[i:end]
                    try:
                        text = raw.decode('utf-16-le', errors='strict')
                        # Very strict: mostly ASCII/Latin-1 with Turkish chars
                        ascii_latin = sum(1 for c in text if ord(c) < 0x0180)
                        has_turkish = any(c in text for c in 'ŞşÇçĞğÖöÜü')  # Exclude İı (too many false positives)

                        if has_turkish and ascii_latin / max(len(text), 1) > 0.95:
                            printable = sum(1 for c in text if c.isprintable())
                            letters = sum(1 for c in text if c.isalpha())
                            if printable / max(len(text), 1) > 0.9 and letters >= 4:
                                results.append({
                                    'hash': 0,
                                    'hash_hex': '0x00000000',
                                    'meta': 0,
                                    'meta_hex': '0x00000000',
                                    'text': text,
                                    'category': self._categorize(text),
                                    'encoding': 'utf-16-le',
                                    'address': f'0x{(base_addr + i):x}',
                                    'length': byte_len,
                                })
                    except (UnicodeDecodeError, ValueError):
                        pass
                i = end + 2
            else:
                i += 2

        return results

    def _categorize(self, text: str) -> str:
        # RAGE markers
        if text.startswith('~z~') or '~z~' in text[:10]:
            return 'dialogue'
        if text.startswith('~s~') or text.startswith('~n~'):
            return 'system'
        # UE4 markers
        if '<text>' in text.lower() or 'nsloctext' in text.lower():
            return 'dialogue'
        # General heuristics
        if text.isupper() and len(text) < 80:
            return 'ui'
        if len(text) > 150:
            return 'narrative'
        if len(text) < 50 and text[0:1].isupper() and '\n' not in text:
            return 'ui'
        return 'general'

# Module registry
ENGINE_MODULES = {
    'rage': RAGEModule,
    'ue4': UnrealModule,
    'unity': UnityModule,
    'generic': GenericModule,
}

# ──────────────────────────────────────────────
# Main extraction logic
# ──────────────────────────────────────────────
def extract(process_name: str, engine: str, output_dir: str,
            pid: Optional[int] = None) -> ExtractionStats:
    """Main extraction function"""
    stats = ExtractionStats()

    print(f"{'='*60}")
    print(f"Makine Memory Translation Extractor")
    print(f"Engine: {engine}")
    print(f"{'='*60}")

    # Find process
    if pid is None:
        pid = find_process(process_name)
    if not pid:
        print(f"HATA: {process_name} bulunamadı!")
        return stats

    print(f"Process: {process_name} (PID: {pid})")

    # Open process
    handle = kernel32.OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, False, pid)
    if not handle:
        print(f"HATA: Process açılamadı (Error: {kernel32.GetLastError()})")
        return stats

    # Enumerate regions
    regions = enumerate_regions(handle)
    stats.total_regions = len(regions)
    stats.total_bytes = sum(r[1] for r in regions)

    print(f"Bölge: {stats.total_regions}")
    print(f"Toplam: {stats.total_bytes / 1024 / 1024:.0f} MB")

    # Initialize engine module
    module_cls = ENGINE_MODULES.get(engine, GenericModule)
    module = module_cls()

    # If auto-detect, try each module on first few regions
    if engine == 'auto':
        for _, (base, size) in zip(range(min(5, len(regions))), regions):
            data = read_region(handle, base, size)
            if data:
                for name, cls in ENGINE_MODULES.items():
                    m = cls()
                    if m.detect(data):
                        module = m
                        engine = name
                        print(f"Motor tespit: {name}")
                        break
                if engine != 'auto':
                    break

    # Scan
    all_entries = []
    t_start = time.time()

    for i, (base, size) in enumerate(regions):
        if i % 200 == 0:
            elapsed = time.time() - t_start
            print(f"\r[{elapsed:.0f}s] {i}/{len(regions)} bölge | "
                  f"Bulunan: {len(all_entries)}", end='', flush=True)

        data = read_region(handle, base, size)
        if not data:
            continue

        entries = module.extract_entries(data, base)
        all_entries.extend(entries)

    stats.scan_duration_s = time.time() - t_start
    print(f"\rTarama tamamlandı: {len(regions)} bölge, "
          f"{stats.total_bytes/1024/1024:.0f} MB, {stats.scan_duration_s:.0f}s")

    kernel32.CloseHandle(handle)

    # Dedup by hash (or by text for generic)
    seen = {}
    for e in all_entries:
        key = e['hash'] if e['hash'] != 0 else e['text']
        if key not in seen or len(e['text']) > len(seen[key]['text']):
            seen[key] = e

    unique = list(seen.values())
    stats.raw_dialogue = sum(1 for e in all_entries if e['category'] == 'dialogue')
    stats.raw_general = len(all_entries) - stats.raw_dialogue

    # Detect and apply encoding fixes
    texts = [e['text'] for e in unique]
    encoding_fixes = detect_obfuscation(texts)

    if encoding_fixes:
        print(f"\nEncoding obfuscation tespit edildi: {encoding_fixes}")
        for e in unique:
            e['raw_text'] = e['text']
            e['text'] = apply_encoding_fix(e['text'], encoding_fixes)
        stats.encoding_fixes = sum(1 for e in unique if e.get('raw_text', '') != e['text'])

    # Categorize
    dialogue = [e for e in unique if e['category'] == 'dialogue']
    general = [e for e in unique if e['category'] != 'dialogue']
    stats.unique_dialogue = len(dialogue)
    stats.unique_general = len(general)

    # Sort
    dialogue.sort(key=lambda x: x.get('hash', 0))
    general.sort(key=lambda x: x['text'])

    # Save outputs
    os.makedirs(output_dir, exist_ok=True)

    # Complete database
    db = {
        'version': '1.0',
        'tool': 'Makine Memory Translation Extractor',
        'game': process_name,
        'engine': engine,
        'stats': asdict(stats),
        'encoding_fixes': encoding_fixes,
        'entries': dialogue + general,
    }

    db_file = os.path.join(output_dir, 'translation_db.json')
    with open(db_file, 'w', encoding='utf-8') as f:
        json.dump(db, f, ensure_ascii=False, indent=2)

    # Hash→text map (for ASI/mod development)
    hash_map = {}
    for e in dialogue + general:
        if e['hash_hex'] != '0x00000000':
            hash_map[e['hash_hex']] = e.get('raw_text', e['text'])

    if hash_map:
        map_file = os.path.join(output_dir, 'hash_map.json')
        with open(map_file, 'w', encoding='utf-8') as f:
            json.dump(hash_map, f, ensure_ascii=False, indent=2)

    # Clean text export
    txt_file = os.path.join(output_dir, 'translations.txt')
    with open(txt_file, 'w', encoding='utf-8') as f:
        for e in dialogue + general:
            f.write(f"{e['text']}\n")

    # Print summary
    print(f"\n{'='*60}")
    print(f"SONUÇLAR")
    print(f"{'='*60}")
    print(f"Diyalog: {stats.unique_dialogue}")
    print(f"Genel: {stats.unique_general}")
    print(f"Toplam: {stats.unique_dialogue + stats.unique_general}")
    print(f"Encoding düzeltme: {stats.encoding_fixes}")
    print(f"Süre: {stats.scan_duration_s:.0f}s")
    print(f"\nÇıktı: {output_dir}")
    print(f"  {db_file} ({os.path.getsize(db_file)/1024/1024:.1f} MB)")
    if hash_map:
        print(f"  {map_file}")
    print(f"  {txt_file}")

    return stats

# ──────────────────────────────────────────────
# CLI
# ──────────────────────────────────────────────
def main():
    parser = argparse.ArgumentParser(
        description='Makine Memory Translation Extractor',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s --process RDR2.exe --engine rage
  %(prog)s --process "Elden Ring.exe" --engine generic
  %(prog)s --pid 12345 --engine auto --output my_game/
        """
    )
    parser.add_argument('--process', '-p', default='RDR2.exe',
                       help='Game process name (default: RDR2.exe)')
    parser.add_argument('--pid', type=int, default=None,
                       help='Process ID (alternative to --process)')
    parser.add_argument('--engine', '-e', default='auto',
                       choices=['auto', 'rage', 'ue4', 'unity', 'generic'],
                       help='Engine module (default: auto)')
    parser.add_argument('--output', '-o', default=None,
                       help='Output directory (default: ./extracted/<process>)')

    args = parser.parse_args()

    output_dir = args.output
    if output_dir is None:
        name = args.process.replace('.exe', '').replace('.', '_')
        output_dir = f'./extracted/{name}'

    extract(args.process, args.engine, output_dir, pid=args.pid)

if __name__ == '__main__':
    main()

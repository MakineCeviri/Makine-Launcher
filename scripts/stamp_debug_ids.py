#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Makine Çeviri

"""
stamp_debug_ids.py -- Give the shipped third-party DLLs a Debug ID so Sentry
can symbolicate their frames.

Every fatal crash arrives with its interesting frames in Qt6Core.dll,
Qt6Quick.dll, libstdc++-6.dll, libgcc_s_seh-1.dll and friends — and Sentry
shows them as raw offsets, because the DLLs Qt/MinGW ship carry a symbol table
and unwind info but no CodeView record, so `sentry-cli debug-files check` reads
their Debug ID as all-zero ("Usable: no (missing debug identifier)"). Without a
Debug ID the event's module cannot be paired with any uploaded symbol file, so
the module is unsymbolicatable no matter what we upload.

Our own Makine-Launcher.exe does not have this problem: `-Wl,--build-id=sha1`
makes ld emit a `.buildid` section holding an IMAGE_DEBUG_DIRECTORY (Type 2 =
CODEVIEW) + an RSDS record, which is exactly the Debug ID carrier. This script
does to the third-party DLLs what ld would have done: it appends a `.buildid`
section with the same contents, so the shipped DLL and the symbol file uploaded
from it identify as the same module.

What it does, per *.dll under the deploy dir that has no CodeView record:
  1. derive a deterministic GUID = first 16 bytes of SHA-256 of the ORIGINAL
     file bytes, so the same Qt DLL gets the same Debug ID every release;
  2. append a new section ".buildid" (IMAGE_SCN_CNT_INITIALIZED_DATA |
     IMAGE_SCN_MEM_READ = 0x40000040) holding an IMAGE_DEBUG_DIRECTORY array
     (any pre-existing entries kept, one CODEVIEW entry added) + "RSDS" + GUID
     + Age(1) + "<pdb>\0";
  3. fix up NumberOfSections, SizeOfImage, DataDirectory[6] (DEBUG) and the PE
     CheckSum.

Constraints handled:
  * needs room for one more 40-byte section header before the first section's
    raw data; no room -> the file is skipped (sections are never shifted);
  * the new section's VirtualAddress is aligned to SectionAlignment after the
    last section; its raw data is appended at the end of the file, aligned to
    FileAlignment — the COFF symbol table + string table MinGW leaves at the
    end (PointerToSymbolTable) is left exactly where it is;
  * PE32+ only (PE32 / non-PE skipped);
  * Authenticode-signed files (DataDirectory[4] non-zero) are skipped and
    reported by default — appending a section invalidates the signature.
    `--include-signed` strips the (now-meaningless, since we repackage these
    DLLs ourselves) certificate table and stamps anyway; needed for the Qt
    DLLs, which Qt ships signed.

Idempotent: a file that already carries a CodeView record is left untouched, so
re-running over a deploy dir stamps nothing and leaves every file byte-identical.

Usage:
    python scripts/stamp_debug_ids.py <deploy-dir>
    python scripts/stamp_debug_ids.py <deploy-dir> --include-signed
    python scripts/stamp_debug_ids.py <deploy-dir> --json
"""

import argparse
import hashlib
import json
import struct
import sys
from pathlib import Path

for _stream in (sys.stdout, sys.stderr):
    if hasattr(_stream, "reconfigure"):
        _stream.reconfigure(encoding="utf-8", errors="replace")

# PE constants
PE32PLUS_MAGIC = 0x20B
IMAGE_SCN_CNT_INITIALIZED_DATA = 0x00000040
IMAGE_SCN_MEM_READ = 0x40000000
BUILDID_CHARACTERISTICS = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ  # 0x40000040
IMAGE_DEBUG_TYPE_CODEVIEW = 2
DEBUG_DIR_ENTRY_SIZE = 28  # IMAGE_DEBUG_DIRECTORY
SECTION_HEADER_SIZE = 40
DIR_DEBUG = 6       # DataDirectory index of the Debug directory
DIR_SECURITY = 4    # DataDirectory index of the certificate table (Authenticode)


def align_up(value: int, alignment: int) -> int:
    return (value + alignment - 1) // alignment * alignment


class PE:
    """Minimal PE32+ view with just the fields this script rewrites."""

    def __init__(self, data: bytes):
        self.data = bytearray(data)
        if self.data[:2] != b"MZ":
            raise ValueError("not an MZ/PE file")
        self.e_lfanew = struct.unpack_from("<I", self.data, 0x3C)[0]
        if self.data[self.e_lfanew:self.e_lfanew + 4] != b"PE\0\0":
            raise ValueError("no PE signature")
        coff = self.e_lfanew + 4
        self.coff = coff
        self.machine = struct.unpack_from("<H", self.data, coff)[0]
        self.num_sections = struct.unpack_from("<H", self.data, coff + 2)[0]
        self.ptr_symtab = struct.unpack_from("<I", self.data, coff + 8)[0]
        self.opt_size = struct.unpack_from("<H", self.data, coff + 16)[0]
        self.opt = coff + 20
        self.magic = struct.unpack_from("<H", self.data, self.opt)[0]
        if self.magic != PE32PLUS_MAGIC:
            raise ValueError("not PE32+")
        self.section_align = struct.unpack_from("<I", self.data, self.opt + 32)[0]
        self.file_align = struct.unpack_from("<I", self.data, self.opt + 36)[0]
        self.size_of_image_off = self.opt + 56
        self.size_of_headers = struct.unpack_from("<I", self.data, self.opt + 60)[0]
        self.checksum_off = self.opt + 64
        self.num_rva = struct.unpack_from("<I", self.data, self.opt + 108)[0]
        self.data_dir = self.opt + 112  # PE32+ data directory
        self.sections_off = self.opt + self.opt_size

    def data_dir_entry(self, index: int):
        off = self.data_dir + index * 8
        return struct.unpack_from("<II", self.data, off)  # (rva/offset, size)

    def set_data_dir_entry(self, index: int, rva: int, size: int):
        struct.pack_into("<II", self.data, self.data_dir + index * 8, rva, size)

    def section(self, i: int):
        off = self.sections_off + i * SECTION_HEADER_SIZE
        name = bytes(self.data[off:off + 8])
        vsize, vaddr, rawsize, rawptr = struct.unpack_from("<IIII", self.data, off + 8)
        return {"name": name, "vsize": vsize, "vaddr": vaddr,
                "rawsize": rawsize, "rawptr": rawptr, "off": off}

    def first_raw_offset(self) -> int:
        ptrs = [self.section(i)["rawptr"] for i in range(self.num_sections)
                if self.section(i)["rawptr"]]
        return min(ptrs) if ptrs else self.size_of_headers

    def header_room(self) -> int:
        """Bytes free between the end of the section table and the first raw data."""
        table_end = self.sections_off + self.num_sections * SECTION_HEADER_SIZE
        return self.first_raw_offset() - table_end

    def has_codeview(self) -> bool:
        rva, size = self.data_dir_entry(DIR_DEBUG)
        if not rva or not size:
            return False
        for j in range(size // DEBUG_DIR_ENTRY_SIZE):
            entry = self._debug_entry_at_rva(rva + j * DEBUG_DIR_ENTRY_SIZE)
            if entry is not None and entry[4] == IMAGE_DEBUG_TYPE_CODEVIEW:
                return True
        return False

    def _rva_to_offset(self, rva: int):
        for i in range(self.num_sections):
            s = self.section(i)
            if s["vaddr"] <= rva < s["vaddr"] + max(s["vsize"], s["rawsize"]):
                return s["rawptr"] + (rva - s["vaddr"])
        return None

    def _debug_entry_at_rva(self, rva: int):
        off = self._rva_to_offset(rva)
        if off is None or off + DEBUG_DIR_ENTRY_SIZE > len(self.data):
            return None
        return struct.unpack_from("<IIHHIIII", self.data, off)

    def existing_debug_entries(self) -> bytes:
        """Raw bytes of the current debug directory array (may be empty)."""
        rva, size = self.data_dir_entry(DIR_DEBUG)
        if not rva or not size:
            return b""
        off = self._rva_to_offset(rva)
        if off is None:
            return b""
        return bytes(self.data[off:off + size])

    def last_section_end_rva(self) -> int:
        end = 0
        for i in range(self.num_sections):
            s = self.section(i)
            end = max(end, s["vaddr"] + s["vsize"])
        return end

    def recompute_checksum(self):
        struct.pack_into("<I", self.data, self.checksum_off, 0)
        buf = self.data if len(self.data) % 2 == 0 else self.data + b"\x00"
        total = 0
        cs = self.checksum_off
        for i in range(0, len(buf), 2):
            if i == cs or i == cs + 2:
                continue
            total += buf[i] | (buf[i + 1] << 8)
            total = (total & 0xFFFF) + (total >> 16)
        total = (total & 0xFFFF) + (total >> 16)
        total = (total + len(self.data)) & 0xFFFFFFFF
        struct.pack_into("<I", self.data, cs, total)


def stamp(path: Path, include_signed: bool) -> dict:
    """Return {"status": ..., "detail": ...}. Writes the file only on 'stamped'."""
    original = path.read_bytes()
    try:
        pe = PE(original)
    except ValueError as exc:
        return {"status": "skipped", "detail": str(exc)}

    if pe.has_codeview():
        return {"status": "had_id", "detail": "already carries a CodeView record"}

    sec_off, sec_size = pe.data_dir_entry(DIR_SECURITY)
    if sec_size:
        if not include_signed:
            return {"status": "skipped", "detail": "Authenticode-signed (use --include-signed)"}
        # Strip the certificate table: appending a section invalidates it anyway,
        # and we repackage these DLLs (unsigned ZIP / re-signed MSIX) ourselves.
        # The cert blob sits at the very end of the file (after the symbol table).
        if sec_off + sec_size <= len(pe.data):
            del pe.data[sec_off:sec_off + sec_size]
        pe.set_data_dir_entry(DIR_SECURITY, 0, 0)

    if pe.header_room() < SECTION_HEADER_SIZE:
        return {"status": "skipped",
                "detail": f"no room for a section header ({pe.header_room()} < {SECTION_HEADER_SIZE} B)"}

    # Deterministic GUID: first 16 bytes of SHA-256 of the ORIGINAL file.
    guid = hashlib.sha256(original).digest()[:16]
    pdb_name = path.stem + ".pdb"

    # CodeView RSDS record: "RSDS" + GUID(16) + Age(4, LE) + name + NUL.
    codeview = b"RSDS" + guid + struct.pack("<I", 1) + pdb_name.encode("ascii") + b"\x00"

    # New debug directory = existing entries + one CODEVIEW entry.
    existing = pe.existing_debug_entries()
    new_dir_size = len(existing) + DEBUG_DIR_ENTRY_SIZE

    # Section layout: [debug directory array][codeview record].
    new_vaddr = align_up(pe.last_section_end_rva(), pe.section_align)
    cv_rva = new_vaddr + new_dir_size
    raw_ptr = align_up(len(pe.data), pe.file_align)
    cv_fileptr = raw_ptr + new_dir_size

    cv_entry = struct.pack("<IIHHIIII",
                           0,                         # Characteristics
                           0,                         # TimeDateStamp
                           0, 0,                      # Major/Minor version
                           IMAGE_DEBUG_TYPE_CODEVIEW,  # Type
                           len(codeview),             # SizeOfData
                           cv_rva,                    # AddressOfRawData (RVA)
                           cv_fileptr)                # PointerToRawData (file offset)
    section_body = existing + cv_entry + codeview
    vsize = len(section_body)
    raw_size = align_up(vsize, pe.file_align)

    # Write the new section header into the free space after the section table.
    hdr_off = pe.sections_off + pe.num_sections * SECTION_HEADER_SIZE
    struct.pack_into("<8sIIIIIIHHI", pe.data, hdr_off,
                     b".buildid",      # Name (8 bytes, fits exactly)
                     vsize,            # VirtualSize
                     new_vaddr,        # VirtualAddress
                     raw_size,         # SizeOfRawData
                     raw_ptr,          # PointerToRawData
                     0, 0, 0, 0,       # PointerToRelocations/Linenumbers, counts
                     BUILDID_CHARACTERISTICS)

    # Append the section raw data (padded to FileAlignment), symbol table untouched.
    pe.data += b"\x00" * (raw_ptr - len(pe.data))   # pad gap up to raw_ptr
    pe.data += section_body + b"\x00" * (raw_size - vsize)

    # Header fixups.
    struct.pack_into("<H", pe.data, pe.coff + 2, pe.num_sections + 1)  # NumberOfSections
    struct.pack_into("<I", pe.data, pe.size_of_image_off,
                     align_up(new_vaddr + vsize, pe.section_align))    # SizeOfImage
    pe.set_data_dir_entry(DIR_DEBUG, new_vaddr, new_dir_size)          # DataDirectory[6]
    pe.recompute_checksum()

    path.write_bytes(bytes(pe.data))
    return {"status": "stamped",
            "detail": f"{pdb_name} +{len(section_body)} B"
                      + ("  (signature stripped)" if sec_size else "")}


def debug_id_of_guid(guid16_hex: str) -> str:
    """Format the GUID the way sentry-cli reports it (little-endian field swap)."""
    b = bytes.fromhex(guid16_hex)
    d1 = struct.unpack_from("<I", b, 0)[0]
    d2 = struct.unpack_from("<H", b, 4)[0]
    d3 = struct.unpack_from("<H", b, 6)[0]
    return f"{d1:08x}-{d2:04x}-{d3:04x}-{b[8:10].hex()}-{b[10:16].hex()}"


def main() -> int:
    parser = argparse.ArgumentParser(description="Stamp a Debug ID onto shipped third-party DLLs")
    parser.add_argument("deploy_dir", help="Directory to scan recursively for *.dll")
    parser.add_argument("--include-signed", action="store_true",
                        help="Strip the certificate table and stamp Authenticode-signed DLLs too")
    parser.add_argument("--json", action="store_true", help="Machine-readable summary")
    args = parser.parse_args()

    root = Path(args.deploy_dir)
    if not root.is_dir():
        print(f"HATA: dizin yok: {root}", file=sys.stderr)
        return 1

    results = {"stamped": [], "had_id": [], "skipped": []}
    for dll in sorted(root.rglob("*.dll")):
        rel = dll.relative_to(root).as_posix()
        r = stamp(dll, args.include_signed)
        results[r["status"]].append({"file": rel, "detail": r["detail"]})

    if args.json:
        print(json.dumps(results, ensure_ascii=False, indent=2))
        return 0

    for item in results["stamped"]:
        print(f"  stamped   {item['file']:52s} {item['detail']}")
    for item in results["had_id"]:
        print(f"  had id    {item['file']:52s} {item['detail']}")
    for item in results["skipped"]:
        print(f"  skipped   {item['file']:52s} {item['detail']}")
    print(f"\n  Özet: {len(results['stamped'])} damgalandı · "
          f"{len(results['had_id'])} zaten kimlikli · {len(results['skipped'])} atlandı")
    return 0


if __name__ == "__main__":
    sys.exit(main())

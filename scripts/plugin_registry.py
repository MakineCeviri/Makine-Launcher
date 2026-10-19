#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Makine Çeviri

"""
plugin_registry.py — package Makine plugins and maintain the signed registry.

    pack   PLUGIN_DIR [--out FILE]        manifest v2 check + <id>-<version>.mkplugin
    add    PACKAGE --index FILE           put the package's entry into index.json
    sign   --index FILE [--key PEM]       write FILE.sig (Ed25519 over the exact bytes)
    verify --index FILE --pub BASE64      check FILE.sig
    keygen --out PEM                      new signing key; prints only the public key

The launcher accepts assets/plugins/index.json only with a valid index.json.sig
made by the key whose public half is compiled in (qml/src/services/pluginregistry.cpp).
Package hashes inside the signed index then cover the packages themselves.
The private key never enters the repository: pass --key or set
MAKINE_PLUGIN_SIGNING_KEY to its path.

Package format: MKPK v2 = b"MKPK" + b"\\x02" + zstd(tar), the same container the
launcher already reads (qml/src/services/mkpkformat.h).
"""

import argparse
import base64
import datetime
import hashlib
import io
import json
import os
import re
import sys
import tarfile

CDN_PLUGIN_BASE = "https://cdn.makineceviri.org/data/plugins"
ABI_VERSION = 2
CAPABILITIES = {"network", "screen-capture", "overlay", "game-files-read",
                "game-files-write", "process-inject"}
EVENTS = {"game.started", "game.stopped", "translation.installed",
          "translation.uninstalled", "settings.changed"}
SETTING_TYPES = {"toggle", "select", "text", "secret", "number", "path"}
# Plugins are DLLs plus data; scripts and installers have no place in one.
FORBIDDEN_EXTENSIONS = {".bat", ".cmd", ".ps1", ".vbs", ".msi", ".js", ".lnk", ".scr"}

ID_RE = re.compile(r"^[a-z0-9][a-z0-9-]*(\.[a-z0-9][a-z0-9-]*)+$")
VERSION_RE = re.compile(r"^\d+\.\d+\.\d+(-[0-9A-Za-z.-]+)?$")
KEY_RE = re.compile(r"^[A-Za-z][A-Za-z0-9_.-]{0,63}$")


def die(message: str) -> None:
    print(f"ERROR: {message}", file=sys.stderr)
    sys.exit(1)


def is_number(value) -> bool:
    return isinstance(value, (int, float)) and not isinstance(value, bool)


def text(value) -> str:
    return value if isinstance(value, str) else ""


def setting_value_fits(setting: dict, value) -> bool:
    """Mirror of pluginmanifest::isValidSettingValue()."""
    kind = setting.get("type")
    if kind == "toggle":
        return isinstance(value, bool)
    if kind == "number":
        return (is_number(value)
                and not ("min" in setting and value < setting["min"])
                and not ("max" in setting and value > setting["max"]))
    if not isinstance(value, str) or len(value) > 4096:
        return False
    if kind == "select":
        return value in [o if isinstance(o, str) else o.get("value") for o in setting.get("options", [])]
    return True


def check_setting(setting: dict) -> None:
    key, kind = text(setting.get("key")), setting.get("type")
    if not KEY_RE.match(key):
        die(f"setting key {key!r} is invalid")
    if kind not in SETTING_TYPES:
        die(f"setting {key}: unknown type {kind!r}")
    if not text(setting.get("label")).strip():
        die(f"setting {key}: label is missing")
    if kind == "select":
        options = setting.get("options", [])
        if not options:
            die(f"setting {key}: select needs options")
        for option in options:
            value = option if isinstance(option, str) else (option or {}).get("value")
            if not isinstance(value, str) or not value:
                die(f"setting {key}: option without a value")
    if kind == "number" and "min" in setting and "max" in setting and setting["min"] > setting["max"]:
        die(f"setting {key}: min is above max")
    if kind == "path" and setting.get("mode", "folder") not in ("folder", "file"):
        die(f"setting {key}: mode must be file or folder")
    if "default" in setting:
        if kind == "secret":
            die(f"setting {key}: a secret cannot have a default")
        if not setting_value_fits(setting, setting["default"]):
            die(f"setting {key}: default does not fit the type")


def check_manifest(manifest: dict, plugin_dir: str | None = None) -> None:
    """Mirror of PluginManifest::parse(): a package that passes here installs."""
    if not isinstance(manifest, dict) or manifest.get("schema") != 2:
        die("manifest schema must be 2")
    plugin_id = text(manifest.get("id"))
    if not ID_RE.match(plugin_id) or len(plugin_id) > 64:
        die(f"id {plugin_id!r} is not reverse-DNS (e.g. org.example.plugin)")
    name = text(manifest.get("name")).strip()
    if not name or len(name) > 64:
        die("name is missing or longer than 64 characters")
    if not VERSION_RE.match(text(manifest.get("version"))):
        die(f"version {manifest.get('version')!r} is not semver")
    if not VERSION_RE.match(text(manifest.get("minLauncher", "0.0.0"))):
        die("minLauncher is not semver")
    if manifest.get("abi") != ABI_VERSION:
        die(f"abi must be {ABI_VERSION}")
    entry = text(manifest.get("entry"))
    parts = entry.replace("\\", "/").split("/")
    if (not entry.lower().endswith(".dll") or ":" in entry or entry.startswith(("/", "\\"))
            or any(p in ("", ".", "..") for p in parts)):
        die(f"entry {entry!r} must be a .dll inside the plugin folder")
    if plugin_dir and not os.path.isfile(os.path.join(plugin_dir, entry)):
        die(f"entry {entry} is not in {plugin_dir}")
    if not text(manifest.get("publisher")).strip():
        die("publisher is missing")
    if len(text(manifest.get("description")).strip()) > 500:
        die("description is longer than 500 characters")
    seen = set()
    for cap in manifest.get("capabilities", []):
        cap_id = text((cap or {}).get("id"))
        if cap_id not in CAPABILITIES:
            die(f"unknown capability {cap_id!r}")
        if not text(cap.get("reason")).strip():
            die(f"capability {cap_id} needs a reason")
        if cap_id in seen:
            die(f"capability {cap_id} is listed twice")
        seen.add(cap_id)
    contributes = manifest.get("contributes", {})
    seen = set()
    for setting in contributes.get("settings", []):
        check_setting(setting)
        if setting["key"] in seen:
            die(f"setting {setting['key']} is listed twice")
        seen.add(setting["key"])
    seen = set()
    for action in contributes.get("actions", []):
        action_id = text(action.get("id"))
        if not KEY_RE.match(action_id) or not text(action.get("label")).strip():
            die(f"action {action_id!r} needs a valid id and a label")
        if action_id in seen:
            die(f"action {action_id} is listed twice")
        seen.add(action_id)
    for event in contributes.get("events", []):
        if event not in EVENTS:
            die(f"unknown event {event!r}")


def load_manifest(plugin_dir: str) -> dict:
    path = os.path.join(plugin_dir, "manifest.json")
    if not os.path.isfile(path):
        die(f"manifest.json not found in {plugin_dir}")
    with open(path, encoding="utf-8") as f:
        manifest = json.load(f)
    check_manifest(manifest, plugin_dir)
    return manifest


def tar_bytes(plugin_dir: str) -> bytes:
    buf = io.BytesIO()
    # USTAR per entry, GNU only for names USTAR cannot hold: the launcher's tar
    # reader ignores PAX records (scripts/package_pipeline.py has the story).
    with tarfile.open(fileobj=buf, mode="w", format=tarfile.USTAR_FORMAT, encoding="utf-8") as tar:
        for root, dirs, files in os.walk(plugin_dir):
            # Dot entries (.git, plugin_dev.py's .dev-data) are never part of a package.
            dirs[:] = sorted(d for d in dirs if not d.startswith("."))
            for name in sorted(f for f in files if not f.startswith(".")):
                full = os.path.join(root, name)
                rel = os.path.relpath(full, plugin_dir).replace(os.sep, "/")
                if os.path.splitext(name)[1].lower() in FORBIDDEN_EXTENSIONS:
                    die(f"forbidden file type in plugin: {rel}")
                info = tar.gettarinfo(full, arcname=rel)
                info.uid = info.gid = 0
                info.uname = info.gname = ""
                info.mode = 0o644
                try:
                    info.tobuf(tarfile.USTAR_FORMAT, "utf-8", "surrogateescape")
                    tar.format = tarfile.USTAR_FORMAT
                except ValueError:
                    tar.format = tarfile.GNU_FORMAT
                with open(full, "rb") as f:
                    tar.addfile(info, f)
    return buf.getvalue()


def cmd_pack(args) -> None:
    try:
        import zstandard
    except ImportError:
        die("pip install zstandard")
    manifest = load_manifest(args.plugin_dir)
    out = args.out or f"{manifest['id']}-{manifest['version']}.mkplugin"
    payload = zstandard.ZstdCompressor(level=19).compress(tar_bytes(args.plugin_dir))
    with open(out, "wb") as f:
        f.write(b"MKPK\x02" + payload)
    print(f"{out}  {os.path.getsize(out)} bytes  sha256 {sha256_file(out)}")


def sha256_file(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def manifest_from_package(path: str) -> dict:
    import zstandard
    with open(path, "rb") as f:
        data = f.read()
    if data[:5] != b"MKPK\x02":
        die(f"{path} is not an MKPK v2 package")
    raw = zstandard.ZstdDecompressor().decompressobj().decompress(data[5:])
    with tarfile.open(fileobj=io.BytesIO(raw)) as tar:
        member = tar.extractfile("manifest.json")
        if member is None:
            die("package has no manifest.json at its root")
        manifest = json.load(member)
    check_manifest(manifest)
    return manifest


def cmd_add(args) -> None:
    manifest = manifest_from_package(args.package)
    if os.path.exists(args.index):
        with open(args.index, encoding="utf-8") as f:
            index = json.load(f)
    else:
        index = {"schema": 1, "plugins": []}
    entry = {
        "id": manifest["id"],
        "name": manifest["name"],
        "version": manifest["version"],
        "description": manifest.get("description", ""),
        "publisher": manifest["publisher"],
        "homepage": manifest.get("homepage", ""),
        "license": manifest.get("license", ""),
        "abi": manifest["abi"],
        "minLauncher": manifest.get("minLauncher", "0.0.0"),
        "capabilities": manifest.get("capabilities", []),
        "url": f"{CDN_PLUGIN_BASE}/{manifest['id']}/{manifest['version']}.mkplugin",
        "sha256": sha256_file(args.package),
        "size": os.path.getsize(args.package),
    }
    index["plugins"] = [p for p in index.get("plugins", []) if p.get("id") != entry["id"]] + [entry]
    index["plugins"].sort(key=lambda p: p["id"])
    index["generatedAt"] = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    os.makedirs(os.path.dirname(os.path.abspath(args.index)), exist_ok=True)
    with open(args.index, "w", encoding="utf-8", newline="\n") as f:
        json.dump(index, f, ensure_ascii=False, indent=2)
        f.write("\n")
    print(f"{entry['id']} {entry['version']} -> {args.index}")
    print(f"upload the package to: data/plugins/{entry['id']}/{entry['version']}.mkplugin, then sign")


def signing_key_path(args) -> str:
    path = args.key or os.environ.get("MAKINE_PLUGIN_SIGNING_KEY")
    if not path:
        die("pass --key or set MAKINE_PLUGIN_SIGNING_KEY")
    return path


def cmd_sign(args) -> None:
    from cryptography.hazmat.primitives import serialization
    with open(signing_key_path(args), "rb") as f:
        key = serialization.load_pem_private_key(f.read(), password=None)
    with open(args.index, "rb") as f:
        signature = key.sign(f.read())
    with open(args.index + ".sig", "w", encoding="ascii", newline="\n") as f:
        f.write(base64.b64encode(signature).decode() + "\n")
    print(f"signed {args.index}")


def cmd_verify(args) -> None:
    from cryptography.exceptions import InvalidSignature
    from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PublicKey
    public = Ed25519PublicKey.from_public_bytes(base64.b64decode(args.pub))
    with open(args.index, "rb") as f:
        data = f.read()
    with open(args.index + ".sig", encoding="ascii") as f:
        signature = base64.b64decode(f.read().strip())
    try:
        public.verify(signature, data)
    except InvalidSignature:
        die("signature does NOT match")
    print("signature OK")


def cmd_keygen(args) -> None:
    from cryptography.hazmat.primitives import serialization
    from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
    if os.path.exists(args.out):
        die(f"{args.out} exists; refusing to overwrite a signing key")
    key = Ed25519PrivateKey.generate()
    pem = key.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8,
                            serialization.NoEncryption())
    fd = os.open(args.out, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(fd, "wb") as f:
        f.write(pem)
    public = key.public_key().public_bytes(serialization.Encoding.Raw,
                                           serialization.PublicFormat.Raw)
    print(base64.b64encode(public).decode())


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    p = sub.add_parser("pack"); p.add_argument("plugin_dir"); p.add_argument("--out"); p.set_defaults(fn=cmd_pack)
    p = sub.add_parser("add"); p.add_argument("package"); p.add_argument("--index", required=True); p.set_defaults(fn=cmd_add)
    p = sub.add_parser("sign"); p.add_argument("--index", required=True); p.add_argument("--key"); p.set_defaults(fn=cmd_sign)
    p = sub.add_parser("verify"); p.add_argument("--index", required=True); p.add_argument("--pub", required=True); p.set_defaults(fn=cmd_verify)
    p = sub.add_parser("keygen"); p.add_argument("--out", required=True); p.set_defaults(fn=cmd_keygen)
    args = parser.parse_args()
    args.fn(args)


if __name__ == "__main__":
    main()

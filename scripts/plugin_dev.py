#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Makine Çeviri

"""
plugin_dev.py — run a Makine plugin in the real makine-plugin-host.exe, without the launcher.

    python plugin_dev.py PLUGIN_DIR [--invoke METHOD [PARAMS_JSON]]...
                         [--event NAME PAYLOAD_JSON]... [--set KEY=VALUE]...
                         [--region X,Y,W,H] [--host PATH]

PLUGIN_DIR holds manifest.json and the entry DLL (an unpacked package). The tool
plays the launcher: it answers the plugin's host calls (settings, ui.*, overlay.*,
games.*) from local state and prints everything the plugin says. --invoke and
--event run in the order given; without either it reads commands from stdin:

    hello {"name": "x"}        invoke a method
    !game.started {"gameId": "1"}   send an event
    quit

Host: --host, else MAKINE_PLUGIN_HOST, else the launcher repository's
build/dev/makine-plugin-host.exe. Outside that repository pass --host: the host
ships next to Makine-Launcher.exe.
"""

import argparse
import json
import os
import struct
import subprocess
import sys
import threading

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from plugin_registry import check_manifest  # noqa: E402


class Host:
    def __init__(self, exe, manifest, plugin_dir, settings, region):
        self.manifest = manifest
        self.settings = settings
        self.region = region
        self.caps = {c["id"] for c in manifest.get("capabilities", [])}
        self.next_id = 1
        self.replies = {}
        self.cond = threading.Condition()
        self.write_lock = threading.Lock()
        dll = os.path.abspath(os.path.join(plugin_dir, manifest["entry"]))
        self.proc = subprocess.Popen([exe, dll], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                     stderr=subprocess.PIPE, cwd=os.path.dirname(dll))
        threading.Thread(target=self._read_stdout, daemon=True).start()
        threading.Thread(target=self._read_stderr, daemon=True).start()

    # ── wire ──
    def send(self, message):
        message["jsonrpc"] = "2.0"
        body = json.dumps(message, ensure_ascii=False).encode("utf-8")
        with self.write_lock:
            self.proc.stdin.write(struct.pack("<I", len(body)) + body)
            self.proc.stdin.flush()

    def request(self, method, params, timeout=60):
        with self.cond:
            rid = self.next_id
            self.next_id += 1
        self.send({"id": rid, "method": method, "params": params})
        with self.cond:
            if not self.cond.wait_for(lambda: rid in self.replies or self.proc.poll() is not None, timeout):
                return {"error": {"code": 7, "message": "timeout"}}
            return self.replies.pop(rid, {"error": {"code": 1, "message": "host exited"}})

    def _read_stdout(self):
        while True:
            head = self.proc.stdout.read(4)
            if len(head) < 4:
                break
            msg = json.loads(self.proc.stdout.read(struct.unpack("<I", head)[0]))
            method = msg.get("method")
            if method is None:
                with self.cond:
                    self.replies[msg.get("id")] = msg
                    self.cond.notify_all()
            elif method == "log":
                level = ["DEBUG", "INFO", "WARN", "ERROR"][max(0, min(3, msg["params"].get("level", 1)))]
                print(f"  [{level}] {msg['params'].get('message')}")
            elif method == "emit":
                self.answer(None, msg["params"].get("event"), msg["params"].get("payload") or {})
            elif method == "call":
                self.answer(msg.get("id"), msg["params"].get("method"), msg["params"].get("params") or {})
        with self.cond:
            self.cond.notify_all()

    def _read_stderr(self):
        for line in self.proc.stderr:
            print(f"  [stderr] {line.decode('utf-8', 'replace').rstrip()}")

    # ── what the launcher would do ──
    def answer(self, call_id, method, params):
        def ok(result):
            if call_id is not None:
                self.send({"id": call_id, "result": result})

        def fail(code, message):
            print(f"  ! {method}: {message}")
            if call_id is not None:
                self.send({"id": call_id, "error": {"code": code, "message": message}})

        def needs(cap):
            if cap in self.caps:
                return True
            fail(4, f"capability {cap} is not declared in manifest.json")
            return False

        print(f"  -> {'call' if call_id is not None else 'emit'} {method} {json.dumps(params, ensure_ascii=False)}")
        if method == "settings.get":
            if params.get("key") not in self.settings:
                return fail(3, f"unknown setting {params.get('key')}")
            return ok(self.settings[params["key"]])
        if method == "settings.all":
            return ok(self.settings)
        if method == "settings.set":
            self.settings[params.get("key")] = params.get("value")
            return ok(True)
        if method in ("ui.notify", "ui.setStatus"):
            return ok(True)
        if method == "games.running":
            return ok(None)
        if method == "games.list":
            return ok([]) if needs("game-files-read") else None
        if method == "games.requestAttach":
            if needs("process-inject"):
                ok({"allowed": True, "gameId": params.get("gameId"), "name": "Dev Game", "installPath": ""})
            return None
        if method == "overlay.pickRegion":
            if needs("overlay"):
                x, y, w, h = self.region
                ok({"x": x, "y": y, "width": w, "height": h})
            return None
        if method in ("overlay.show", "overlay.hide"):
            return ok(True) if needs("overlay") else None
        return fail(3, f"unknown method {method}")


def default_settings(manifest):
    values = {}
    for s in manifest.get("contributes", {}).get("settings", []):
        if s.get("type") == "secret":
            values[s["key"]] = ""
        elif "default" in s:
            values[s["key"]] = s["default"]
    return values


def parse_value(text):
    try:
        return json.loads(text)
    except ValueError:
        return text


class Step(argparse.Action):
    """Keeps --invoke and --event in command-line order."""

    def __call__(self, parser, namespace, values, option_string=None):
        namespace.steps.append((self.dest, values))


def main():
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")   # Turkish text on a cp1252 console
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("plugin_dir")
    parser.add_argument("--host")
    parser.add_argument("--invoke", nargs="+", action=Step, metavar=("METHOD", "PARAMS"))
    parser.add_argument("--event", nargs=2, action=Step, metavar=("NAME", "PAYLOAD"))
    parser.set_defaults(steps=[])
    parser.add_argument("--set", action="append", default=[], metavar="KEY=VALUE")
    parser.add_argument("--region", default="100,100,800,200")
    args = parser.parse_args()

    here = os.path.dirname(os.path.abspath(__file__))
    exe = args.host or os.environ.get("MAKINE_PLUGIN_HOST") or os.path.join(
        here, "..", "build", "dev", "makine-plugin-host.exe")
    if not os.path.isfile(exe):
        sys.exit(f"makine-plugin-host.exe not found ({exe}); pass --host or set MAKINE_PLUGIN_HOST")

    with open(os.path.join(args.plugin_dir, "manifest.json"), encoding="utf-8") as f:
        manifest = json.load(f)
    check_manifest(manifest, args.plugin_dir)

    settings = default_settings(manifest)
    for item in args.set:
        key, _, value = item.partition("=")
        settings[key] = parse_value(value)

    data_dir = os.path.abspath(os.path.join(args.plugin_dir, ".dev-data"))
    os.makedirs(data_dir, exist_ok=True)
    host = Host(exe, manifest, args.plugin_dir, settings, [int(v) for v in args.region.split(",")])
    init = {"pluginId": manifest["id"], "version": manifest["version"], "launcherVersion": "dev",
            "abi": 2, "locale": "tr", "dataDir": data_dir,
            "pluginDir": os.path.abspath(args.plugin_dir), "settings": settings}
    print(f"initialize {manifest['id']} {manifest['version']}")
    reply = host.request("initialize", {"init": init}, timeout=15)
    print(f"  = {json.dumps(reply.get('result', reply.get('error')), ensure_ascii=False)}")
    if "error" in reply:
        sys.exit(1)

    def invoke(method, params):
        print(f"invoke {method} {json.dumps(params, ensure_ascii=False)}")
        r = host.request("invoke", {"method": method, "params": params}, timeout=120)
        print(f"  = {json.dumps(r.get('result', r.get('error')), ensure_ascii=False)}")

    for kind, values in args.steps:
        if kind == "event":
            print(f"event {values[0]}")
            host.send({"method": "event", "params": {"event": values[0], "payload": parse_value(values[1])}})
        else:
            invoke(values[0], parse_value(values[1]) if len(values) > 1 else {})
    if not args.steps:
        print("commands: <method> [json] | !<event> [json] | quit")
        for line in sys.stdin:
            line = line.strip()
            if not line:
                continue
            if line == "quit":
                break
            name, _, rest = line.partition(" ")
            payload = parse_value(rest) if rest else {}
            if name.startswith("!"):
                host.send({"method": "event", "params": {"event": name[1:], "payload": payload}})
            else:
                invoke(name, payload)

    print("shutdown")
    host.request("shutdown", {}, timeout=5)
    try:
        host.proc.wait(5)
    except subprocess.TimeoutExpired:
        host.proc.kill()
    print(f"host exited with {host.proc.returncode}")


if __name__ == "__main__":
    main()

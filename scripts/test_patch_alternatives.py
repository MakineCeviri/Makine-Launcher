#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Makine Çeviri

"""Tests for patch_alternatives.py and the pipeline's handling of the optional
patch keys.  python scripts/test_patch_alternatives.py"""

import copy
import json
import os
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import patch_alternatives as pa  # noqa: E402

SHA = "sha256:" + "ab" * 32

GOOD = {
    "name": "ELDEN RING", "v": "2026-10-30", "sizeBytes": 10, "size": 9,
    "dataUrl": "https://cdn.makineceviri.org/data/1245620-8a16332d6fdc.makine",
    "checksum": SHA, "dirName": "ELDEN RING", "exe": ["eldenring.exe"],
    "label": "Topluluk çevirisi",
    "gameVersion": {"label": "1.16.1", "steamBuild": 16412345, "date": "2026-10-28"},
    "alternatives": [{
        "id": "dublaj", "label": "Türkçe dublaj", "description": "Ses de Türkçe.",
        "v": "2026-11-02", "sizeBytes": 123, "size": 100,
        "dataUrl": "https://cdn.makineceviri.org/data/1245620--dublaj-0123456789ab.makine",
        "checksum": SHA, "dirName": "ELDEN RING--dublaj",
        "contributors": [{"name": "B", "role": "Çevirmen"}],
        "gameVersion": {"label": "", "steamBuild": 16412345, "date": "2026-10-28"},
    }],
}


class CheckEntry(unittest.TestCase):
    def test_contract_example_is_clean(self):
        self.assertEqual(pa.check_entry("1245620", GOOD), [])

    def test_entry_without_new_keys_is_clean(self):
        self.assertEqual(pa.check_entry("1", {"name": "x", "v": "1"}), [])

    def test_bad_and_repeated_ids(self):
        e = copy.deepcopy(GOOD)
        e["alternatives"] += [dict(e["alternatives"][0], dirName="other"),
                              {"id": "Bad Id", "label": "x"},
                              {"id": "default", "label": "x"},
                              {"id": "-x", "label": "x"}]
        problems = pa.check_entry("1245620", e)
        self.assertTrue(any("tekrar" in p for p in problems))
        self.assertEqual(sum("id geçersiz" in p for p in problems), 3)

    def test_label_required(self):
        e = copy.deepcopy(GOOD)
        e["alternatives"][0]["label"] = ""
        self.assertTrue(any("label yok" in p for p in pa.check_entry("1", e)))

    def test_dataurl_and_checksum_come_together(self):
        e = copy.deepcopy(GOOD)
        del e["alternatives"][0]["checksum"]
        self.assertTrue(any("birlikte" in p for p in pa.check_entry("1", e)))
        e = copy.deepcopy(GOOD)
        del e["alternatives"][0]["dataUrl"]
        del e["alternatives"][0]["checksum"]
        self.assertEqual(pa.check_entry("1", e), [])   # no file yet: "Yakında"
        e = copy.deepcopy(GOOD)
        e["alternatives"][0]["checksum"] = "md5:x"
        self.assertTrue(any("sha256" in p for p in pa.check_entry("1", e)))

    def test_dirname_unique_across_patches(self):
        e = copy.deepcopy(GOOD)
        e["alternatives"][0]["dirName"] = "ELDEN RING"   # the default's
        self.assertTrue(any("dirName" in p for p in pa.check_entry("1", e)))

    def test_game_version_shape(self):
        for bad in ("1.16.1", {"steamBuild": "16412345"}, {"steamBuild": -1},
                    {"steamBuild": True}, {"date": "28.10.2026"}, {"label": 7}):
            e = copy.deepcopy(GOOD)
            e["gameVersion"] = bad
            self.assertTrue(pa.check_entry("1", e), bad)
        e = copy.deepcopy(GOOD)
        e["gameVersion"] = {"label": "", "steamBuild": 0, "date": ""}
        self.assertEqual(pa.check_entry("1", e), [])

    def test_alternatives_must_be_a_list_of_objects(self):
        self.assertTrue(pa.check_entry("1", {"alternatives": {"id": "x"}}))
        self.assertTrue(pa.check_entry("1", {"alternatives": [42]}))
        too_many = [{"id": f"a{i}", "label": "x"} for i in range(21)]
        self.assertTrue(any("en çok" in p for p in pa.check_entry("1", {"alternatives": too_many})))

    def test_iter_skips_what_the_launcher_drops(self):
        e = {"alternatives": [{"id": "a", "label": "x"}, {"id": "a", "label": "y"},
                              {"id": "B"}, 5, {"id": "c", "label": "z"}]}
        self.assertEqual([i for i, _ in pa.iter_alternatives(e)], ["a", "c"])

    def test_check_index_covers_soon(self):
        idx = {"packages": {"1": GOOD}, "soon": {"2": {"alternatives": "x"}}}
        self.assertEqual(len(pa.check_index(idx)), 1)


class PipelineManifest(unittest.TestCase):
    def setUp(self):
        try:
            import package_pipeline  # noqa: F401  (needs zstandard + cryptography)
        except SystemExit:
            self.skipTest("zstandard/cryptography not installed")
        self.pp = sys.modules["package_pipeline"]
        self.tmp = tempfile.TemporaryDirectory()
        self.index_path = Path(self.tmp.name) / "index.json"
        self.index_path.write_text(json.dumps({"version": 1, "packages": {"1245620": GOOD}}),
                                   encoding="utf-8")

    def tearDown(self):
        self.tmp.cleanup()

    def test_alternative_update_keeps_every_other_key(self):
        r = {"appId": "1245620", "alt": "dublaj", "encryptedSize": 555, "rawSize": 777,
             "checksum": SHA.replace("ab", "cd"), "fileName": "1245620--dublaj-cdcdcdcdcdcd.makine",
             "dirName": "ELDEN RING--dublaj"}
        out = self.pp.update_manifest(self.index_path, Path(self.tmp.name), [r],
                                      "https://cdn.makineceviri.org/data")
        self.assertNotIn("error", out)
        index = json.loads(self.index_path.read_text(encoding="utf-8"))
        e = index["packages"]["1245620"]
        alt = e["alternatives"][0]
        self.assertEqual(alt["size"], 555)
        self.assertEqual(alt["checksum"], r["checksum"])
        self.assertTrue(alt["dataUrl"].endswith("/1245620--dublaj-cdcdcdcdcdcd.makine"))
        # the default and the new keys are untouched
        self.assertEqual(e["checksum"], SHA)
        self.assertEqual(e["label"], "Topluluk çevirisi")
        self.assertEqual(e["gameVersion"]["steamBuild"], 16412345)
        self.assertEqual(alt["gameVersion"]["steamBuild"], 16412345)

    def test_index_with_broken_alternatives_is_not_written(self):
        bad = copy.deepcopy(GOOD)
        bad["alternatives"].append({"id": "Bad", "label": "x"})
        self.index_path.write_text(json.dumps({"packages": {"1245620": bad}}), encoding="utf-8")
        before = self.index_path.read_text(encoding="utf-8")
        r = {"appId": "1245620", "encryptedSize": 1, "rawSize": 1, "checksum": SHA,
             "fileName": "1245620.makine"}
        out = self.pp.update_manifest(self.index_path, Path(self.tmp.name), [r],
                                      "https://cdn.makineceviri.org/data")
        self.assertIn("error", out)
        self.assertEqual(self.index_path.read_text(encoding="utf-8"), before)


if __name__ == "__main__":
    unittest.main()

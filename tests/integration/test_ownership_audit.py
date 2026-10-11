#!/usr/bin/env python3
"""Synthetic, source-free checks for the audit locator; NOT Qt/PJSIP tests."""

from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[2] / "scripts/phase8/audit-qt-pjsip-ownership.py"


class OwnershipInventoryTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / "src").mkdir()

    def source(self, path, text):
        dst = self.root / path
        dst.parent.mkdir(parents=True, exist_ok=True)
        dst.write_text(text, encoding="utf-8")

    def invoke(self, *args):
        return subprocess.run([sys.executable, str(SCRIPT), "--root", str(self.root), *args],
                              capture_output=True, text=True, check=False)

    def test_expected_hotspots_are_reported_with_relative_paths_and_line_numbers(self):
        self.source("src/qt/Backend.cpp", """// mock
void onRegState() {}
QMetaObject::invokeMethod(receiver, method, Qt::QueuedConnection);
Qt::DirectConnection;
std::jthread worker;
""")
        result = self.invoke()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("src/qt/Backend.cpp:2 PJSUA_CALLBACK", result.stdout)
        self.assertIn("src/qt/Backend.cpp:3 QT_QUEUED_HANDOFF", result.stdout)
        self.assertIn("src/qt/Backend.cpp:4 QT_DIRECT_OR_BLOCKING", result.stdout)
        self.assertIn("src/qt/Backend.cpp:5 WORKER_LIFECYCLE", result.stdout)
        self.assertIn("VERDICT: INCONCLUSIVE", result.stdout)
        self.assertNotIn(str(self.root), result.stdout)

    def test_content_secrets_are_never_emitted(self):
        secret = "SIP_PASSWORD=DO_NOT_PRINT_THIS"
        self.source("src/sip/Session.cpp", f"// {secret}\nonCallState(); // {secret}\n")
        result = self.invoke()
        self.assertEqual(result.returncode, 0)
        self.assertNotIn(secret, result.stdout + result.stderr)
        self.assertIn("src/sip/Session.cpp:2 PJSUA_CALLBACK", result.stdout)

    def test_ignores_symlinks_and_non_source_paths(self):
        external = self.root / "external.cpp"
        external.write_text("onRegState();\n")
        (self.root / "src" / "unsafe.cpp").symlink_to(external)
        self.source("src/notes.txt", "onRegState();\n")
        self.source("src/safe.hpp", "std::mutex mu;\n")
        result = self.invoke()
        self.assertEqual(result.returncode, 0)
        self.assertIn("src/safe.hpp:1 LOCK_OR_HANDOFF", result.stdout)
        self.assertNotIn("unsafe.cpp", result.stdout)
        self.assertNotIn("notes.txt", result.stdout)

    def test_report_is_deterministic_and_truncation_is_explicit(self):
        self.source("src/a.cpp", "std::mutex mu;\nstd::jthread t;\nonCallState();\n")
        first = self.invoke("--max-hits", "2")
        second = self.invoke("--max-hits", "2")
        self.assertEqual(first.returncode, 0)
        self.assertEqual(first.stdout, second.stdout)
        self.assertIn("TRUNCATED 1 hits", first.stdout)

    def test_empty_tree_and_bad_limit_fail_closed(self):
        empty = self.invoke()
        self.assertNotEqual(empty.returncode, 0)
        self.assertIn("BLOCKED", empty.stderr)
        self.source("src/test.cpp", "int i;\n")
        bad = self.invoke("--max-hits", "0")
        self.assertNotEqual(bad.returncode, 0)


if __name__ == "__main__":
    unittest.main()

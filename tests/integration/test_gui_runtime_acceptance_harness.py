"""Black-box checks: runtime probe detects successful, failed, and timed-out launches."""

import json
from pathlib import Path
import subprocess
import sys
from tempfile import TemporaryDirectory
import unittest

PROBE = Path(__file__).resolve().parents[2] / "scripts/phase8/gui-runtime-probe.py"


class RuntimeAcceptanceProbeTests(unittest.TestCase):
    def setUp(self):
        self.temp = TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def fake_gui(self, behavior: str) -> Path:
        binary = self.root / "voip-qt-gui"
        binary.write_text(
            "#!/usr/bin/env python3\n"
            "import sys, time\n"
            "assert sys.argv[1:] == ['--smoke-test']\n"
            f"{behavior}\n",
            encoding="utf-8",
        )
        binary.chmod(0o755)
        return binary

    def run_probe(self, binary: Path, **options):
        args = [
            sys.executable, "-B", str(PROBE), "smoke", "--binary", str(binary),
            "--log-dir", str(self.root / "logs"), "--iterations", "2",
            "--timeout", "2", "--slow-ms", "1000",
        ]
        for key, value in options.items():
            args.extend([f"--{key.replace('_', '-')}", str(value)])
        return subprocess.run(args, capture_output=True, text=True, timeout=10)

    def test_successful_exit_is_accepted(self):
        result = self.run_probe(self.fake_gui("sys.exit(0)"))
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("PASS: GUI headless startup/exit 2/2", result.stdout)
        summary = json.loads((self.root / "logs/gui-smoke-results.json").read_text())
        self.assertTrue(summary["success"])

    def test_nonzero_exit_is_rejected(self):
        result = self.run_probe(self.fake_gui("sys.exit(23)"))
        self.assertEqual(result.returncode, 1)
        self.assertIn("BAD_EXIT", result.stdout)

    def test_timeout_is_rejected(self):
        result = self.run_probe(self.fake_gui("time.sleep(5)"))
        self.assertEqual(result.returncode, 1)
        self.assertIn("TIMEOUT", result.stdout)

    def test_slow_success_is_rejected(self):
        result = self.run_probe(self.fake_gui("time.sleep(0.3); sys.exit(0)"), slow_ms=100)
        self.assertEqual(result.returncode, 1)
        self.assertIn("slow_startup", result.stderr)

    def test_missing_binary_is_rejected(self):
        result = self.run_probe(self.root / "not-here")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("missing", result.stderr)

    def test_manual_without_terminal_fails_closed(self):
        result = subprocess.run(
            [sys.executable, "-B", str(PROBE), "manual", "--binary",
             str(self.fake_gui("sys.exit(0)")), "--log-dir", str(self.root / "logs")],
            stdin=subprocess.DEVNULL, capture_output=True, text=True, timeout=10,
        )
        self.assertEqual(result.returncode, 2)
        self.assertIn("interactive terminal", result.stderr)


if __name__ == "__main__":
    unittest.main()

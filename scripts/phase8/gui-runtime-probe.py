#!/usr/bin/env python3
"""Opt-in runtime GUI probe: no SIP account, network or production shutdown assertions."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import statistics
import subprocess
import sys
import threading
import time


def positive_integer(raw: str) -> int:
    value = int(raw)
    if not 1 <= value <= 300:
        raise argparse.ArgumentTypeError("value must be between 1 and 300")
    return value


def positive_float(raw: str) -> float:
    value = float(raw)
    if not 0.5 <= value <= 60.0:
        raise argparse.ArgumentTypeError("value must be between 0.5 and 60 seconds")
    return value


def _stop_process(process: subprocess.Popen[bytes]) -> None:
    if process.poll() is not None:
        return
    process.terminate()
    try:
        process.wait(timeout=3)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=3)


def smoke(args: argparse.Namespace) -> int:
    env = dict(os.environ)
    env["QT_QPA_PLATFORM"] = "offscreen"
    env["QT_QUICK_BACKEND"] = "software"
    env["QT_LOGGING_RULES"] = "qt.qpa.*=false"
    durations_ms: list[float] = []
    observations: list[dict[str, object]] = []
    issues: list[str] = []
    stderr_path = args.log_dir / "gui-smoke-output.txt"

    with stderr_path.open("w", encoding="utf-8") as stderr_file:
        for run in range(1, args.iterations + 1):
            started = time.monotonic()
            process: subprocess.Popen[bytes] | None = None
            status = "PASS"
            code: int | None = None
            try:
                process = subprocess.Popen(
                    [str(args.binary), "--smoke-test"],
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    env=env,
                    start_new_session=True,
                )
                try:
                    output, errors = process.communicate(timeout=args.timeout)
                except subprocess.TimeoutExpired:
                    status = "TIMEOUT"
                    _stop_process(process)
                    output, errors = process.communicate()
                code = process.returncode
                if status == "PASS" and code != 0:
                    status = "BAD_EXIT"
                if output or errors:
                    stderr_file.write(f"===== GUI RUN {run} =====\n")
                    stderr_file.write(output.decode("utf-8", errors="replace")[:16384])
                    stderr_file.write(errors.decode("utf-8", errors="replace")[:16384])
                    stderr_file.write("\n")
            except OSError as exc:
                status = "LAUNCH_ERROR"
                stderr_file.write(f"Run {run}: {exc}\n")
            finally:
                if process is not None:
                    _stop_process(process)
            elapsed = round((time.monotonic() - started) * 1000, 2)
            durations_ms.append(elapsed)
            observations.append({
                "run": run, "status": status, "exit_code": code,
                "duration_ms": elapsed,
            })
            if status != "PASS":
                issues.append(f"run={run} status={status} exit={code}")
            elif elapsed > args.slow_ms:
                issues.append(f"run={run} slow_startup={elapsed}ms>{args.slow_ms}ms")
            print(f"GUI smoke {run}/{args.iterations}: {status}, {elapsed:.0f}ms", flush=True)

    samples = sorted(durations_ms)
    p95_index = max(0, (len(samples) * 95 + 99) // 100 - 1)
    summary = {
        "mode": "headless_gui_smoke",
        "iterations": args.iterations,
        "success": len(issues) == 0,
        "threshold_ms": args.slow_ms,
        "median_ms": round(statistics.median(samples), 2),
        "p95_ms": samples[p95_index],
        "max_ms": max(samples),
        "issues": issues,
        "measurements": observations,
        "limits": "QML launch plus 100ms event loop, not FPS, SIP or long UI responsiveness",
    }
    (args.log_dir / "gui-smoke-results.json").write_text(
        json.dumps(summary, indent=2) + "\n", encoding="utf-8"
    )
    print(
        f"GUI startup median={summary['median_ms']}ms "
        f"p95={summary['p95_ms']}ms max={summary['max_ms']}ms"
    )
    if issues:
        print("FAIL: GUI startup smoke: " + "; ".join(issues[:8]), file=sys.stderr)
        return 1
    print(f"PASS: GUI headless startup/exit {args.iterations}/{args.iterations}")
    return 0


def _linux_process_sample(pid: int) -> dict[str, int] | None:
    try:
        stat = (Path("/proc") / str(pid) / "status").read_text(encoding="utf-8")
        fd_count = len(list((Path("/proc") / str(pid) / "fd").iterdir()))
    except (FileNotFoundError, PermissionError, ProcessLookupError):
        return None
    output: dict[str, int] = {"fds": fd_count}
    for line in stat.splitlines():
        if line.startswith("VmRSS:"):
            output["rss_kib"] = int(line.split()[1])
        elif line.startswith("Threads:"):
            output["threads"] = int(line.split()[1])
    return output if "rss_kib" in output and "threads" in output else None


def manual(args: argparse.Namespace) -> int:
    if not sys.stdin.isatty():
        print("ABORT: manual GUI acceptance requires an interactive terminal", file=sys.stderr)
        return 2
    if os.environ.get("QT_QPA_PLATFORM", "") in {"offscreen", "minimal"}:
        print("ABORT: unset QT_QPA_PLATFORM=offscreen/minimal before manual GUI test", file=sys.stderr)
        return 2
    if not (os.environ.get("DISPLAY") or os.environ.get("WAYLAND_DISPLAY")):
        print("ABORT: no graphical DISPLAY or WAYLAND_DISPLAY", file=sys.stderr)
        return 2
    if sys.platform != "linux":
        print("ABORT: /proc resource monitoring requires Linux", file=sys.stderr)
        return 2

    # SIP is deliberately NOT enabled: no credentials, no live calls.
    env = dict(os.environ)
    env.pop("QT_QUICK_BACKEND", None)
    monitor_path = args.log_dir / "gui-manual-samples.jsonl"
    run_log = args.log_dir / "gui-manual-output.txt"
    start = time.monotonic()
    result: dict[str, object] = {"mode": "manual_gui_soak", "accepted": False}
    samples: list[dict[str, float | int]] = []
    stop = threading.Event()

    with run_log.open("wb") as output:
        process = subprocess.Popen(
            [str(args.binary)], stdout=output, stderr=subprocess.STDOUT, env=env
        )
        print(f"GUI PID={process.pid}; resource samples: {monitor_path}", flush=True)
        print("Check: window paints; keypad 0-9/*/#; Delete/Clear; type destination; scroll/resize.", flush=True)
        print("Check: Register/Call/Hang up remain disabled in preview mode; no freeze/lag/crash.", flush=True)
        print("Interact now and answer the prompt after checking the listed UI behavior.", flush=True)
        print("After acceptance, keep the GUI open until the harness explicitly asks you to close it.", flush=True)

        def sampler() -> None:
            with monitor_path.open("w", encoding="utf-8") as file:
                while not stop.wait(0.5):
                    sample = _linux_process_sample(process.pid)
                    if sample is None:
                        continue
                    sample["elapsed_seconds"] = round(time.monotonic() - start, 2)
                    samples.append(sample)
                    file.write(json.dumps(sample) + "\n")
                    file.flush()

        thread = threading.Thread(target=sampler, daemon=True)
        thread.start()
        try:
            answer = input("After interacting: did EVERY listed UI check pass? [yes/no]: ")
            if answer.strip().lower() != "yes":
                print("FAIL: manual UI observation not accepted")
                return 1
            remaining = args.observe_seconds - (time.monotonic() - start)
            if remaining > 0:
                print(f"Continue using the GUI for {remaining:.0f}s before closure check...", flush=True)
                end = time.monotonic() + remaining
                while time.monotonic() < end:
                    if process.poll() is not None:
                        print("FAIL: GUI exited before minimum observation interval")
                        return 1
                    time.sleep(min(0.25, end - time.monotonic()))
            if process.poll() is not None:
                print("FAIL: GUI exited before requested normal window close")
                return 1
            print(f"Close the GUI using the window X (not kill/CTRL+C), within {args.close_timeout}s.", flush=True)
            try:
                code = process.wait(timeout=args.close_timeout)
            except subprocess.TimeoutExpired:
                print("FAIL: GUI failed to close within timeout")
                return 1
            if code != 0:
                print(f"FAIL: GUI exit={code} (expected normal exit=0)")
                return 1
            if len(samples) < 3:
                print("FAIL: insufficient /proc resource observations")
                return 1
            result = {
                "mode": "manual_gui_soak", "accepted": True,
                "observation_seconds": round(time.monotonic() - start, 2),
                "samples": len(samples),
                "rss_first_kib": samples[0]["rss_kib"],
                "rss_last_kib": samples[-1]["rss_kib"],
                "rss_peak_kib": max(s["rss_kib"] for s in samples),
                "fd_min": min(s["fds"] for s in samples),
                "fd_max": max(s["fds"] for s in samples),
                "thread_min": min(s["threads"] for s in samples),
                "thread_max": max(s["threads"] for s in samples),
                "exit_code": code,
                "limits": "Human UI observation, no SIP authentication, no production teardown claim",
            }
            print("PASS: manual GUI usability and normal window-close check")
            print(
                f"Observed RSS first/last/peak KiB: {result['rss_first_kib']}/"
                f"{result['rss_last_kib']}/{result['rss_peak_kib']}; "
                f"FD max={result['fd_max']}; threads max={result['thread_max']}"
            )
            return 0
        except (KeyboardInterrupt, EOFError):
            print("FAIL: manual GUI acceptance interrupted")
            return 1
        finally:
            stop.set()
            thread.join(timeout=2)
            _stop_process(process)
            (args.log_dir / "gui-manual-results.json").write_text(
                json.dumps(result, indent=2) + "\n", encoding="utf-8"
            )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("smoke", "manual"))
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--log-dir", type=Path, required=True)
    parser.add_argument("--iterations", type=positive_integer, default=40)
    parser.add_argument("--timeout", type=positive_float, default=8.0)
    parser.add_argument("--slow-ms", type=int, default=4_000)
    parser.add_argument("--observe-seconds", type=positive_integer, default=90)
    parser.add_argument("--close-timeout", type=positive_integer, default=45)
    args = parser.parse_args()
    if not args.binary.is_file() or not os.access(args.binary, os.X_OK):
        parser.error(f"GUI executable missing or not executable: {args.binary}")
    if not 100 <= args.slow_ms <= 30_000:
        parser.error("--slow-ms must be 100..30000")
    args.log_dir.mkdir(parents=True, exist_ok=True)
    return smoke(args) if args.mode == "smoke" else manual(args)


if __name__ == "__main__":
    raise SystemExit(main())

"""Check the compiled replay CLI and optionally save the three demo reports."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess


ROOT = Path(__file__).resolve().parents[1]
DATASET = ROOT / "datasets" / "heartbeat_v1"
TRACE = DATASET / "heartbeat.csv"
FIXTURES = Path(__file__).resolve().parent / "fixtures"
OUTAGE_EVENTS = [{"time_ms": 800, "kind": "timeout"}, {"time_ms": 1100, "kind": "recovered"}]


def invoke(binary, trace, scenario, subject="reference"):
    return subprocess.run(
        [str(binary), str(trace), str(scenario), subject],
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=10,
        check=False,
    )


def expect_report(run, code, **expected_fields):
    assert run.returncode == code, (run.args, run.returncode, run.stdout, run.stderr)
    assert not run.stderr, run.stderr
    report = json.loads(run.stdout)
    assert report["report_version"] == 1
    assert report["passed"] == (code == 0)
    for key, expected_value in expected_fields.items():
        assert report[key] == expected_value, (key, expected_value, report[key])
    return {"exit_code": run.returncode, "report": report}


def verify_demo(binary):
    healthy = expect_report(
        invoke(binary, TRACE, DATASET / "healthy.case"), 0,
        scenario_id="healthy", subject="reference", delivered_heartbeats=16, dropped_heartbeats=0,
        expected_events=[], observed_events=[], final_state="online",
    )
    dropout_run = invoke(binary, TRACE, DATASET / "dropout.case")
    dropout = expect_report(
        dropout_run, 0, scenario_id="dropout", subject="reference", end_ms=1500, timeout_ms=300,
        delivered_heartbeats=11, dropped_heartbeats=5, expected_events=OUTAGE_EVENTS,
        observed_events=OUTAGE_EVENTS, final_state="online",
    )
    missed_alert = expect_report(
        invoke(binary, TRACE, DATASET / "dropout.case", "ignore_timeout"), 1,
        scenario_id="dropout", subject="ignore_timeout", delivered_heartbeats=11, dropped_heartbeats=5,
        expected_events=OUTAGE_EVENTS, observed_events=[], final_state="online",
    )
    repeat_run = invoke(binary, TRACE, DATASET / "dropout.case")
    assert (repeat_run.returncode, repeat_run.stdout, repeat_run.stderr) == (
        dropout_run.returncode, dropout_run.stdout, dropout_run.stderr,
    ), "A repeated run changed its result"
    return {"healthy": healthy, "dropout": dropout, "missed_alert": missed_alert}


def expect_input_error(binary, trace, scenario, message):
    run = invoke(binary, trace, scenario)
    assert run.returncode == 2 and not run.stdout and message in run.stderr, (
        run.args, run.returncode, run.stdout, run.stderr,
    )


def verify_edges(binary):
    startup = FIXTURES / "startup_timeout.case"
    empty = FIXTURES / "empty.csv"
    # Exact-deadline delivery wins the tie, including duplicated timestamps.
    expect_report(
        invoke(binary, FIXTURES / "deadline_tie.csv", FIXTURES / "deadline_tie.case"),
        0, delivered_heartbeats=3, dropped_heartbeats=0,
        observed_events=[{"time_ms": 600, "kind": "timeout"}], final_state="timed_out",
    )
    expect_report(
        invoke(binary, empty, startup), 0, delivered_heartbeats=0,
        observed_events=[{"time_ms": 300, "kind": "timeout"}], final_state="timed_out",
    )
    expect_input_error(binary, FIXTURES / "decreasing.csv", startup, "nondecreasing")
    expect_input_error(binary, FIXTURES / "malformed.csv", startup, "Invalid millisecond value")
    expect_input_error(binary, FIXTURES / "beyond_end.csv", startup, "after end_ms")
    expect_input_error(
        binary, empty, FIXTURES / "unsupported_version.case", "Unsupported case schema_version"
    )


def save_evidence(report_dir, demo_results):
    report_dir.mkdir(parents=True, exist_ok=True)
    for case_name, payload in demo_results.items():
        (report_dir / f"{case_name}.json").write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    inputs = ("heartbeat.csv", "healthy.case", "dropout.case")
    summary = {
        "dataset": "heartbeat_v1",
        "inputs_sha256": {name: hashlib.sha256((DATASET / name).read_bytes()).hexdigest() for name in inputs},
        "demo_exit_codes": {name: payload["exit_code"] for name, payload in demo_results.items()},
        "repeat_dropout_report_identical": True,
        "edge_checks_passed": [
            "deadline_tie_duplicate_timestamps_and_end_horizon",
            "empty_trace_startup_timeout",
            "reject_decreasing_trace",
            "reject_malformed_timestamp",
            "reject_record_after_end",
            "reject_unknown_schema_version",
        ],
    }
    (report_dir / "summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")


def main():
    if not __debug__:
        raise SystemExit("Run verification with Python assertions enabled.")
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--report-dir", type=Path)
    args = parser.parse_args()
    binary = args.binary.resolve()
    demo_results = verify_demo(binary)
    verify_edges(binary)
    if args.report_dir is not None:
        save_evidence(args.report_dir, demo_results)
    print("PASS: healthy (0), dropout (0), missed alert detected (1)")
    print("PASS: deterministic repeat and six edge checks")


if __name__ == "__main__":
    main()

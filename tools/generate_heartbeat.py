"""Generate the fixed, synthetic heartbeat_v1 trace."""

import argparse
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
TRACE = ROOT / "datasets" / "heartbeat_v1" / "heartbeat.csv"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="Verify the existing CSV without writing it.")
    args = parser.parse_args()
    content = "time_ms,event\n" + "".join(f"{time_ms},heartbeat\n" for time_ms in range(0, 1501, 100))
    expected_bytes = content.encode("ascii")
    if args.check:
        if not TRACE.is_file() or TRACE.read_bytes() != expected_bytes:
            parser.exit(1, "heartbeat.csv differs from the generated v1 fixture\n")
        print("Verified heartbeat_v1: 16 records, 0 to 1500 ms")
    else:
        TRACE.write_bytes(expected_bytes)
        print("Generated datasets/heartbeat_v1/heartbeat.csv")


if __name__ == "__main__":
    main()

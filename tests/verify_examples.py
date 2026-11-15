"""Exercise DLL-linked client/server programs and TCP wire boundaries on loopback."""

import argparse
from contextlib import contextmanager
import json
from pathlib import Path
import queue
import re
import socket
import struct
import subprocess
import threading


def executable(directory, name):
    windows_path = directory / (name + ".exe")
    return windows_path if windows_path.is_file() else directory / name


@contextmanager
def server(directory, protocol):
    with subprocess.Popen(
        [str(executable(directory, protocol + "_echo_server")), "0"],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding="utf-8",
    ) as process:
        try:
            ready = queue.Queue()
            threading.Thread(target=lambda: ready.put(process.stdout.readline()), daemon=True).start()
            line = ready.get(timeout=10).strip()
            match = re.fullmatch(r"Listening on 127\.0\.0\.1:(\d+)", line)
            if match is None:
                raise RuntimeError(f"Server did not report its endpoint: {line!r}")
            yield process, int(match.group(1))
        finally:
            if process.poll() is None:
                process.kill()
            process.communicate(timeout=5)


def finish(process, expected_code):
    stdout, stderr = process.communicate(timeout=8)
    if process.returncode != expected_code:
        raise AssertionError((process.returncode, expected_code, stdout, stderr))
    if expected_code == 0 and stderr:
        raise AssertionError(stderr)


def receive_exact(stream, size):
    result = bytearray()
    while len(result) < size:
        chunk = stream.recv(size - len(result))
        if not chunk:
            raise AssertionError("Peer closed before completing the echoed frame")
        result.extend(chunk)
    return bytes(result)


def run(directory):
    passed = []
    for protocol in ("udp", "tcp"):
        with server(directory, protocol) as (process, port):
            client = subprocess.run(
                [str(executable(directory, protocol + "_echo_client")), str(port)],
                capture_output=True, text=True, encoding="utf-8", timeout=10, check=False,
            )
            if client.returncode != 0 or client.stderr:
                raise AssertionError((protocol, client.returncode, client.stdout, client.stderr))
            finish(process, 0)
        passed.append(protocol + "_separate_process_echo")

    with server(directory, "tcp") as (process, port):
        with socket.create_connection(("127.0.0.1", port), timeout=3) as stream:
            payload = b"\x00USV\xfftelemetry\x00"
            frame = struct.pack("!I", len(payload)) + payload
            # Distinct writes exercise the byte-stream contract. The OS may coalesce them.
            for byte in frame:
                stream.sendall(bytes([byte]))
            length = struct.unpack("!I", receive_exact(stream, 4))[0]
            if length != len(payload) or receive_exact(stream, length) != payload:
                raise AssertionError("Fragmented binary frame did not round-trip")
        finish(process, 0)
    passed.append("tcp_binary_frame_sent_in_chunks")

    for name, payload in (
        ("tcp_truncated_frame", struct.pack("!I", 10) + b"abc"),
        ("tcp_oversized_frame", struct.pack("!I", 1024 * 1024 + 1)),
    ):
        with server(directory, "tcp") as (process, port):
            with socket.create_connection(("127.0.0.1", port), timeout=3) as stream:
                stream.sendall(payload)
                stream.shutdown(socket.SHUT_WR)
                finish(process, 1)
        passed.append(name)
    return {"passed": passed, "transport": "actual IPv4 loopback sockets", "all_passed": True}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bin-dir", required=True, type=Path)
    parser.add_argument("--report-file", type=Path)
    args = parser.parse_args()
    report = run(args.bin_dir.resolve())
    text = json.dumps(report, indent=2) + "\n"
    if args.report_file:
        args.report_file.parent.mkdir(parents=True, exist_ok=True)
        args.report_file.write_text(text, encoding="utf-8")
    print(text, end="")


if __name__ == "__main__":
    main()

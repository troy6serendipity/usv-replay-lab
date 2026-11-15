# Changelog

## 0.2.0 — development version

- Add an installable C++17 UDP/TCP shared library with a CMake package and an independent consumer example.
- Add UDP and TCP client/server examples and loopback, wire-format, and installed-consumer regression tests.
- Validate four test suites locally on Windows/MSVC and WSL Ubuntu 26.04.1/GCC. See [communication validation](docs/communication_validation.md) for the exact coverage and limitations.

## 0.1.0 — local prototype

- Add deterministic synthetic heartbeat replay, a fixed dropout scenario, and JSON results with process exit codes.
- Add a deliberately faulty monitor for demonstrating a failed timeout assertion and tests for input and timing boundaries.

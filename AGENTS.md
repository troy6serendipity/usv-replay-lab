# Project guidance

- Apply the C++ style in [CONTRIBUTING.md](CONTRIBUTING.md) to every project-owned C++ file. Run the project formatter check and review the naming, include guards and namespace qualifications before delivery.
- Build with CMake, run CTest, and check the relevant generated reports when changing replay behavior or input contracts.
- Keep deterministic offline fixtures synthetic and distinguish their results from hardware or real network measurements.

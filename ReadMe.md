# OpENer-Enhanced

Maintained and hardened EtherNet/IP adapter stack based on OpENer, with integrated test tooling and embedded platform support.

---

## Overview

OpENer-Enhanced is a continuation of the original OpENer adapter stack focused on improving robustness, testability, and practical deployability.

Key focus areas include:

- Parser hardening and safer error handling
- Expanded regression coverage for malformed message paths
- Embedded and platform integration support
- Repeatable runtime validation for cyclic I/O behavior

The project maintains alignment with OpENer architecture while addressing real-world reliability concerns.

---

## Accuracy Notes

This repository contains both completed improvements and ongoing integration work.

All claims in this README are based on:

- Observed behavior in local test runs
- Verified code changes present in this repository

They should not be interpreted as guarantees across all hardware, operating systems, or network conditions.

---

## Key Improvements

### Protocol and Parser Hardening

- Additional bounds checking for malformed CPF, EPath, and Message Router requests
- Elimination of shared CPF scratch-state in send paths (use of local buffers)
- Improved error handling and status propagation
- Targeted regression tests for malformed UCMM, EPath, and GetAttributeList scenarios

### Runtime Test Tooling

- Python stress/fuzz client: `fuzz/scripts/aggressive_local_client.py`
- C++ cyclic benchmark client: `fuzz/cpp/plc_rate_client.cpp`
- C++ assembly monitor client: `fuzz/cpp/assembly_monitor.cpp`

### Port and Integration Work

- ESP32 integration scaffolding
- Teknic ClearCore integration scaffolding
- Platform-specific notes and documentation included per port

---

## Validation Summary (Observed)

Recent lab testing on a Win32 target demonstrated:

- Sustained cyclic explicit messaging at request rates down to 1 ms
- Zero observed read/write failures, timeouts, or connection resets during test runs
- Sub-millisecond request/response service latency percentiles

Important context:

- Observed timing jitter is primarily due to host OS scheduling
- Results depend on hardware, OS, NIC/driver stack, and network conditions
- Included tools should be used to validate target deployment environments

---

## Feature Maturity

| Area | Current Status |
|---|---|
| Core adapter behavior | Mature with additional hardening |
| Malformed request handling | Expanded regression coverage |
| Win32 runtime validation | Actively exercised |
| ESP32 port | Scaffolded; hardware validation required |
| ClearCore port | Scaffolded; hardware validation required |
| CIP Security | Integrated; validation depth varies |
| LLDP | Integrated; validation depth varies |
| CIP File Object | Present and exercised in builds/tests |

---

## Related Work

This repository is part of a broader effort to make EtherNet/IP practical on low-cost embedded systems.

Related projects include:

- FusionCoreEnIP - general-purpose embedded EtherNet/IP framework
- EthernetIP-Scale - load cell / scale integration
- EIP_GROUND_TRUTH - embedded device providing absolute orientation data (gyro-based) for validation and testing
- OpENer_EnIP_KC868A16 - digital I/O controller implementation

These projects demonstrate how real-world signals and device behaviors can be exposed directly to EtherNet/IP-based control systems using embedded platforms.

---

## Getting Started

### Build and Test Tooling

Use the existing CMake build flow for the stack.

Runtime validation tools are located in:

- `fuzz/cpp` - C++ test clients
- `fuzz/scripts` - Python stress/fuzz tools

### Example Build (Windows)

```powershell
python -m cmake -S "D:\UpdatedOpeNer\fuzz\cpp" -B "D:\UpdatedOpeNer\fuzz\cpp\build" -G "Visual Studio 17 2022" -A x64
python -m cmake --build "D:\UpdatedOpeNer\fuzz\cpp\build" --config Release
```

---

## Relationship to Upstream OpENer

This project is based on upstream OpENer and keeps core architecture alignment where practical, while prioritizing:

- robustness and correctness fixes
- additional tests for historical failure modes
- practical integration support

---

## Licensing

This repository includes original OpENer-derived code and additional modifications.

### Original OpENer-derived Code

Governed by the OpENer distribution license in:

- `license.txt`

### Additional Modifications

Additional code and modifications in this fork are released under MIT terms as documented in this repository.

Users must comply with all applicable license terms for the files they use.

---

## Contributing

Contributions are welcome for:

- bug fixes and protocol correctness
- test coverage (unit/integration/stress)
- platform port validation
- documentation improvements

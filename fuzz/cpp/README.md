# C++ PLC Rate Client

`plc_rate_client.cpp` is a small Windows benchmark client for testing OpENer at deterministic PLC-like scan rates.

`assembly_monitor.cpp` is a simple Windows monitor that reads Assembly instance `150` (attr `3` by default) at 1 ms intervals and prints payload changes with timestamps.

It opens one TCP EtherNet/IP session per worker and performs cyclic:
- `SetAttributeSingle` write to Assembly instance (default `150`, attr `3`)
- `GetAttributeSingle` read verify every N cycles

It reports:
- expected vs achieved cycles/sec
- cycle attempts/completions
- deadline misses
- write/read success/failure counts
- p50/p95/p99 write/read latency

## Build (CMake, recommended)

```powershell
python -m cmake -S "D:\UpdatedOpeNer\fuzz\cpp" -B "D:\UpdatedOpeNer\fuzz\cpp\build" -G "Visual Studio 17 2022" -A x64
python -m cmake --build "D:\UpdatedOpeNer\fuzz\cpp\build" --config Release
```

Executable:

```text
D:\UpdatedOpeNer\fuzz\cpp\build\Release\plc_rate_client.exe
D:\UpdatedOpeNer\fuzz\cpp\build\Release\assembly_monitor.exe
```

## Build (direct `cl`, optional)

From a Developer PowerShell where `cl` is on `PATH`:

```powershell
cl /std:c++20 /O2 /EHsc /W4 /nologo "D:\UpdatedOpeNer\fuzz\cpp\plc_rate_client.cpp" /Fe:"D:\UpdatedOpeNer\fuzz\cpp\plc_rate_client.exe"
```

## Run Examples

Default rate sweep (`50,20,10,5` ms):

```powershell
"D:\UpdatedOpeNer\fuzz\cpp\plc_rate_client.exe" --host 172.16.82.199 --workers 1 --duration-s 30
```

Custom rates:

```powershell
"D:\UpdatedOpeNer\fuzz\cpp\plc_rate_client.exe" --host 172.16.82.199 --workers 2 --duration-s 20 --rates-ms 50,25,10,5
```

Lower readback overhead at high rate:

```powershell
"D:\UpdatedOpeNer\fuzz\cpp\plc_rate_client.exe" --host 172.16.82.199 --workers 1 --duration-s 30 --rates-ms 10,5 --verify-read-every 5
```

Assembly monitor at 1 ms (prints on change):

```powershell
& "D:\UpdatedOpeNer\fuzz\cpp\build\Release\assembly_monitor.exe" --host 172.16.82.199 --interval-ms 1
```

Print every sample:

```powershell
& "D:\UpdatedOpeNer\fuzz\cpp\build\Release\assembly_monitor.exe" --host 172.16.82.199 --interval-ms 1 --print-all
```

## Key Options

- `--host <ip-or-hostname>`
- `--port <port>`
- `--workers <n>`
- `--duration-s <seconds>`
- `--cycles <n>` (overrides duration per rate)
- `--rates-ms <csv>` (minimum supported rate is 1 ms)
- `--timeout-ms <ms>`
- `--assembly-input <id>`
- `--assembly-output <id>`
- `--assembly-attr <id>`
- `--write-size <bytes>`
- `--verify-read-every <n>`
- `--hard-miss-ms <ms>` (deadline miss threshold, default `0.5`)
- `--timer-resolution-ms <ms>` (Windows timer request, default `1`)
- `--spin-threshold-us <us>` (busy-spin near deadline, default `300`)
- `--no-spin-wait` (disable busy-spin near deadline)
- `--expect-mirror`

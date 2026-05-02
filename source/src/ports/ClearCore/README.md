# ClearCore Port Integration Notes

This directory contains a `ClearCore` platform port for OpENer with a generic
sample assembly application under `sample_application/`.

## What this port expects

The code in this folder is intended for a ClearCore/lwIP-style embedded runtime.
It depends on:

- `lwip` networking headers (`lwip/sockets.h`, `lwip/netif.h`, etc.)
- an embedded runtime that provides timer and socket support expected by OpENer

If you build this platform in a desktop-only environment without ClearCore/lwIP
headers, missing include errors are expected.

## Generic sample assembly behavior

The generic sample assembly implementation is in:

- `sample_application/sampleapplication.c`

It exposes standard demo assemblies:

- Input: `100`
- Output: `150`
- Config: `151`
- Input-only heartbeat: `152`
- Listen-only heartbeat: `153`
- Explicit messaging assembly: `154`

Current behavior mirrors output data to input data for demo validation.

## Integrating into a ClearCore project

Recommended approach is to keep OpENer as a reusable stack layer and place
device-specific machine logic in your own application modules.

1. Add OpENer sources to your embedded build.
2. Ensure lwIP/socket headers and libraries are available.
3. Initialize network interface data before starting OpENer.
4. Call platform startup (`opener_init(...)` path) from your application bring-up.
5. Extend `sample_application/sampleapplication.c` with your production assemblies.

## CMake platform selection in this repo

For repository-level CMake selection, use:

```sh
cmake -S source -B Build-ClearCore -DOpENer_PLATFORM=ClearCore
```

This validates platform wiring in this repository but does not replace your
target ClearCore SDK/toolchain build.

## Important customization points

- `opener.c`: startup flow, stack bring-up, platform-specific init.
- `networkconfig.c`: MAC/IP/hostname handling and network configuration behavior.
- `sample_application/opener_user_conf.h`: feature flags, limits, connection counts.
- `sample_application/sampleapplication.c`: assembly map and application behavior.

## Notes on identity/device data

Identity defaults still come from the normal OpENer configuration path
(`devicedata.h.in` generated output and runtime setters such as
`SetDeviceSerialNumber`). Update these in your startup/application logic for
device-specific production values.

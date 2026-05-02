# ESP32 Port Integration Notes

This directory contains an `ESP32` platform port for OpENer with a generic sample
assembly application under `sample_application/`.

## What this port expects

The code in this folder expects to be built in an ESP-IDF environment and depends
on the following ESP-IDF components/headers:

- `lwip` (`lwip/sockets.h`, `lwip/netif.h`, `lwip/ip4_addr.h`, etc.)
- `freertos` (`freertos/task.h`, `freertos/semphr.h`)
- `esp_system` / random (`esp_random.h`)
- `esp_timer` (`esp_timer.h`)

If you try to build this platform on desktop MSVC/GCC without ESP-IDF include
paths, you will get missing `lwip/...` errors by design.

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

Current behavior mirrors output data to input data on receive for the demo path.

## Integrating into an ESP-IDF project

Recommended approach is to treat OpENer as an ESP-IDF component and keep your
board/application logic outside this port layer.

1. Place OpENer source in your project components tree (for example as
   `components/opener`).
2. Ensure your ESP-IDF project links `lwip` and `freertos` (default in ESP-IDF).
3. Include and call `opener_init(struct netif *netif)` from your network bring-up
   path after your interface is up.
4. Keep the generic sample app as-is, or extend `sample_application.c` for your
   product-specific assemblies.

## Minimal runtime call flow

From your application code:

1. Bring up Ethernet and obtain a valid `struct netif *`.
2. Call `opener_init(netif)`.
3. OpENer starts its own FreeRTOS task via `xTaskCreatePinnedToCore`.
4. Allow normal FreeRTOS/lwIP scheduling to run; OpENer processes cyclic traffic
   in its internal task loop.

## CMake platform selection in this repo

For repository-level CMake selection, use:

```sh
cmake -S source -B Build-ESP32 -DOpENer_PLATFORM=ESP32
```

This verifies platform wiring in this repository, but does not replace an ESP-IDF
build environment.

## Important customization points

- `opener.c`: task creation strategy, init flow, connection ID/randomization.
- `networkconfig.c`: host name, MAC/IP extraction from lwIP `netif`.
- `sample_application/opener_user_conf.h`: stack/session/object sizing and feature flags.
- `sample_application/sampleapplication.c`: assembly layout and application logic.

## Notes on identity/device data

Device identity defaults still come from the standard OpENer configuration flow
(`devicedata.h.in` generated path and runtime setters such as `SetDeviceSerialNumber`).
Adjust these in your application startup path as needed for production.

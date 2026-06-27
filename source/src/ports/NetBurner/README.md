# NetBurner Port Integration Notes

This directory defines a `NetBurner` OpENer platform target for ARM SAME70
projects using NetBurner SDK networking.

## Current implementation strategy

To keep the initial bring-up simple, the `NetBurner` target reuses the existing
`ClearCore` lwIP-based platform source files while enabling a dedicated
`NETBURNER` build define.

This gives you a working baseline quickly and keeps subsequent NetBurner
specialization isolated to this platform target.

## Configure with CMake

```sh
cmake -S source -B Build-NetBurner -DOpENer_PLATFORM=NetBurner -DOPENER_NETBURNER_LWIP_INCLUDE_DIR=<path-to-lwip-headers>
```

If you only need a configure-time wiring check (without full SDK include paths),
add:

```sh
-DOPENER_EMBEDDED_PORT_DRY_RUN=ON
```

## Next steps for native NetBurner behavior

1. Replace shared `ClearCore` sources with dedicated `NetBurner` copies.
2. Update socket and interface accessors to match NetBurner APIs.
3. Keep sample application logic unchanged unless product-specific assemblies are required.

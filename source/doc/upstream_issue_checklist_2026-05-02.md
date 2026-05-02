# Upstream Issue-to-Code Checklist (2026-05-02)

Scope: Verify current repository state against open issues in the upstream
OpENer tracker.

Reference source:
- https://github.com/EIPStackGroup/OpENer/issues

Status legend:
- **Present**: mitigation/fix clearly visible in code.
- **Partial**: hardening exists, but strict one-to-one parity with upstream fix is not fully proven.
- **Not Found**: no clear mitigation found in current tree.
- **N/A (Process/Test)**: requires test execution or external process, not static code check.

## Checklist

| Issue | Title (upstream) | Status | Evidence in this repo | Notes |
|---|---|---|---|---|
| #567 | Malformed CPF and Get_Attribute_List request leads to pointer corruption / DoS | **Present** | `source/src/enet_encap/cpf.c` has `CPF_REQUIRE_BYTES(...)` bounds guards and strict CPF item-count enforcement; `source/src/cip/cipcommon.c` validates `request_data_size`, count-vs-buffer consistency, and per-attribute ID availability in `GetAttributeList(...)`; regression tests now cover both unit vectors (`source/tests/cip/cipcommontests.cpp`) and integration vectors (`source/tests/enet_encap/encaptest.cpp`: malformed CPF through `SendRRData`, malformed `GetAttributeList` payload through encapsulation path) | Static hardening plus integration test coverage now exercise both parser layers in the reported failure chain. Keep upstream PoC replay as optional defense-in-depth confirmation. |
| #566 | ASan stack-use-after-return in TCP SendRRData path via CPF payload pointer lifetime | **Present** | Global CPF scratch usage has been removed: `source/src/enet_encap/cpf.c` now uses per-call local `CipCommonPacketFormatData` in both `NotifyCommonPacketFormat(...)` and `NotifyConnectedCommonPacketFormat(...)`; `source/src/cip/cipioconnection.c` and `source/src/cip/cipconnectionmanager.c` no longer depend on `g_common_packet_format_data_item`; `source/src/enet_encap/cpf.h` no longer exports global CPF claim/release APIs | Static review now shows no shared CPF scratch state remaining in the tree. Keep fuzz/ASan replay as defense-in-depth validation. |
| #565 | Incorrect Access Control on Session Handles | **Present** | `source/src/enet_encap/encap.c` in `CheckRegisteredSessions(...)`: comment and logic require session handle ownership by requesting socket; invalid-handle checks in `CloseSessionBySessionHandle(...)` | Session/socket binding check is explicit. |
| #564 | Stack OOB Read in ForwardOpen Parsing | **Present** | `source/src/cip/cipconnectionmanager.c` in `ForwardOpenRoutine(...)` validates fixed header length using `g_kForwardOpenHeaderLength` / `g_kLargeForwardOpenHeaderLength` before parsing | Guard is explicit and early. |
| #563 | Integer overflow/truncation leading to buffer overflow in message router | **Present** | `source/src/cip/cipmessagerouter.c` validates declared EPATH size before decode, passes bounded path length into decoder, and requires decoded bytes to exactly match declared path size before computing `request_data_size`; runtime tests now cover direct and encapsulated malformed path-size vectors: `source/tests/cip/cipcommontests.cpp` (`CreateMessageRouterRequestRejectsOversizedDeclaredPath`, `CreateMessageRouterRequestRejectsTruncatedLogicalSegment`, `NotifyMessageRouterReturnsPathSizeInvalidForMalformedDeclaredPath`) and `source/tests/enet_encap/encaptest.cpp` (`SendRRDataPropagatesPathSizeInvalidStatus`) | Static hardening plus unit/integration regression tests now exercise the truncation/overflow boundary in the message router path. Keep upstream PoC replay optional for extra assurance. |
| #562 | DoS via single-connection slowloris attack in TCP network handler | **Present** | `source/src/ports/generic_networkhandler.c`: non-blocking accepted sockets, bounded reassembly, overflow-risk close path, and `ToSocketDataLength(...)` size checks | Mitigation is visible in the shared network handler used by platforms. |
| #558 | Potential OOB read in CPF parsing logic | **Present** | `source/src/enet_encap/cpf.c`: `CPF_REQUIRE_BYTES(...)` macro and repeated boundary checks in `CreateCommonPacketFormatStructure(...)` | Clear parser hardening present. |
| #557 | OOB read in CIP padded EPath decoder | **Present** | `source/src/cip/cipcommon.c` `DecodePaddedEPath(...)` now enforces caller-provided byte bounds, validates per-segment minimum bytes before every read, and rejects non-exact path consumption; `source/src/cip/cipmessagerouter.c` supplies declared path bytes as decode limit; regression tests added in `source/tests/cip/cipcommontests.cpp` (`DecodePaddedEPathRejectsTrailingJunkInsideDeclaredPath`, plus malformed 32-bit segment coverage) and passed in `OpENer_Tests` | Static and runtime checks now both cover this decoder class. Keep fuzz/PoC replay as defense-in-depth runtime validation. |
| #556 | Windows link error case mismatch `getMicroSeconds` / `GetMicroSeconds` | **Present** | All active port handlers now use `GetMicroSeconds` (`source/src/ports/WIN32/networkhandler.c`, `.../MINGW/networkhandler.c`, `.../POSIX/networkhandler.c`, `.../ESP32/networkhandler.c`, `.../ClearCore/networkhandler.c`) | MINGW alignment was applied during this verification pass. |
| #559 | ODVA CT22 conformance testing | **N/A (Process/Test)** | No static code claim | Requires running CT test suite and capturing results. |
| #555 | ODVA CT Test | **N/A (Process/Test)** | No static code claim | Same as above. |
| #552 | DeviceProductName cleaning | **Present** | `source/src/cip/cipidentity.c` in `SetDeviceProductName(...)` now trims leading/trailing whitespace and sanitizes non-printable bytes before storing | Product-name cleaning behavior is now explicit in code. |

## Platform-specific verification notes

- `GetMicroSeconds` naming consistency check:
  - `WIN32`, `MINGW`, `POSIX`, `ESP32`, `ClearCore` all now match the exported API symbol.
- Shared hardening lives mostly in common files:
  - `source/src/ports/generic_networkhandler.c`
  - `source/src/enet_encap/cpf.c`
  - `source/src/enet_encap/encap.c`
  - `source/src/cip/cipconnectionmanager.c`
  - `source/src/cip/cipmessagerouter.c`

## Recommended next strict step

For issues marked **Partial**, run targeted repros/fuzz seeds corresponding to each
issue report and compare runtime behavior against expected safe outcomes.

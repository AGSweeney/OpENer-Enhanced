#!/usr/bin/env python3
"""Aggressive local EtherNet/IP test client for OpENer.

This script repeatedly connects to a running OpENer instance and sends a mix of:
  - valid encapsulation/CIP requests
  - malformed CPF/CIP requests
  - random fuzz payloads
  - length-mismatch and fragmented packets

The goal is to stress parser and session handling paths and surface crashes,
hangs, resets, or unexpected protocol behavior.
"""

from __future__ import annotations

import argparse
import csv
import json
import os
import random
import socket
import struct
import threading
import time
from dataclasses import dataclass
from typing import Optional


ENCAP_HEADER_SIZE = 24

CMD_LIST_SERVICES = 0x0004
CMD_LIST_IDENTITY = 0x0063
CMD_LIST_INTERFACES = 0x0064
CMD_REGISTER_SESSION = 0x0065
CMD_UNREGISTER_SESSION = 0x0066
CMD_SEND_RR_DATA = 0x006F


@dataclass
class Stats:
    sent: int = 0
    responses: int = 0
    timeouts: int = 0
    resets: int = 0
    connect_failures: int = 0
    protocol_errors: int = 0
    exceptions: int = 0

    def add(self, other: "Stats") -> None:
        self.sent += other.sent
        self.responses += other.responses
        self.timeouts += other.timeouts
        self.resets += other.resets
        self.connect_failures += other.connect_failures
        self.protocol_errors += other.protocol_errors
        self.exceptions += other.exceptions


@dataclass
class AssemblyConfig:
    input_instance: int = 100
    output_instance: int = 150
    data_attribute: int = 3
    write_size: int = 32
    expect_mirror: bool = False


@dataclass
class PlcStats:
    cycles_attempted: int = 0
    cycles_completed: int = 0
    cycle_deadline_misses: int = 0
    write_ok: int = 0
    write_fail: int = 0
    read_ok: int = 0
    read_fail: int = 0
    connect_failures: int = 0
    resets: int = 0
    timeouts: int = 0
    exceptions: int = 0
    write_latency_ms: list[float] = None
    read_latency_ms: list[float] = None

    def __post_init__(self) -> None:
        if self.write_latency_ms is None:
            self.write_latency_ms = []
        if self.read_latency_ms is None:
            self.read_latency_ms = []

    def add(self, other: "PlcStats") -> None:
        self.cycles_attempted += other.cycles_attempted
        self.cycles_completed += other.cycles_completed
        self.cycle_deadline_misses += other.cycle_deadline_misses
        self.write_ok += other.write_ok
        self.write_fail += other.write_fail
        self.read_ok += other.read_ok
        self.read_fail += other.read_fail
        self.connect_failures += other.connect_failures
        self.resets += other.resets
        self.timeouts += other.timeouts
        self.exceptions += other.exceptions
        self.write_latency_ms.extend(other.write_latency_ms)
        self.read_latency_ms.extend(other.read_latency_ms)


class FailureRecorder:
    def __init__(self, output_dir: str) -> None:
        self.output_dir = output_dir
        self.enabled = bool(output_dir)
        self._lock = threading.Lock()
        self._counter = 0
        if self.enabled:
            os.makedirs(self.output_dir, exist_ok=True)

    def record(self, reason: str, worker_id: int, packet: bytes, response: bytes = b"") -> None:
        if not self.enabled:
            return
        timestamp_ms = int(time.time() * 1000)
        with self._lock:
            seq = self._counter
            self._counter += 1
        prefix = f"{timestamp_ms}_w{worker_id}_{seq:06d}_{reason}"
        packet_path = os.path.join(self.output_dir, f"{prefix}_request.bin")
        with open(packet_path, "wb") as f:
            f.write(packet)
        if response:
            response_path = os.path.join(self.output_dir, f"{prefix}_response.bin")
            with open(response_path, "wb") as f:
                f.write(response)
        meta = {
            "reason": reason,
            "worker_id": worker_id,
            "packet_len": len(packet),
            "response_len": len(response),
            "timestamp_ms": timestamp_ms,
        }
        with open(os.path.join(self.output_dir, f"{prefix}_meta.json"), "w", encoding="utf-8") as f:
            json.dump(meta, f, indent=2)


def build_encap(command: int, session_handle: int, payload: bytes, status: int = 0) -> bytes:
    sender_context = os.urandom(8)
    header = struct.pack(
        "<HHII8sI",
        command,
        len(payload),
        session_handle,
        status,
        sender_context,
        0,
    )
    return header + payload


def parse_encap_header(data: bytes) -> Optional[tuple[int, int, int, int]]:
    if len(data) < ENCAP_HEADER_SIZE:
        return None
    command, length, session, status = struct.unpack("<HHII", data[:12])
    return command, length, session, status


def rr_data(cpf_payload: bytes) -> bytes:
    return struct.pack("<IH", 0, 0) + cpf_payload


def cpf_null_ucmm(cip_payload: bytes) -> bytes:
    return (
        struct.pack("<H", 2) +        # item count
        struct.pack("<HH", 0x0000, 0) +  # null address
        struct.pack("<HH", 0x00B2, len(cip_payload)) +  # unconnected data item
        cip_payload
    )


def cip_get_attribute_single_identity_vendor() -> bytes:
    # Service 0x0E, path: class 0x01, instance 1, attribute 1.
    return bytes([0x0E, 0x03, 0x20, 0x01, 0x24, 0x01, 0x30, 0x01])


def cip_get_attribute_single_assembly_data(instance_id: int, attribute_id: int) -> bytes:
    # Service 0x0E, path class 0x04 assembly, specific instance and attribute.
    if not (0 <= instance_id <= 0xFF and 0 <= attribute_id <= 0xFF):
        raise ValueError("Assembly instance/attribute for this helper must be <= 255")
    return bytes([0x0E, 0x03, 0x20, 0x04, 0x24, instance_id, 0x30, attribute_id])


def cip_set_attribute_single_assembly_data(instance_id: int, attribute_id: int, payload: bytes) -> bytes:
    # Service 0x10, path class 0x04 assembly, specific instance and attribute.
    if not (0 <= instance_id <= 0xFF and 0 <= attribute_id <= 0xFF):
        raise ValueError("Assembly instance/attribute for this helper must be <= 255")
    return bytes([0x10, 0x03, 0x20, 0x04, 0x24, instance_id, 0x30, attribute_id]) + payload


def cip_get_attr_list_truncated() -> bytes:
    # Service 0x03, class 0x01 instance 1, then malformed payload (only one byte count).
    return bytes([0x03, 0x02, 0x20, 0x01, 0x24, 0x01, 0x01])


def cip_declared_path_oversize() -> bytes:
    # Service 0x0E with path size claiming 3 words, but only 2 words supplied.
    return bytes([0x0E, 0x03, 0x20, 0x01, 0x24, 0x01])


def parse_rr_cip_response(encap_response: bytes) -> Optional[dict]:
    parsed = parse_encap_header(encap_response)
    if parsed is None:
        return None
    _, encap_length, _, encap_status = parsed
    if encap_status != 0:
        return {
            "encap_status": encap_status,
            "cip_general_status": None,
            "cip_data": b"",
        }
    if len(encap_response) < ENCAP_HEADER_SIZE + encap_length:
        return None
    body = encap_response[ENCAP_HEADER_SIZE : ENCAP_HEADER_SIZE + encap_length]
    if len(body) < 6:
        return None
    # Skip Interface Handle (UDINT) and Timeout (UINT)
    cpf_blob = body[6:]
    if len(cpf_blob) < 10:
        return None
    item_count = struct.unpack_from("<H", cpf_blob, 0)[0]
    if item_count < 2:
        return None
    # Address item (4 bytes header + 0/4/8 bytes)
    addr_len = struct.unpack_from("<H", cpf_blob, 4)[0]
    addr_total = 4 + addr_len
    data_off = 2 + addr_total
    if len(cpf_blob) < data_off + 4:
        return None
    data_type, data_len = struct.unpack_from("<HH", cpf_blob, data_off)
    if data_type not in (0x00B1, 0x00B2):
        return None
    data_start = data_off + 4
    data_end = data_start + data_len
    if len(cpf_blob) < data_end:
        return None
    cip = cpf_blob[data_start:data_end]
    if len(cip) < 4:
        return None
    cip_general = cip[2]
    addl_count = cip[3]
    addl_bytes = 2 * addl_count
    payload_offset = 4 + addl_bytes
    cip_payload = cip[payload_offset:] if len(cip) >= payload_offset else b""
    return {
        "encap_status": encap_status,
        "cip_general_status": cip_general,
        "cip_data": cip_payload,
    }


def percentile(values: list[float], p: float) -> float:
    if not values:
        return 0.0
    sorted_values = sorted(values)
    idx = int(round((p / 100.0) * (len(sorted_values) - 1)))
    return sorted_values[max(0, min(idx, len(sorted_values) - 1))]


def make_test_packets(session_handle: int, rng: random.Random, assembly: AssemblyConfig) -> list[bytes]:
    packets: list[bytes] = []

    # Baseline valid requests.
    packets.append(build_encap(CMD_LIST_IDENTITY, session_handle, b""))
    packets.append(build_encap(CMD_LIST_SERVICES, session_handle, b""))
    packets.append(build_encap(CMD_LIST_INTERFACES, session_handle, b""))
    packets.append(
        build_encap(
            CMD_SEND_RR_DATA,
            session_handle,
            rr_data(cpf_null_ucmm(cip_get_attribute_single_identity_vendor())),
        )
    )
    read_output = cip_get_attribute_single_assembly_data(
        assembly.output_instance, assembly.data_attribute
    )
    packets.append(
        build_encap(CMD_SEND_RR_DATA, session_handle, rr_data(cpf_null_ucmm(read_output)))
    )
    write_payload = bytes(rng.getrandbits(8) for _ in range(max(1, assembly.write_size)))
    write_output = cip_set_attribute_single_assembly_data(
        assembly.output_instance, assembly.data_attribute, write_payload
    )
    packets.append(
        build_encap(CMD_SEND_RR_DATA, session_handle, rr_data(cpf_null_ucmm(write_output)))
    )
    read_input = cip_get_attribute_single_assembly_data(
        assembly.input_instance, assembly.data_attribute
    )
    packets.append(
        build_encap(CMD_SEND_RR_DATA, session_handle, rr_data(cpf_null_ucmm(read_input)))
    )

    # Malformed CPF item count (>4).
    malformed_cpf = struct.pack("<IH", 0, 0) + struct.pack("<H", 5)
    packets.append(build_encap(CMD_SEND_RR_DATA, session_handle, malformed_cpf))

    # Malformed CIP path declaration.
    packets.append(
        build_encap(
            CMD_SEND_RR_DATA,
            session_handle,
            rr_data(cpf_null_ucmm(cip_declared_path_oversize())),
        )
    )

    # Truncated GetAttributeList payload.
    packets.append(
        build_encap(
            CMD_SEND_RR_DATA,
            session_handle,
            rr_data(cpf_null_ucmm(cip_get_attr_list_truncated())),
        )
    )

    # Header length larger than payload.
    short_payload = b"\x00\x00"
    bad_header = struct.pack("<HHII8sI", CMD_SEND_RR_DATA, 64, session_handle, 0, os.urandom(8), 0)
    packets.append(bad_header + short_payload)

    # Random command + random payload.
    for _ in range(5):
        random_command = rng.randint(0, 0xFFFF)
        random_payload = os.urandom(rng.randint(0, 128))
        packets.append(build_encap(random_command, session_handle, random_payload))

    return packets


def send_rr_cip_and_parse(
    sock: socket.socket,
    session_handle: int,
    cip_payload: bytes,
    timeout: float,
) -> tuple[Optional[dict], float]:
    packet = build_encap(CMD_SEND_RR_DATA, session_handle, rr_data(cpf_null_ucmm(cip_payload)))
    start = time.perf_counter()
    sock.settimeout(timeout)
    sock.sendall(packet)
    response = sock.recv(4096)
    elapsed_ms = (time.perf_counter() - start) * 1000.0
    parsed = parse_rr_cip_response(response)
    return parsed, elapsed_ms


def send_and_maybe_receive(
    worker_id: int,
    sock: socket.socket,
    packet: bytes,
    fragmented: bool,
    rng: random.Random,
    timeout: float,
    stats: Stats,
    failure_recorder: FailureRecorder,
) -> None:
    try:
        if fragmented and len(packet) > 2:
            cursor = 0
            while cursor < len(packet):
                chunk_size = rng.randint(1, min(64, len(packet) - cursor))
                sock.sendall(packet[cursor : cursor + chunk_size])
                cursor += chunk_size
                if rng.random() < 0.3:
                    time.sleep(rng.uniform(0.0, 0.01))
        else:
            sock.sendall(packet)
        stats.sent += 1

        sock.settimeout(timeout)
        response = sock.recv(4096)
        if response:
            stats.responses += 1
            parsed = parse_encap_header(response)
            if parsed is None:
                stats.protocol_errors += 1
                failure_recorder.record("protocol_error_bad_response_header", worker_id, packet, response)
    except TimeoutError:
        stats.timeouts += 1
        failure_recorder.record("timeout", worker_id, packet)
    except (ConnectionResetError, BrokenPipeError):
        stats.resets += 1
        failure_recorder.record("connection_reset", worker_id, packet)
        raise


def register_session(sock: socket.socket, timeout: float) -> Optional[int]:
    register_payload = struct.pack("<HH", 1, 0)
    packet = build_encap(CMD_REGISTER_SESSION, 0, register_payload)
    sock.settimeout(timeout)
    sock.sendall(packet)
    response = sock.recv(1024)
    parsed = parse_encap_header(response)
    if parsed is None:
        return None
    command, _, session_handle, status = parsed
    if command != CMD_REGISTER_SESSION or status != 0 or session_handle == 0:
        return None
    return session_handle


def worker(
    worker_id: int,
    host: str,
    port: int,
    timeout: float,
    iterations: int,
    end_time: Optional[float],
    seed: int,
    stop_on_connect_failure: bool,
    failure_recorder: FailureRecorder,
    assembly: AssemblyConfig,
) -> Stats:
    stats = Stats()
    rng = random.Random(seed + worker_id * 1000003)

    def keep_going(i: int) -> bool:
        if end_time is not None and time.time() >= end_time:
            return False
        if iterations > 0 and i >= iterations:
            return False
        return True

    i = 0
    while keep_going(i):
        i += 1
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(timeout)
            sock.connect((host, port))
        except OSError:
            stats.connect_failures += 1
            if stop_on_connect_failure:
                break
            time.sleep(0.05)
            continue

        with sock:
            session_handle = register_session(sock, timeout)
            if session_handle is None:
                stats.protocol_errors += 1
                continue

            packets = make_test_packets(session_handle, rng, assembly)
            rng.shuffle(packets)

            for packet in packets:
                fragmented = rng.random() < 0.35
                try:
                    send_and_maybe_receive(worker_id, sock, packet, fragmented, rng, timeout, stats, failure_recorder)
                except (ConnectionResetError, BrokenPipeError):
                    # Connection died under test load. Continue next iteration.
                    break
                except Exception:
                    stats.exceptions += 1
                    failure_recorder.record("exception", worker_id, packet)
                    break

            # Best effort unregister.
            try:
                unregister_packet = build_encap(CMD_UNREGISTER_SESSION, session_handle, b"")
                sock.sendall(unregister_packet)
            except OSError:
                pass

    return stats


def plc_worker(
    worker_id: int,
    host: str,
    port: int,
    timeout: float,
    scan_ms: float,
    iterations: int,
    end_time: Optional[float],
    assembly: AssemblyConfig,
    verify_read_every: int,
) -> PlcStats:
    stats = PlcStats()
    cycle_period_s = max(0.001, scan_ms / 1000.0)
    verify_every = max(1, verify_read_every)

    def keep_going(i: int) -> bool:
        if end_time is not None and time.time() >= end_time:
            return False
        if iterations > 0 and i >= iterations:
            return False
        return True

    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(timeout)
        sock.connect((host, port))
    except OSError:
        stats.connect_failures += 1
        return stats

    with sock:
        session_handle = register_session(sock, timeout)
        if session_handle is None:
            stats.connect_failures += 1
            return stats

        cycle = 0
        next_deadline = time.perf_counter()
        while keep_going(cycle):
            cycle += 1
            stats.cycles_attempted += 1
            next_deadline += cycle_period_s
            cycle_ok = True
            write_pattern = bytes([(cycle + worker_id) & 0xFF]) * max(1, assembly.write_size)
            write_cip = cip_set_attribute_single_assembly_data(
                assembly.output_instance,
                assembly.data_attribute,
                write_pattern,
            )
            try:
                write_resp, write_latency = send_rr_cip_and_parse(
                    sock, session_handle, write_cip, timeout
                )
                stats.write_latency_ms.append(write_latency)
                if write_resp is None or write_resp["encap_status"] != 0 or write_resp["cip_general_status"] != 0:
                    stats.write_fail += 1
                    cycle_ok = False
                else:
                    stats.write_ok += 1
            except TimeoutError:
                stats.timeouts += 1
                stats.write_fail += 1
                cycle_ok = False
            except (ConnectionResetError, BrokenPipeError):
                stats.resets += 1
                stats.write_fail += 1
                break
            except Exception:
                stats.exceptions += 1
                stats.write_fail += 1
                cycle_ok = False

            if cycle_ok and (cycle % verify_every == 0):
                read_cip = cip_get_attribute_single_assembly_data(
                    assembly.input_instance if assembly.expect_mirror else assembly.output_instance,
                    assembly.data_attribute,
                )
                try:
                    read_resp, read_latency = send_rr_cip_and_parse(
                        sock, session_handle, read_cip, timeout
                    )
                    stats.read_latency_ms.append(read_latency)
                    if read_resp is None or read_resp["encap_status"] != 0 or read_resp["cip_general_status"] != 0:
                        stats.read_fail += 1
                        cycle_ok = False
                    else:
                        if len(read_resp["cip_data"]) >= len(write_pattern):
                            observed = read_resp["cip_data"][: len(write_pattern)]
                            if observed == write_pattern:
                                stats.read_ok += 1
                            else:
                                stats.read_fail += 1
                                cycle_ok = False
                        else:
                            stats.read_fail += 1
                            cycle_ok = False
                except TimeoutError:
                    stats.timeouts += 1
                    stats.read_fail += 1
                    cycle_ok = False
                except (ConnectionResetError, BrokenPipeError):
                    stats.resets += 1
                    stats.read_fail += 1
                    break
                except Exception:
                    stats.exceptions += 1
                    stats.read_fail += 1
                    cycle_ok = False

            if cycle_ok:
                stats.cycles_completed += 1

            now = time.perf_counter()
            if now > next_deadline:
                stats.cycle_deadline_misses += 1
            else:
                time.sleep(next_deadline - now)

        try:
            sock.sendall(build_encap(CMD_UNREGISTER_SESSION, session_handle, b""))
        except OSError:
            pass

    return stats


def run_sanity_suite(host: str, port: int, timeout: float, assembly: AssemblyConfig) -> dict:
    result = {
        "ok": True,
        "steps": [],
    }
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(timeout)
        sock.connect((host, port))
    except OSError as exc:
        result["ok"] = False
        result["steps"].append({"step": "connect", "ok": False, "detail": str(exc)})
        return result

    with sock:
        session_handle = register_session(sock, timeout)
        if session_handle is None:
            result["ok"] = False
            result["steps"].append({"step": "register_session", "ok": False, "detail": "invalid register response"})
            return result
        result["steps"].append({"step": "register_session", "ok": True})

        write_data = bytes([0xA5]) * max(1, assembly.write_size)
        sanity_packets = [
            ("list_identity", build_encap(CMD_LIST_IDENTITY, session_handle, b"")),
            ("list_services", build_encap(CMD_LIST_SERVICES, session_handle, b"")),
            ("list_interfaces", build_encap(CMD_LIST_INTERFACES, session_handle, b"")),
            (
                "send_rr_get_identity_vendor",
                build_encap(
                    CMD_SEND_RR_DATA,
                    session_handle,
                    rr_data(cpf_null_ucmm(cip_get_attribute_single_identity_vendor())),
                ),
            ),
            (
                "assembly_read_output_before",
                build_encap(
                    CMD_SEND_RR_DATA,
                    session_handle,
                    rr_data(
                        cpf_null_ucmm(
                            cip_get_attribute_single_assembly_data(
                                assembly.output_instance, assembly.data_attribute
                            )
                        )
                    ),
                ),
            ),
            (
                "assembly_write_output",
                build_encap(
                    CMD_SEND_RR_DATA,
                    session_handle,
                    rr_data(
                        cpf_null_ucmm(
                            cip_set_attribute_single_assembly_data(
                                assembly.output_instance, assembly.data_attribute, write_data
                            )
                        )
                    ),
                ),
            ),
            (
                "assembly_read_output_after",
                build_encap(
                    CMD_SEND_RR_DATA,
                    session_handle,
                    rr_data(
                        cpf_null_ucmm(
                            cip_get_attribute_single_assembly_data(
                                assembly.output_instance, assembly.data_attribute
                            )
                        )
                    ),
                ),
            ),
            (
                "assembly_read_input_after",
                build_encap(
                    CMD_SEND_RR_DATA,
                    session_handle,
                    rr_data(
                        cpf_null_ucmm(
                            cip_get_attribute_single_assembly_data(
                                assembly.input_instance, assembly.data_attribute
                            )
                        )
                    ),
                ),
            ),
        ]

        observed_output_after: Optional[bytes] = None
        observed_input_after: Optional[bytes] = None
        for step_name, packet in sanity_packets:
            try:
                sock.sendall(packet)
                response = sock.recv(4096)
                if not response:
                    result["ok"] = False
                    result["steps"].append({"step": step_name, "ok": False, "detail": "empty response"})
                    continue
                parsed = parse_encap_header(response)
                if parsed is None:
                    result["ok"] = False
                    result["steps"].append({"step": step_name, "ok": False, "detail": "short/invalid encapsulation header"})
                    continue
                _, _, _, status = parsed
                ok = status == 0
                detail = f"encap_status={status}"
                rr_info = None
                if step_name.startswith("assembly_") or step_name.startswith("send_rr_"):
                    rr_info = parse_rr_cip_response(response)
                    if rr_info is None:
                        ok = False
                        detail += " rr_parse=fail"
                    else:
                        cip_status = rr_info["cip_general_status"]
                        detail += f" cip_status={cip_status}"
                        if cip_status is not None and cip_status != 0:
                            ok = False
                        if step_name == "assembly_read_output_after":
                            observed_output_after = rr_info["cip_data"]
                        if step_name == "assembly_read_input_after":
                            observed_input_after = rr_info["cip_data"]
                result["steps"].append({"step": step_name, "ok": ok, "detail": detail})
                if not ok:
                    result["ok"] = False
            except Exception as exc:
                result["ok"] = False
                result["steps"].append({"step": step_name, "ok": False, "detail": str(exc)})

        if observed_output_after is not None:
            output_match = observed_output_after[: len(write_data)] == write_data
            result["steps"].append(
                {
                    "step": "assembly_verify_output_write_echo",
                    "ok": output_match,
                    "detail": f"expected_prefix_len={len(write_data)}",
                }
            )
            if not output_match:
                result["ok"] = False
        if assembly.expect_mirror and observed_output_after is not None and observed_input_after is not None:
            mirror_match = observed_input_after[: len(write_data)] == write_data
            result["steps"].append(
                {
                    "step": "assembly_verify_input_mirrors_output",
                    "ok": mirror_match,
                    "detail": f"expected_prefix_len={len(write_data)}",
                }
            )
            if not mirror_match:
                result["ok"] = False

        try:
            sock.sendall(build_encap(CMD_UNREGISTER_SESSION, session_handle, b""))
            result["steps"].append({"step": "unregister_session", "ok": True})
        except OSError as exc:
            result["ok"] = False
            result["steps"].append({"step": "unregister_session", "ok": False, "detail": str(exc)})

    return result


def write_reports(
    json_path: str,
    csv_path: str,
    summary: dict,
    per_worker: list[dict],
) -> None:
    if json_path:
        with open(json_path, "w", encoding="utf-8") as f:
            json.dump({"summary": summary, "workers": per_worker}, f, indent=2)
    if csv_path:
        if per_worker:
            preferred = ["worker_id"]
            rest = sorted(k for k in per_worker[0].keys() if k not in preferred)
            fieldnames = preferred + rest
        else:
            fieldnames = ["worker_id"]
        with open(csv_path, "w", encoding="utf-8", newline="") as f:
            writer = csv.DictWriter(f, fieldnames=fieldnames)
            writer.writeheader()
            for row in per_worker:
                writer.writerow(row)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Aggressive local OpENer test client",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument("--host", default="127.0.0.1", help="Target host running OpENer")
    parser.add_argument("--port", type=int, default=44818, help="Target EtherNet/IP TCP port")
    parser.add_argument("--workers", type=int, default=4, help="Parallel worker connections")
    parser.add_argument(
        "--iterations",
        type=int,
        default=200,
        help="Iterations per worker (ignored if --duration > 0)",
    )
    parser.add_argument(
        "--duration",
        type=float,
        default=0.0,
        help="Run for this many seconds instead of fixed iterations",
    )
    parser.add_argument("--timeout", type=float, default=0.5, help="Socket timeout in seconds")
    parser.add_argument("--seed", type=int, default=1337, help="Random seed")
    parser.add_argument(
        "--stop-on-connect-failure",
        action="store_true",
        help="Stop worker immediately when connect fails",
    )
    parser.add_argument(
        "--save-failing-packets-dir",
        default="",
        help="Directory for failing packet repro artifacts",
    )
    parser.add_argument(
        "--report-json",
        default="",
        help="Write run summary to this JSON file",
    )
    parser.add_argument(
        "--report-csv",
        default="",
        help="Write per-worker stats to this CSV file",
    )
    parser.add_argument(
        "--sanity-mode",
        choices=["none", "before", "after", "both"],
        default="none",
        help="Run known-good sanity checks before/after aggressive phase",
    )
    parser.add_argument(
        "--sanity-only",
        action="store_true",
        help="Run sanity suite only and skip aggressive phase",
    )
    parser.add_argument(
        "--mode",
        choices=["aggressive", "plc"],
        default="aggressive",
        help="Traffic model to run",
    )
    parser.add_argument(
        "--plc-scan-ms",
        type=float,
        default=50.0,
        help="PLC mode scan period in ms (supports down to 5ms)",
    )
    parser.add_argument(
        "--plc-verify-read-every",
        type=int,
        default=1,
        help="PLC mode: verify assembly read every N cycles",
    )
    parser.add_argument(
        "--assembly-input-instance",
        type=int,
        default=100,
        help="Assembly input instance id for explicit reads",
    )
    parser.add_argument(
        "--assembly-output-instance",
        type=int,
        default=150,
        help="Assembly output instance id for explicit reads/writes",
    )
    parser.add_argument(
        "--assembly-data-attribute",
        type=int,
        default=3,
        help="Assembly data attribute id",
    )
    parser.add_argument(
        "--assembly-write-size",
        type=int,
        default=32,
        help="Bytes written in assembly write operations",
    )
    parser.add_argument(
        "--assembly-expect-mirror",
        action="store_true",
        help="Require input assembly reads to mirror output writes",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.workers < 1:
        print("workers must be >= 1")
        return 2

    sanity_before = args.sanity_mode in ("before", "both")
    sanity_after = args.sanity_mode in ("after", "both")
    assembly = AssemblyConfig(
        input_instance=args.assembly_input_instance,
        output_instance=args.assembly_output_instance,
        data_attribute=args.assembly_data_attribute,
        write_size=max(1, args.assembly_write_size),
        expect_mirror=args.assembly_expect_mirror,
    )
    before_result = None
    after_result = None

    if sanity_before or args.sanity_only:
        print("Running sanity suite (before)...")
        before_result = run_sanity_suite(args.host, args.port, args.timeout, assembly)
        print(f"Sanity before result: {'PASS' if before_result['ok'] else 'FAIL'}")
        for step in before_result["steps"]:
            detail = step.get("detail", "")
            print(f"  - {step['step']}: {'ok' if step['ok'] else 'fail'} {detail}".rstrip())
        if args.sanity_only:
            if args.report_json or args.report_csv:
                write_reports(
                    args.report_json,
                    args.report_csv,
                    {
                        "target": f"{args.host}:{args.port}",
                        "workers": 0,
                        "elapsed_s": 0.0,
                        "packets_sent": 0,
                        "responses": 0,
                        "timeouts": 0,
                        "connection_resets": 0,
                        "connect_failures": 0,
                        "protocol_errors": 0,
                        "exceptions": 0,
                        "sanity_before_ok": before_result["ok"],
                        "sanity_after_ok": None,
                    },
                    [],
                )
            return 0 if before_result["ok"] else 1

    failure_recorder = FailureRecorder(args.save_failing_packets_dir)
    end_time = time.time() + args.duration if args.duration > 0 else None
    threads: list[threading.Thread] = []
    results: list[Stats] = [Stats() for _ in range(args.workers)]
    plc_results: list[PlcStats] = [PlcStats() for _ in range(args.workers)]

    start = time.time()
    if args.mode == "aggressive":
        for idx in range(args.workers):
            def run_worker(index: int = idx) -> None:
                results[index] = worker(
                    worker_id=index,
                    host=args.host,
                    port=args.port,
                    timeout=args.timeout,
                    iterations=args.iterations,
                    end_time=end_time,
                    seed=args.seed,
                    stop_on_connect_failure=args.stop_on_connect_failure,
                    failure_recorder=failure_recorder,
                    assembly=assembly,
                )

            t = threading.Thread(target=run_worker, name=f"worker-{idx}", daemon=True)
            threads.append(t)
            t.start()

        for t in threads:
            t.join()

        total = Stats()
        for item in results:
            total.add(item)

        elapsed = time.time() - start
        print("\n=== Aggressive OpENer Test Summary ===")
        print(f"target: {args.host}:{args.port}")
        print(f"workers: {args.workers}")
        print(f"elapsed_s: {elapsed:.2f}")
        print(f"packets_sent: {total.sent}")
        print(f"responses: {total.responses}")
        print(f"timeouts: {total.timeouts}")
        print(f"connection_resets: {total.resets}")
        print(f"connect_failures: {total.connect_failures}")
        print(f"protocol_errors: {total.protocol_errors}")
        print(f"exceptions: {total.exceptions}")
    else:
        if args.plc_scan_ms < 5.0:
            print("plc-scan-ms must be >= 5.0")
            return 2
        for idx in range(args.workers):
            def run_plc_worker(index: int = idx) -> None:
                plc_results[index] = plc_worker(
                    worker_id=index,
                    host=args.host,
                    port=args.port,
                    timeout=args.timeout,
                    scan_ms=args.plc_scan_ms,
                    iterations=args.iterations,
                    end_time=end_time,
                    assembly=assembly,
                    verify_read_every=args.plc_verify_read_every,
                )

            t = threading.Thread(target=run_plc_worker, name=f"plc-worker-{idx}", daemon=True)
            threads.append(t)
            t.start()

        for t in threads:
            t.join()

        total_plc = PlcStats()
        for item in plc_results:
            total_plc.add(item)
        elapsed = time.time() - start
        expected_cps_per_worker = 1000.0 / args.plc_scan_ms
        expected_cps_total = expected_cps_per_worker * args.workers
        achieved_cps = total_plc.cycles_attempted / elapsed if elapsed > 0 else 0.0
        print("\n=== PLC Mode OpENer Test Summary ===")
        print(f"target: {args.host}:{args.port}")
        print(f"workers: {args.workers}")
        print(f"scan_ms: {args.plc_scan_ms}")
        print(f"elapsed_s: {elapsed:.2f}")
        print(f"expected_cycles_per_sec_total: {expected_cps_total:.2f}")
        print(f"achieved_cycles_per_sec: {achieved_cps:.2f}")
        print(f"cycles_attempted: {total_plc.cycles_attempted}")
        print(f"cycles_completed: {total_plc.cycles_completed}")
        print(f"cycle_deadline_misses: {total_plc.cycle_deadline_misses}")
        print(f"write_ok: {total_plc.write_ok}")
        print(f"write_fail: {total_plc.write_fail}")
        print(f"read_ok: {total_plc.read_ok}")
        print(f"read_fail: {total_plc.read_fail}")
        print(f"timeouts: {total_plc.timeouts}")
        print(f"connection_resets: {total_plc.resets}")
        print(f"connect_failures: {total_plc.connect_failures}")
        print(f"exceptions: {total_plc.exceptions}")
        if total_plc.write_latency_ms:
            print(
                "write_latency_ms p50/p95/p99: "
                f"{percentile(total_plc.write_latency_ms, 50):.2f}/"
                f"{percentile(total_plc.write_latency_ms, 95):.2f}/"
                f"{percentile(total_plc.write_latency_ms, 99):.2f}"
            )
        if total_plc.read_latency_ms:
            print(
                "read_latency_ms p50/p95/p99: "
                f"{percentile(total_plc.read_latency_ms, 50):.2f}/"
                f"{percentile(total_plc.read_latency_ms, 95):.2f}/"
                f"{percentile(total_plc.read_latency_ms, 99):.2f}"
            )

    if sanity_after:
        print("Running sanity suite (after)...")
        after_result = run_sanity_suite(args.host, args.port, args.timeout, assembly)
        print(f"Sanity after result: {'PASS' if after_result['ok'] else 'FAIL'}")
        for step in after_result["steps"]:
            detail = step.get("detail", "")
            print(f"  - {step['step']}: {'ok' if step['ok'] else 'fail'} {detail}".rstrip())

    worker_rows = []
    if args.mode == "aggressive":
        for idx, item in enumerate(results):
            worker_rows.append(
                {
                    "worker_id": idx,
                    "sent": item.sent,
                    "responses": item.responses,
                    "timeouts": item.timeouts,
                    "resets": item.resets,
                    "connect_failures": item.connect_failures,
                    "protocol_errors": item.protocol_errors,
                    "exceptions": item.exceptions,
                }
            )
        summary = {
            "mode": "aggressive",
            "target": f"{args.host}:{args.port}",
            "workers": args.workers,
            "elapsed_s": round(elapsed, 3),
            "packets_sent": total.sent,
            "responses": total.responses,
            "timeouts": total.timeouts,
            "connection_resets": total.resets,
            "connect_failures": total.connect_failures,
            "protocol_errors": total.protocol_errors,
            "exceptions": total.exceptions,
            "sanity_before_ok": None if before_result is None else before_result["ok"],
            "sanity_after_ok": None if after_result is None else after_result["ok"],
            "fail_artifacts_dir": args.save_failing_packets_dir or None,
        }
    else:
        expected_cps_per_worker = 1000.0 / args.plc_scan_ms
        expected_cps_total = expected_cps_per_worker * args.workers
        achieved_cps = total_plc.cycles_attempted / elapsed if elapsed > 0 else 0.0
        for idx, item in enumerate(plc_results):
            worker_rows.append(
                {
                    "worker_id": idx,
                    "cycles_attempted": item.cycles_attempted,
                    "cycles_completed": item.cycles_completed,
                    "cycle_deadline_misses": item.cycle_deadline_misses,
                    "write_ok": item.write_ok,
                    "write_fail": item.write_fail,
                    "read_ok": item.read_ok,
                    "read_fail": item.read_fail,
                    "timeouts": item.timeouts,
                    "resets": item.resets,
                    "connect_failures": item.connect_failures,
                    "exceptions": item.exceptions,
                }
            )
        summary = {
            "mode": "plc",
            "target": f"{args.host}:{args.port}",
            "workers": args.workers,
            "scan_ms": args.plc_scan_ms,
            "elapsed_s": round(elapsed, 3),
            "expected_cycles_per_sec_total": round(expected_cps_total, 3),
            "achieved_cycles_per_sec": round(achieved_cps, 3),
            "cycles_attempted": total_plc.cycles_attempted,
            "cycles_completed": total_plc.cycles_completed,
            "cycle_deadline_misses": total_plc.cycle_deadline_misses,
            "write_ok": total_plc.write_ok,
            "write_fail": total_plc.write_fail,
            "read_ok": total_plc.read_ok,
            "read_fail": total_plc.read_fail,
            "timeouts": total_plc.timeouts,
            "connection_resets": total_plc.resets,
            "connect_failures": total_plc.connect_failures,
            "exceptions": total_plc.exceptions,
            "write_latency_p50_ms": round(percentile(total_plc.write_latency_ms, 50), 3),
            "write_latency_p95_ms": round(percentile(total_plc.write_latency_ms, 95), 3),
            "write_latency_p99_ms": round(percentile(total_plc.write_latency_ms, 99), 3),
            "read_latency_p50_ms": round(percentile(total_plc.read_latency_ms, 50), 3),
            "read_latency_p95_ms": round(percentile(total_plc.read_latency_ms, 95), 3),
            "read_latency_p99_ms": round(percentile(total_plc.read_latency_ms, 99), 3),
            "sanity_before_ok": None if before_result is None else before_result["ok"],
            "sanity_after_ok": None if after_result is None else after_result["ok"],
            "fail_artifacts_dir": args.save_failing_packets_dir or None,
        }
    if args.report_json or args.report_csv:
        write_reports(args.report_json, args.report_csv, summary, worker_rows)

    # Non-zero exit for severe connectivity failures.
    if args.mode == "aggressive":
        if total.connect_failures > 0 and total.responses == 0:
            print("No successful response received. Target may be down.")
            return 1
    else:
        if total_plc.connect_failures > 0 and total_plc.cycles_completed == 0:
            print("No completed PLC cycles. Target may be down.")
            return 1
    if before_result is not None and not before_result["ok"]:
        return 1
    if after_result is not None and not after_result["ok"]:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

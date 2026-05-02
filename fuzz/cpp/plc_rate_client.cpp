#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <mmsystem.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#pragma comment(lib, "Ws2_32.lib")

namespace {

constexpr uint16_t kCmdRegisterSession = 0x0065;
constexpr uint16_t kCmdUnregisterSession = 0x0066;
constexpr uint16_t kCmdSendRrData = 0x006F;

constexpr size_t kEncapHeaderSize = 24;

struct Config {
  std::string host = "127.0.0.1";
  uint16_t port = 44818;
  int workers = 1;
  double duration_s = 30.0;
  int cycles = 0;
  int timeout_ms = 500;
  std::vector<double> rates_ms = {50.0, 20.0, 10.0, 5.0};
  uint8_t input_instance = 100;
  uint8_t output_instance = 150;
  uint8_t data_attribute = 3;
  int write_size = 32;
  int verify_read_every = 1;
  double hard_miss_ms = 0.5;
  int timer_resolution_ms = 1;
  int spin_threshold_us = 300;
  bool no_spin_wait = false;
  bool expect_mirror = false;
};

struct WorkerStats {
  uint64_t cycles_attempted = 0;
  uint64_t cycles_completed = 0;
  uint64_t deadline_misses = 0;
  uint64_t hard_deadline_misses = 0;
  uint64_t write_ok = 0;
  uint64_t write_fail = 0;
  uint64_t read_ok = 0;
  uint64_t read_fail = 0;
  uint64_t connect_fail = 0;
  uint64_t timeouts = 0;
  uint64_t resets = 0;
  uint64_t exceptions = 0;
  std::vector<double> write_latency_ms;
  std::vector<double> read_latency_ms;
  std::vector<double> lateness_ms;
  std::vector<double> cycle_work_ms;

  void Add(const WorkerStats &other) {
    cycles_attempted += other.cycles_attempted;
    cycles_completed += other.cycles_completed;
    deadline_misses += other.deadline_misses;
    hard_deadline_misses += other.hard_deadline_misses;
    write_ok += other.write_ok;
    write_fail += other.write_fail;
    read_ok += other.read_ok;
    read_fail += other.read_fail;
    connect_fail += other.connect_fail;
    timeouts += other.timeouts;
    resets += other.resets;
    exceptions += other.exceptions;
    write_latency_ms.insert(write_latency_ms.end(),
                            other.write_latency_ms.begin(),
                            other.write_latency_ms.end());
    read_latency_ms.insert(read_latency_ms.end(),
                           other.read_latency_ms.begin(),
                           other.read_latency_ms.end());
    lateness_ms.insert(lateness_ms.end(),
                       other.lateness_ms.begin(),
                       other.lateness_ms.end());
    cycle_work_ms.insert(cycle_work_ms.end(),
                         other.cycle_work_ms.begin(),
                         other.cycle_work_ms.end());
  }
};

struct ParsedRrResponse {
  bool ok = false;
  uint32_t encap_status = 0;
  int cip_general_status = -1;
  std::vector<uint8_t> cip_payload;
};

bool ParseUInt(const std::string &text, int *out) {
  if (out == nullptr) {
    return false;
  }
  try {
    size_t parsed = 0;
    const int value = std::stoi(text, &parsed, 10);
    if (parsed != text.size()) {
      return false;
    }
    *out = value;
    return true;
  } catch (...) {
    return false;
  }
}

bool ParseDouble(const std::string &text, double *out) {
  if (out == nullptr) {
    return false;
  }
  try {
    size_t parsed = 0;
    const double value = std::stod(text, &parsed);
    if (parsed != text.size()) {
      return false;
    }
    *out = value;
    return true;
  } catch (...) {
    return false;
  }
}

std::vector<std::string> Split(const std::string &text, char delimiter) {
  std::vector<std::string> result;
  std::stringstream ss(text);
  std::string item;
  while (std::getline(ss, item, delimiter)) {
    if (!item.empty()) {
      result.push_back(item);
    }
  }
  return result;
}

void PrintUsage() {
  std::cout
      << "plc_rate_client - EtherNet/IP PLC cycle benchmark\n\n"
      << "Options:\n"
      << "  --host <ip-or-hostname>          Target host (default 127.0.0.1)\n"
      << "  --port <port>                    Target TCP port (default 44818)\n"
      << "  --workers <n>                    Parallel worker connections (default 1)\n"
      << "  --duration-s <seconds>           Duration per test rate (default 30)\n"
      << "  --cycles <n>                     Fixed cycles per worker (overrides duration if >0)\n"
      << "  --rates-ms <csv>                 Scan rates to test, e.g. 50,20,10,5,1\n"
      << "  --timeout-ms <ms>                Socket timeout (default 500)\n"
      << "  --assembly-input <id>            Input assembly instance (default 100)\n"
      << "  --assembly-output <id>           Output assembly instance (default 150)\n"
      << "  --assembly-attr <id>             Assembly data attribute (default 3)\n"
      << "  --write-size <bytes>             Bytes written each cycle (default 32)\n"
      << "  --verify-read-every <n>          Verify readback every N cycles (default 1)\n"
      << "  --hard-miss-ms <ms>              Count miss only if lateness >= threshold (default 0.5)\n"
      << "  --timer-resolution-ms <ms>       Request Windows timer resolution (default 1)\n"
      << "  --spin-threshold-us <us>         Busy-spin this close to deadline (default 300)\n"
      << "  --no-spin-wait                   Disable busy-spin final phase\n"
      << "  --expect-mirror                  Read input assembly instead of output\n"
      << "  --help                           Show this help\n";
}

bool ParseArgs(int argc, char **argv, Config *cfg) {
  if (cfg == nullptr) {
    return false;
  }
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    auto need_value = [&](const char *name) -> const char * {
      if (i + 1 >= argc) {
        std::cerr << "Missing value for " << name << "\n";
        return nullptr;
      }
      return argv[++i];
    };

    if (arg == "--help" || arg == "-h") {
      PrintUsage();
      return false;
    }
    if (arg == "--host") {
      const char *v = need_value("--host");
      if (v == nullptr) {
        return false;
      }
      cfg->host = v;
      continue;
    }
    if (arg == "--port") {
      const char *v = need_value("--port");
      int parsed = 0;
      if (v == nullptr || !ParseUInt(v, &parsed) || parsed <= 0 || parsed > 65535) {
        std::cerr << "Invalid --port\n";
        return false;
      }
      cfg->port = static_cast<uint16_t>(parsed);
      continue;
    }
    if (arg == "--workers") {
      const char *v = need_value("--workers");
      int parsed = 0;
      if (v == nullptr || !ParseUInt(v, &parsed) || parsed < 1) {
        std::cerr << "Invalid --workers\n";
        return false;
      }
      cfg->workers = parsed;
      continue;
    }
    if (arg == "--duration-s") {
      const char *v = need_value("--duration-s");
      double parsed = 0.0;
      if (v == nullptr || !ParseDouble(v, &parsed) || parsed <= 0.0) {
        std::cerr << "Invalid --duration-s\n";
        return false;
      }
      cfg->duration_s = parsed;
      continue;
    }
    if (arg == "--cycles") {
      const char *v = need_value("--cycles");
      int parsed = 0;
      if (v == nullptr || !ParseUInt(v, &parsed) || parsed < 0) {
        std::cerr << "Invalid --cycles\n";
        return false;
      }
      cfg->cycles = parsed;
      continue;
    }
    if (arg == "--rates-ms") {
      const char *v = need_value("--rates-ms");
      if (v == nullptr) {
        return false;
      }
      cfg->rates_ms.clear();
      for (const std::string &token : Split(v, ',')) {
        double parsed = 0.0;
        if (!ParseDouble(token, &parsed) || parsed < 1.0) {
          std::cerr << "Invalid rate in --rates-ms (minimum 1.0 ms)\n";
          return false;
        }
        cfg->rates_ms.push_back(parsed);
      }
      if (cfg->rates_ms.empty()) {
        std::cerr << "No rates provided for --rates-ms\n";
        return false;
      }
      continue;
    }
    if (arg == "--timeout-ms") {
      const char *v = need_value("--timeout-ms");
      int parsed = 0;
      if (v == nullptr || !ParseUInt(v, &parsed) || parsed < 1) {
        std::cerr << "Invalid --timeout-ms\n";
        return false;
      }
      cfg->timeout_ms = parsed;
      continue;
    }
    if (arg == "--assembly-input") {
      const char *v = need_value("--assembly-input");
      int parsed = 0;
      if (v == nullptr || !ParseUInt(v, &parsed) || parsed < 0 || parsed > 255) {
        std::cerr << "Invalid --assembly-input\n";
        return false;
      }
      cfg->input_instance = static_cast<uint8_t>(parsed);
      continue;
    }
    if (arg == "--assembly-output") {
      const char *v = need_value("--assembly-output");
      int parsed = 0;
      if (v == nullptr || !ParseUInt(v, &parsed) || parsed < 0 || parsed > 255) {
        std::cerr << "Invalid --assembly-output\n";
        return false;
      }
      cfg->output_instance = static_cast<uint8_t>(parsed);
      continue;
    }
    if (arg == "--assembly-attr") {
      const char *v = need_value("--assembly-attr");
      int parsed = 0;
      if (v == nullptr || !ParseUInt(v, &parsed) || parsed < 0 || parsed > 255) {
        std::cerr << "Invalid --assembly-attr\n";
        return false;
      }
      cfg->data_attribute = static_cast<uint8_t>(parsed);
      continue;
    }
    if (arg == "--write-size") {
      const char *v = need_value("--write-size");
      int parsed = 0;
      if (v == nullptr || !ParseUInt(v, &parsed) || parsed < 1 || parsed > 504) {
        std::cerr << "Invalid --write-size (1..504)\n";
        return false;
      }
      cfg->write_size = parsed;
      continue;
    }
    if (arg == "--verify-read-every") {
      const char *v = need_value("--verify-read-every");
      int parsed = 0;
      if (v == nullptr || !ParseUInt(v, &parsed) || parsed < 1) {
        std::cerr << "Invalid --verify-read-every\n";
        return false;
      }
      cfg->verify_read_every = parsed;
      continue;
    }
    if (arg == "--hard-miss-ms") {
      const char *v = need_value("--hard-miss-ms");
      double parsed = 0.0;
      if (v == nullptr || !ParseDouble(v, &parsed) || parsed < 0.0) {
        std::cerr << "Invalid --hard-miss-ms\n";
        return false;
      }
      cfg->hard_miss_ms = parsed;
      continue;
    }
    if (arg == "--timer-resolution-ms") {
      const char *v = need_value("--timer-resolution-ms");
      int parsed = 0;
      if (v == nullptr || !ParseUInt(v, &parsed) || parsed < 1 || parsed > 15) {
        std::cerr << "Invalid --timer-resolution-ms (1..15)\n";
        return false;
      }
      cfg->timer_resolution_ms = parsed;
      continue;
    }
    if (arg == "--spin-threshold-us") {
      const char *v = need_value("--spin-threshold-us");
      int parsed = 0;
      if (v == nullptr || !ParseUInt(v, &parsed) || parsed < 0 || parsed > 5000) {
        std::cerr << "Invalid --spin-threshold-us (0..5000)\n";
        return false;
      }
      cfg->spin_threshold_us = parsed;
      continue;
    }
    if (arg == "--no-spin-wait") {
      cfg->no_spin_wait = true;
      continue;
    }
    if (arg == "--expect-mirror") {
      cfg->expect_mirror = true;
      continue;
    }

    std::cerr << "Unknown argument: " << arg << "\n";
    return false;
  }
  return true;
}

void PushLe16(std::vector<uint8_t> *out, uint16_t value) {
  out->push_back(static_cast<uint8_t>(value & 0xFFU));
  out->push_back(static_cast<uint8_t>((value >> 8U) & 0xFFU));
}

void PushLe32(std::vector<uint8_t> *out, uint32_t value) {
  out->push_back(static_cast<uint8_t>(value & 0xFFU));
  out->push_back(static_cast<uint8_t>((value >> 8U) & 0xFFU));
  out->push_back(static_cast<uint8_t>((value >> 16U) & 0xFFU));
  out->push_back(static_cast<uint8_t>((value >> 24U) & 0xFFU));
}

std::vector<uint8_t> BuildEncap(uint16_t command, uint32_t session_handle,
                                const std::vector<uint8_t> &payload) {
  std::vector<uint8_t> packet;
  packet.reserve(kEncapHeaderSize + payload.size());
  PushLe16(&packet, command);
  PushLe16(&packet, static_cast<uint16_t>(payload.size()));
  PushLe32(&packet, session_handle);
  PushLe32(&packet, 0);
  for (int i = 0; i < 8; ++i) {
    packet.push_back(static_cast<uint8_t>(i + 1));
  }
  PushLe32(&packet, 0);
  packet.insert(packet.end(), payload.begin(), payload.end());
  return packet;
}

std::vector<uint8_t> BuildRrData(const std::vector<uint8_t> &cpf_payload) {
  std::vector<uint8_t> out;
  out.reserve(6 + cpf_payload.size());
  PushLe32(&out, 0);
  PushLe16(&out, 0);
  out.insert(out.end(), cpf_payload.begin(), cpf_payload.end());
  return out;
}

std::vector<uint8_t> BuildCpfNullUcmm(const std::vector<uint8_t> &cip_payload) {
  std::vector<uint8_t> out;
  out.reserve(10 + cip_payload.size());
  PushLe16(&out, 2);
  PushLe16(&out, 0x0000);
  PushLe16(&out, 0);
  PushLe16(&out, 0x00B2);
  PushLe16(&out, static_cast<uint16_t>(cip_payload.size()));
  out.insert(out.end(), cip_payload.begin(), cip_payload.end());
  return out;
}

std::vector<uint8_t> CipGetAssemblyData(uint8_t instance, uint8_t attribute) {
  return {0x0E, 0x03, 0x20, 0x04, 0x24, instance, 0x30, attribute};
}

std::vector<uint8_t> CipSetAssemblyData(uint8_t instance, uint8_t attribute,
                                        const std::vector<uint8_t> &payload) {
  std::vector<uint8_t> out = {0x10, 0x03, 0x20, 0x04, 0x24, instance, 0x30, attribute};
  out.insert(out.end(), payload.begin(), payload.end());
  return out;
}

std::vector<uint8_t> BuildCycleWritePattern(int cycle, int worker_id, int write_size) {
  std::vector<uint8_t> data(static_cast<size_t>(write_size), 0);
  const uint8_t cycle_base = static_cast<uint8_t>((cycle + worker_id * 17) & 0xFF);
  for (size_t i = 0; i < data.size(); ++i) {
    data[i] = static_cast<uint8_t>((cycle_base + static_cast<uint8_t>(i * 13U)) & 0xFF);
  }
  return data;
}

bool SendAll(SOCKET sock, const std::vector<uint8_t> &buffer) {
  int sent = 0;
  while (sent < static_cast<int>(buffer.size())) {
    const int rc = send(sock, reinterpret_cast<const char *>(buffer.data()) + sent,
                        static_cast<int>(buffer.size()) - sent, 0);
    if (rc <= 0) {
      return false;
    }
    sent += rc;
  }
  return true;
}

bool RecvSome(SOCKET sock, std::vector<uint8_t> *out) {
  out->assign(4096, 0);
  const int rc = recv(sock, reinterpret_cast<char *>(out->data()),
                      static_cast<int>(out->size()), 0);
  if (rc <= 0) {
    out->clear();
    return false;
  }
  out->resize(static_cast<size_t>(rc));
  return true;
}

bool ParseEncapHeader(const std::vector<uint8_t> &buffer, uint16_t *command,
                      uint16_t *length, uint32_t *session, uint32_t *status) {
  if (buffer.size() < kEncapHeaderSize) {
    return false;
  }
  *command = static_cast<uint16_t>(buffer[0] | (buffer[1] << 8));
  *length = static_cast<uint16_t>(buffer[2] | (buffer[3] << 8));
  *session = static_cast<uint32_t>(buffer[4] | (buffer[5] << 8) |
                                   (buffer[6] << 16) | (buffer[7] << 24));
  *status = static_cast<uint32_t>(buffer[8] | (buffer[9] << 8) |
                                  (buffer[10] << 16) | (buffer[11] << 24));
  return true;
}

ParsedRrResponse ParseRrCipResponse(const std::vector<uint8_t> &response) {
  ParsedRrResponse parsed;
  uint16_t command = 0;
  uint16_t length = 0;
  uint32_t session = 0;
  uint32_t status = 0;
  if (!ParseEncapHeader(response, &command, &length, &session, &status)) {
    return parsed;
  }
  parsed.encap_status = status;
  if (status != 0) {
    parsed.ok = true;
    return parsed;
  }
  if (response.size() < kEncapHeaderSize + length) {
    return parsed;
  }
  const uint8_t *body = response.data() + kEncapHeaderSize;
  if (length < 6) {
    return parsed;
  }
  const uint8_t *cpf = body + 6;
  const size_t cpf_size = length - 6;
  if (cpf_size < 10) {
    return parsed;
  }
  const uint16_t item_count = static_cast<uint16_t>(cpf[0] | (cpf[1] << 8));
  if (item_count < 2) {
    return parsed;
  }
  const uint16_t addr_len = static_cast<uint16_t>(cpf[4] | (cpf[5] << 8));
  const size_t data_off = 2U + 4U + addr_len;
  if (cpf_size < data_off + 4U) {
    return parsed;
  }
  const uint16_t data_type =
      static_cast<uint16_t>(cpf[data_off] | (cpf[data_off + 1] << 8));
  const uint16_t data_len = static_cast<uint16_t>(cpf[data_off + 2] |
                                                  (cpf[data_off + 3] << 8));
  if (data_type != 0x00B1U && data_type != 0x00B2U) {
    return parsed;
  }
  const size_t data_start = data_off + 4U;
  if (cpf_size < data_start + data_len) {
    return parsed;
  }
  const uint8_t *cip = cpf + data_start;
  if (data_len < 4) {
    return parsed;
  }
  parsed.cip_general_status = static_cast<int>(cip[2]);
  const size_t addl_count = cip[3];
  const size_t payload_offset = 4U + (2U * addl_count);
  if (data_len >= payload_offset) {
    parsed.cip_payload.assign(cip + payload_offset, cip + data_len);
  }
  parsed.ok = true;
  return parsed;
}

bool RegisterSession(SOCKET sock, uint32_t *session_handle) {
  std::vector<uint8_t> payload;
  PushLe16(&payload, 1);
  PushLe16(&payload, 0);
  const std::vector<uint8_t> packet = BuildEncap(kCmdRegisterSession, 0, payload);
  if (!SendAll(sock, packet)) {
    return false;
  }
  std::vector<uint8_t> response;
  if (!RecvSome(sock, &response)) {
    return false;
  }
  uint16_t command = 0;
  uint16_t length = 0;
  uint32_t session = 0;
  uint32_t status = 0;
  if (!ParseEncapHeader(response, &command, &length, &session, &status)) {
    return false;
  }
  if (command != kCmdRegisterSession || status != 0 || session == 0) {
    return false;
  }
  *session_handle = session;
  return true;
}

void UnregisterSession(SOCKET sock, uint32_t session_handle) {
  const std::vector<uint8_t> packet = BuildEncap(kCmdUnregisterSession, session_handle, {});
  SendAll(sock, packet);
}

double Percentile(std::vector<double> values, double p) {
  if (values.empty()) {
    return 0.0;
  }
  std::sort(values.begin(), values.end());
  const double idx_f = (p / 100.0) * static_cast<double>(values.size() - 1);
  size_t idx = static_cast<size_t>(idx_f + 0.5);
  if (idx >= values.size()) {
    idx = values.size() - 1;
  }
  return values[idx];
}

void WaitUntilDeadline(const std::chrono::steady_clock::time_point &deadline,
                       int spin_threshold_us, bool no_spin_wait) {
  auto now = std::chrono::steady_clock::now();
  if (now >= deadline) {
    return;
  }

  if (no_spin_wait || spin_threshold_us <= 0) {
    std::this_thread::sleep_until(deadline);
    return;
  }

  const auto spin_window = std::chrono::microseconds(spin_threshold_us);
  const auto sleep_until = deadline - spin_window;
  now = std::chrono::steady_clock::now();
  if (now < sleep_until) {
    std::this_thread::sleep_until(sleep_until);
  }
  while (std::chrono::steady_clock::now() < deadline) {
  }
}

WorkerStats RunWorker(const Config &cfg, double scan_ms, int worker_id) {
  WorkerStats stats;
  const double cycle_period_s = scan_ms / 1000.0;
  const int verify_every = (std::max)(1, cfg.verify_read_every);
  const auto cycle_period =
      std::chrono::microseconds(static_cast<int64_t>(cycle_period_s * 1000000.0));

  SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sock == INVALID_SOCKET) {
    stats.connect_fail += 1;
    return stats;
  }

  const DWORD timeout = static_cast<DWORD>(cfg.timeout_ms);
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&timeout),
             sizeof(timeout));
  setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char *>(&timeout),
             sizeof(timeout));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(cfg.port);
  if (inet_pton(AF_INET, cfg.host.c_str(), &addr.sin_addr) != 1) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    addrinfo *result = nullptr;
    if (getaddrinfo(cfg.host.c_str(), nullptr, &hints, &result) != 0 || result == nullptr) {
      closesocket(sock);
      stats.connect_fail += 1;
      return stats;
    }
    addr.sin_addr = reinterpret_cast<sockaddr_in *>(result->ai_addr)->sin_addr;
    freeaddrinfo(result);
  }
  if (connect(sock, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
    closesocket(sock);
    stats.connect_fail += 1;
    return stats;
  }

  uint32_t session_handle = 0;
  if (!RegisterSession(sock, &session_handle)) {
    closesocket(sock);
    stats.connect_fail += 1;
    return stats;
  }

  const auto end_time = std::chrono::steady_clock::now() +
                        std::chrono::duration<double>(cfg.duration_s);
  auto next_deadline = std::chrono::steady_clock::now();

  int cycle = 0;
  while (true) {
    if (cfg.cycles > 0) {
      if (cycle >= cfg.cycles) {
        break;
      }
    } else {
      if (std::chrono::steady_clock::now() >= end_time) {
        break;
      }
    }

    cycle += 1;
    stats.cycles_attempted += 1;
    if (cycle > 1) {
      const auto pre_sleep_now = std::chrono::steady_clock::now();
      if (pre_sleep_now < next_deadline) {
        WaitUntilDeadline(next_deadline, cfg.spin_threshold_us, cfg.no_spin_wait);
      }
    }
    const auto cycle_start = std::chrono::steady_clock::now();
    if (cycle_start > next_deadline) {
      const double late_ms = std::chrono::duration<double, std::milli>(
          cycle_start - next_deadline).count();
      stats.deadline_misses += 1;
      stats.lateness_ms.push_back(late_ms);
      if (late_ms >= cfg.hard_miss_ms) {
        stats.hard_deadline_misses += 1;
      }
    }
    next_deadline += cycle_period;

    bool cycle_ok = true;
    std::vector<uint8_t> write_data =
        BuildCycleWritePattern(cycle, worker_id, cfg.write_size);
    const std::vector<uint8_t> write_cip =
        CipSetAssemblyData(cfg.output_instance, cfg.data_attribute, write_data);
    const std::vector<uint8_t> write_packet =
        BuildEncap(kCmdSendRrData, session_handle, BuildRrData(BuildCpfNullUcmm(write_cip)));

    const auto write_start = std::chrono::steady_clock::now();
    if (!SendAll(sock, write_packet)) {
      stats.resets += 1;
      stats.write_fail += 1;
      break;
    }
    std::vector<uint8_t> write_response;
    if (!RecvSome(sock, &write_response)) {
      const int err = WSAGetLastError();
      if (err == WSAETIMEDOUT) {
        stats.timeouts += 1;
      } else {
        stats.resets += 1;
      }
      stats.write_fail += 1;
      break;
    }
    const auto write_stop = std::chrono::steady_clock::now();
    stats.write_latency_ms.push_back(
        std::chrono::duration<double, std::milli>(write_stop - write_start).count());
    const ParsedRrResponse write_parsed = ParseRrCipResponse(write_response);
    if (!write_parsed.ok || write_parsed.encap_status != 0 ||
        write_parsed.cip_general_status != 0) {
      stats.write_fail += 1;
      cycle_ok = false;
    } else {
      stats.write_ok += 1;
    }

    if (cycle_ok && (cycle % verify_every == 0)) {
      const uint8_t read_instance = cfg.expect_mirror ? cfg.input_instance : cfg.output_instance;
      const std::vector<uint8_t> read_cip =
          CipGetAssemblyData(read_instance, cfg.data_attribute);
      const std::vector<uint8_t> read_packet =
          BuildEncap(kCmdSendRrData, session_handle, BuildRrData(BuildCpfNullUcmm(read_cip)));
      const auto read_start = std::chrono::steady_clock::now();
      if (!SendAll(sock, read_packet)) {
        stats.resets += 1;
        stats.read_fail += 1;
        break;
      }
      std::vector<uint8_t> read_response;
      if (!RecvSome(sock, &read_response)) {
        const int err = WSAGetLastError();
        if (err == WSAETIMEDOUT) {
          stats.timeouts += 1;
        } else {
          stats.resets += 1;
        }
        stats.read_fail += 1;
        break;
      }
      const auto read_stop = std::chrono::steady_clock::now();
      stats.read_latency_ms.push_back(
          std::chrono::duration<double, std::milli>(read_stop - read_start).count());
      const ParsedRrResponse read_parsed = ParseRrCipResponse(read_response);
      if (!read_parsed.ok || read_parsed.encap_status != 0 ||
          read_parsed.cip_general_status != 0) {
        stats.read_fail += 1;
        cycle_ok = false;
      } else {
        const bool payload_ok = read_parsed.cip_payload.size() >= write_data.size() &&
                                std::equal(write_data.begin(), write_data.end(),
                                           read_parsed.cip_payload.begin());
        if (payload_ok) {
          stats.read_ok += 1;
        } else {
          stats.read_fail += 1;
          cycle_ok = false;
        }
      }
    }

    if (cycle_ok) {
      stats.cycles_completed += 1;
    }
    const auto cycle_stop = std::chrono::steady_clock::now();
    stats.cycle_work_ms.push_back(
        std::chrono::duration<double, std::milli>(cycle_stop - cycle_start).count());
  }

  UnregisterSession(sock, session_handle);
  closesocket(sock);
  return stats;
}

bool RunRate(const Config &cfg, double scan_ms) {
  std::vector<std::thread> threads;
  std::vector<WorkerStats> per_worker(static_cast<size_t>(cfg.workers));

  const auto rate_start = std::chrono::steady_clock::now();
  for (int i = 0; i < cfg.workers; ++i) {
    threads.emplace_back([&, i]() { per_worker[static_cast<size_t>(i)] = RunWorker(cfg, scan_ms, i); });
  }
  for (auto &t : threads) {
    t.join();
  }
  const auto rate_stop = std::chrono::steady_clock::now();
  const double elapsed_s =
      std::chrono::duration<double>(rate_stop - rate_start).count();

  WorkerStats total;
  for (const WorkerStats &ws : per_worker) {
    total.Add(ws);
  }

  const double expected_cps = (1000.0 / scan_ms) * static_cast<double>(cfg.workers);
  const double achieved_cps =
      elapsed_s > 0.0 ? static_cast<double>(total.cycles_attempted) / elapsed_s : 0.0;

  std::cout << "\n=== PLC Rate Result ===\n";
  std::cout << "scan_ms: " << scan_ms << "\n";
  std::cout << "workers: " << cfg.workers << "\n";
  std::cout << "elapsed_s: " << std::fixed << std::setprecision(3) << elapsed_s << "\n";
  std::cout << "expected_cycles_per_sec: " << std::setprecision(2) << expected_cps << "\n";
  std::cout << "achieved_cycles_per_sec: " << std::setprecision(2) << achieved_cps << "\n";
  std::cout << "cycles_attempted: " << total.cycles_attempted << "\n";
  std::cout << "cycles_completed: " << total.cycles_completed << "\n";
  std::cout << "deadline_misses(any_late): " << total.deadline_misses << "\n";
  std::cout << "deadline_misses_hard(>=" << cfg.hard_miss_ms << "ms): "
            << total.hard_deadline_misses << "\n";
  std::cout << "write_ok/write_fail: " << total.write_ok << "/" << total.write_fail << "\n";
  std::cout << "read_ok/read_fail: " << total.read_ok << "/" << total.read_fail << "\n";
  std::cout << "timeouts: " << total.timeouts << " resets: " << total.resets
            << " connect_fail: " << total.connect_fail
            << " exceptions: " << total.exceptions << "\n";
  if (!total.write_latency_ms.empty()) {
    std::cout << "write_latency_ms p50/p95/p99: "
              << std::setprecision(3) << Percentile(total.write_latency_ms, 50.0) << "/"
              << Percentile(total.write_latency_ms, 95.0) << "/"
              << Percentile(total.write_latency_ms, 99.0) << "\n";
  }
  if (!total.read_latency_ms.empty()) {
    std::cout << "read_latency_ms p50/p95/p99: "
              << std::setprecision(3) << Percentile(total.read_latency_ms, 50.0) << "/"
              << Percentile(total.read_latency_ms, 95.0) << "/"
              << Percentile(total.read_latency_ms, 99.0) << "\n";
  }
  if (!total.cycle_work_ms.empty()) {
    std::cout << "cycle_work_ms p50/p95/p99: "
              << std::setprecision(3) << Percentile(total.cycle_work_ms, 50.0) << "/"
              << Percentile(total.cycle_work_ms, 95.0) << "/"
              << Percentile(total.cycle_work_ms, 99.0) << "\n";
  }
  if (!total.lateness_ms.empty()) {
    const double max_late = *std::max_element(total.lateness_ms.begin(),
                                              total.lateness_ms.end());
    std::cout << "lateness_ms p50/p95/p99/max: "
              << std::setprecision(3) << Percentile(total.lateness_ms, 50.0) << "/"
              << Percentile(total.lateness_ms, 95.0) << "/"
              << Percentile(total.lateness_ms, 99.0) << "/"
              << max_late << "\n";
  }

  if (total.connect_fail > 0 && total.cycles_completed == 0) {
    std::cout << "RESULT: FAIL (no completed cycles)\n";
    return false;
  }
  std::cout << "RESULT: PASS\n";
  return true;
}

}  // namespace

int main(int argc, char **argv) {
  Config cfg;
  if (!ParseArgs(argc, argv, &cfg)) {
    return (argc > 1 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")) ? 0 : 2;
  }

  WSADATA wsa_data{};
  if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
    std::cerr << "WSAStartup failed\n";
    return 1;
  }

  std::cout << "Target: " << cfg.host << ":" << cfg.port << "\n";
  std::cout << "Timer resolution request (ms): " << cfg.timer_resolution_ms << "\n";
  std::cout << "Spin threshold (us): " << cfg.spin_threshold_us
            << (cfg.no_spin_wait ? " (disabled)" : "") << "\n";
  std::cout << "Rates(ms): ";
  for (size_t i = 0; i < cfg.rates_ms.size(); ++i) {
    if (i != 0) {
      std::cout << ",";
    }
    std::cout << cfg.rates_ms[i];
  }
  std::cout << "\n";

  const MMRESULT timer_result = timeBeginPeriod(static_cast<UINT>(cfg.timer_resolution_ms));
  if (timer_result != TIMERR_NOERROR) {
    std::cerr << "Warning: timeBeginPeriod failed; timer jitter may be high.\n";
  }

  bool all_ok = true;
  for (double rate : cfg.rates_ms) {
    if (!RunRate(cfg, rate)) {
      all_ok = false;
    }
  }

  if (timer_result == TIMERR_NOERROR) {
    timeEndPeriod(static_cast<UINT>(cfg.timer_resolution_ms));
  }

  WSACleanup();
  return all_ok ? 0 : 1;
}

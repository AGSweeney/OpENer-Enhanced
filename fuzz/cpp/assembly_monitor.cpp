#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <mmsystem.h>

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "Ws2_32.lib")

namespace {

constexpr uint16_t kCmdRegisterSession = 0x0065;
constexpr uint16_t kCmdUnregisterSession = 0x0066;
constexpr uint16_t kCmdSendRrData = 0x006F;
constexpr size_t kEncapHeaderSize = 24;

struct Config {
  std::string host = "172.16.82.199";
  uint16_t port = 44818;
  double interval_ms = 1.0;
  double duration_s = 0.0;
  int timeout_ms = 500;
  int timer_resolution_ms = 1;
  uint8_t assembly_instance = 150;
  uint8_t attribute = 3;
  bool print_all = false;
};

struct RrResponse {
  bool ok = false;
  uint32_t encap_status = 0;
  int cip_status = -1;
  std::vector<uint8_t> payload;
};

bool ParseInt(const std::string &text, int *out) {
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

void PrintUsage() {
  std::cout
      << "assembly_monitor - Read OpENer assembly data cyclically\n\n"
      << "Options:\n"
      << "  --host <ip-or-hostname>      Target host (default 172.16.82.199)\n"
      << "  --port <port>                Target TCP port (default 44818)\n"
      << "  --interval-ms <ms>           Poll interval (default 1.0)\n"
      << "  --duration-s <seconds>       Optional run length, 0=infinite (default 0)\n"
      << "  --timeout-ms <ms>            Socket timeout (default 500)\n"
      << "  --timer-resolution-ms <ms>   Windows timer request (default 1)\n"
      << "  --assembly-instance <id>     Assembly instance (default 150)\n"
      << "  --attribute <id>             Assembly attribute (default 3)\n"
      << "  --print-all                  Print every poll, not only changes\n"
      << "  --help                       Show this help\n";
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
      if (v == nullptr || !ParseInt(v, &parsed) || parsed <= 0 || parsed > 65535) {
        std::cerr << "Invalid --port\n";
        return false;
      }
      cfg->port = static_cast<uint16_t>(parsed);
      continue;
    }
    if (arg == "--interval-ms") {
      const char *v = need_value("--interval-ms");
      double parsed = 0.0;
      if (v == nullptr || !ParseDouble(v, &parsed) || parsed < 1.0) {
        std::cerr << "Invalid --interval-ms (minimum 1.0)\n";
        return false;
      }
      cfg->interval_ms = parsed;
      continue;
    }
    if (arg == "--duration-s") {
      const char *v = need_value("--duration-s");
      double parsed = 0.0;
      if (v == nullptr || !ParseDouble(v, &parsed) || parsed < 0.0) {
        std::cerr << "Invalid --duration-s\n";
        return false;
      }
      cfg->duration_s = parsed;
      continue;
    }
    if (arg == "--timeout-ms") {
      const char *v = need_value("--timeout-ms");
      int parsed = 0;
      if (v == nullptr || !ParseInt(v, &parsed) || parsed < 1) {
        std::cerr << "Invalid --timeout-ms\n";
        return false;
      }
      cfg->timeout_ms = parsed;
      continue;
    }
    if (arg == "--timer-resolution-ms") {
      const char *v = need_value("--timer-resolution-ms");
      int parsed = 0;
      if (v == nullptr || !ParseInt(v, &parsed) || parsed < 1 || parsed > 15) {
        std::cerr << "Invalid --timer-resolution-ms (1..15)\n";
        return false;
      }
      cfg->timer_resolution_ms = parsed;
      continue;
    }
    if (arg == "--assembly-instance") {
      const char *v = need_value("--assembly-instance");
      int parsed = 0;
      if (v == nullptr || !ParseInt(v, &parsed) || parsed < 0 || parsed > 255) {
        std::cerr << "Invalid --assembly-instance\n";
        return false;
      }
      cfg->assembly_instance = static_cast<uint8_t>(parsed);
      continue;
    }
    if (arg == "--attribute") {
      const char *v = need_value("--attribute");
      int parsed = 0;
      if (v == nullptr || !ParseInt(v, &parsed) || parsed < 0 || parsed > 255) {
        std::cerr << "Invalid --attribute\n";
        return false;
      }
      cfg->attribute = static_cast<uint8_t>(parsed);
      continue;
    }
    if (arg == "--print-all") {
      cfg->print_all = true;
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

std::vector<uint8_t> BuildEncap(uint16_t command,
                                uint32_t session_handle,
                                const std::vector<uint8_t> &payload) {
  std::vector<uint8_t> packet;
  packet.reserve(kEncapHeaderSize + payload.size());
  PushLe16(&packet, command);
  PushLe16(&packet, static_cast<uint16_t>(payload.size()));
  PushLe32(&packet, session_handle);
  PushLe32(&packet, 0);
  for(int i = 0; i < 8; ++i) {
    packet.push_back(static_cast<uint8_t>(i + 1));
  }
  PushLe32(&packet, 0);
  packet.insert(packet.end(), payload.begin(), payload.end());
  return packet;
}

std::vector<uint8_t> BuildGetAssemblyCip(uint8_t instance, uint8_t attribute) {
  return {0x0E, 0x03, 0x20, 0x04, 0x24, instance, 0x30, attribute};
}

std::vector<uint8_t> BuildRrRequest(const std::vector<uint8_t> &cip) {
  std::vector<uint8_t> cpf;
  PushLe16(&cpf, 2);
  PushLe16(&cpf, 0x0000);
  PushLe16(&cpf, 0);
  PushLe16(&cpf, 0x00B2);
  PushLe16(&cpf, static_cast<uint16_t>(cip.size()));
  cpf.insert(cpf.end(), cip.begin(), cip.end());

  std::vector<uint8_t> rr;
  PushLe32(&rr, 0);
  PushLe16(&rr, 0);
  rr.insert(rr.end(), cpf.begin(), cpf.end());
  return rr;
}

bool SendAll(SOCKET sock, const std::vector<uint8_t> &buffer) {
  int sent = 0;
  while(sent < static_cast<int>(buffer.size())) {
    const int rc = send(sock,
                        reinterpret_cast<const char *>(buffer.data()) + sent,
                        static_cast<int>(buffer.size()) - sent,
                        0);
    if(rc <= 0) {
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
  if(rc <= 0) {
    out->clear();
    return false;
  }
  out->resize(static_cast<size_t>(rc));
  return true;
}

bool ParseEncapHeader(const std::vector<uint8_t> &buffer,
                      uint16_t *command,
                      uint16_t *length,
                      uint32_t *session,
                      uint32_t *status) {
  if(buffer.size() < kEncapHeaderSize) {
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

RrResponse ParseRrResponse(const std::vector<uint8_t> &response) {
  RrResponse out;
  uint16_t command = 0;
  uint16_t length = 0;
  uint32_t session = 0;
  uint32_t status = 0;
  if(!ParseEncapHeader(response, &command, &length, &session, &status)) {
    return out;
  }

  out.encap_status = status;
  if(status != 0) {
    out.ok = true;
    return out;
  }
  if(response.size() < kEncapHeaderSize + length || length < 6) {
    return out;
  }

  const uint8_t *cpf = response.data() + kEncapHeaderSize + 6;
  const size_t cpf_size = length - 6;
  if(cpf_size < 10) {
    return out;
  }

  const uint16_t item_count = static_cast<uint16_t>(cpf[0] | (cpf[1] << 8));
  if(item_count < 2) {
    return out;
  }
  const uint16_t addr_len = static_cast<uint16_t>(cpf[4] | (cpf[5] << 8));
  const size_t data_off = 2U + 4U + addr_len;
  if(cpf_size < data_off + 4U) {
    return out;
  }
  const uint16_t data_type = static_cast<uint16_t>(cpf[data_off] | (cpf[data_off + 1] << 8));
  const uint16_t data_len = static_cast<uint16_t>(cpf[data_off + 2] | (cpf[data_off + 3] << 8));
  if(data_type != 0x00B1U && data_type != 0x00B2U) {
    return out;
  }
  const size_t data_start = data_off + 4U;
  if(cpf_size < data_start + data_len || data_len < 4) {
    return out;
  }

  const uint8_t *cip = cpf + data_start;
  out.cip_status = static_cast<int>(cip[2]);
  const size_t addl_count = cip[3];
  const size_t payload_offset = 4U + 2U * addl_count;
  if(data_len >= payload_offset) {
    out.payload.assign(cip + payload_offset, cip + data_len);
  }
  out.ok = true;
  return out;
}

bool RegisterSession(SOCKET sock, uint32_t *session_handle) {
  std::vector<uint8_t> payload;
  PushLe16(&payload, 1);
  PushLe16(&payload, 0);
  const auto packet = BuildEncap(kCmdRegisterSession, 0, payload);
  if(!SendAll(sock, packet)) {
    return false;
  }
  std::vector<uint8_t> response;
  if(!RecvSome(sock, &response)) {
    return false;
  }

  uint16_t command = 0;
  uint16_t length = 0;
  uint32_t session = 0;
  uint32_t status = 0;
  if(!ParseEncapHeader(response, &command, &length, &session, &status)) {
    return false;
  }
  if(command != kCmdRegisterSession || status != 0 || session == 0) {
    return false;
  }
  *session_handle = session;
  return true;
}

void UnregisterSession(SOCKET sock, uint32_t session_handle) {
  const auto packet = BuildEncap(kCmdUnregisterSession, session_handle, {});
  SendAll(sock, packet);
}

std::string HexBytes(const std::vector<uint8_t> &payload) {
  std::ostringstream oss;
  oss << std::hex << std::setfill('0');
  for(size_t i = 0; i < payload.size(); ++i) {
    if(i != 0) {
      oss << " ";
    }
    oss << std::setw(2) << static_cast<unsigned int>(payload[i]);
  }
  return oss.str();
}

std::string TimestampNow() {
  SYSTEMTIME st{};
  GetLocalTime(&st);
  std::ostringstream oss;
  oss << std::setfill('0')
      << std::setw(2) << st.wHour << ":"
      << std::setw(2) << st.wMinute << ":"
      << std::setw(2) << st.wSecond << "."
      << std::setw(3) << st.wMilliseconds;
  return oss.str();
}

}  // namespace

int main(int argc, char **argv) {
  Config cfg;
  if(!ParseArgs(argc, argv, &cfg)) {
    return (argc > 1 &&
            (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")) ? 0 : 2;
  }

  WSADATA wsa_data{};
  if(WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
    std::cerr << "WSAStartup failed\n";
    return 1;
  }

  const MMRESULT timer_result = timeBeginPeriod(static_cast<UINT>(cfg.timer_resolution_ms));
  if(timer_result != TIMERR_NOERROR) {
    std::cerr << "Warning: timeBeginPeriod failed.\n";
  }

  SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if(sock == INVALID_SOCKET) {
    std::cerr << "socket() failed\n";
    if(timer_result == TIMERR_NOERROR) {
      timeEndPeriod(static_cast<UINT>(cfg.timer_resolution_ms));
    }
    WSACleanup();
    return 1;
  }

  const DWORD timeout = static_cast<DWORD>(cfg.timeout_ms);
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&timeout),
             sizeof(timeout));
  setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char *>(&timeout),
             sizeof(timeout));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(cfg.port);
  if(inet_pton(AF_INET, cfg.host.c_str(), &addr.sin_addr) != 1) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    addrinfo *result = nullptr;
    if(getaddrinfo(cfg.host.c_str(), nullptr, &hints, &result) != 0 || result == nullptr) {
      std::cerr << "Failed to resolve host: " << cfg.host << "\n";
      closesocket(sock);
      if(result != nullptr) {
        freeaddrinfo(result);
      }
      if(timer_result == TIMERR_NOERROR) {
        timeEndPeriod(static_cast<UINT>(cfg.timer_resolution_ms));
      }
      WSACleanup();
      return 1;
    }
    addr.sin_addr = reinterpret_cast<sockaddr_in *>(result->ai_addr)->sin_addr;
    freeaddrinfo(result);
  }

  if(connect(sock, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
    std::cerr << "connect() failed\n";
    closesocket(sock);
    if(timer_result == TIMERR_NOERROR) {
      timeEndPeriod(static_cast<UINT>(cfg.timer_resolution_ms));
    }
    WSACleanup();
    return 1;
  }

  uint32_t session_handle = 0;
  if(!RegisterSession(sock, &session_handle)) {
    std::cerr << "RegisterSession failed\n";
    closesocket(sock);
    if(timer_result == TIMERR_NOERROR) {
      timeEndPeriod(static_cast<UINT>(cfg.timer_resolution_ms));
    }
    WSACleanup();
    return 1;
  }

  std::cout << "Monitoring " << cfg.host << ":" << cfg.port
            << " assembly " << static_cast<unsigned int>(cfg.assembly_instance)
            << " attr " << static_cast<unsigned int>(cfg.attribute)
            << " every " << cfg.interval_ms << " ms\n";
  if(cfg.duration_s > 0.0) {
    std::cout << "Duration: " << cfg.duration_s << " s\n";
  } else {
    std::cout << "Duration: until stopped (Ctrl+C)\n";
  }

  const auto cip_get = BuildGetAssemblyCip(cfg.assembly_instance, cfg.attribute);
  const auto rr_payload = BuildRrRequest(cip_get);
  const auto packet = BuildEncap(kCmdSendRrData, session_handle, rr_payload);

  const auto start = std::chrono::steady_clock::now();
  auto next_tick = start;
  auto next_report = start + std::chrono::seconds(1);
  const auto interval = std::chrono::microseconds(
      static_cast<int64_t>(cfg.interval_ms * 1000.0));
  const auto end_time = start + std::chrono::duration<double>(cfg.duration_s);

  uint64_t polls = 0;
  uint64_t ok_reads = 0;
  uint64_t errors = 0;
  std::vector<uint8_t> last_payload;
  bool have_last_payload = false;

  while(true) {
    const auto now = std::chrono::steady_clock::now();
    if(cfg.duration_s > 0.0 && now >= end_time) {
      break;
    }

    if(now < next_tick) {
      std::this_thread::sleep_until(next_tick);
    }
    next_tick += interval;
    polls += 1;

    if(!SendAll(sock, packet)) {
      errors += 1;
      std::cerr << "[" << TimestampNow() << "] send failed\n";
      break;
    }

    std::vector<uint8_t> response;
    if(!RecvSome(sock, &response)) {
      errors += 1;
      std::cerr << "[" << TimestampNow() << "] recv failed (timeout/reset)\n";
      break;
    }

    const RrResponse parsed = ParseRrResponse(response);
    if(!parsed.ok || parsed.encap_status != 0 || parsed.cip_status != 0) {
      errors += 1;
      std::cerr << "[" << TimestampNow() << "] protocol error"
                << " encap=" << parsed.encap_status
                << " cip=" << parsed.cip_status << "\n";
      continue;
    }

    ok_reads += 1;
    const bool changed = !have_last_payload || parsed.payload != last_payload;
    if(cfg.print_all || changed) {
      std::cout << "[" << TimestampNow() << "] "
                << "len=" << parsed.payload.size()
                << " data=" << HexBytes(parsed.payload) << "\n";
      last_payload = parsed.payload;
      have_last_payload = true;
    }

    if(now >= next_report) {
      const double elapsed_s = std::chrono::duration<double>(now - start).count();
      const double poll_rate = elapsed_s > 0.0 ? static_cast<double>(polls) / elapsed_s : 0.0;
      std::cout << "[" << TimestampNow() << "] "
                << "polls=" << polls
                << " ok=" << ok_reads
                << " errors=" << errors
                << " rate_hz=" << std::fixed << std::setprecision(1) << poll_rate
                << "\n";
      next_report += std::chrono::seconds(1);
    }
  }

  UnregisterSession(sock, session_handle);
  closesocket(sock);
  if(timer_result == TIMERR_NOERROR) {
    timeEndPeriod(static_cast<UINT>(cfg.timer_resolution_ms));
  }
  WSACleanup();

  std::cout << "Done: polls=" << polls
            << " ok=" << ok_reads
            << " errors=" << errors << "\n";
  return errors == 0 ? 0 : 1;
}

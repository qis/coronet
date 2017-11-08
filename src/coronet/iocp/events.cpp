#include <coronet/events.h>
#include <coronet/iocp/event.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <array>
#include <cstdio>

namespace coronet {
namespace {

class library {
public:
  library() noexcept {
    WSADATA wsadata = {};
    WSAStartup(MAKEWORD(2, 2), &wsadata);
  }

  ~library() {
    WSACleanup();
    if (IsDebuggerPresent()) {
      std::puts("Press ENTER or CTRL+C to exit . . .");
      std::getchar();
    }
  }
};

}  // namespace

std::error_code events::create() noexcept {
  // Initialize windows sockets.
  static library library;

  // Create a new completion port.
  events events(CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0));
  if (!events) {
    return { static_cast<int>(GetLastError()), error_category() };
  }

  // Replace current completion port.
  if (const auto ec = close()) {
    return ec;
  }
  *this = std::move(events);
  return {};
}

std::error_code events::run(int processor) {
  if (!valid()) {
    return { static_cast<int>(std::errc::bad_file_descriptor), error_category() };
  }

  // Handle completed events.
  std::error_code ec;
  std::array<OVERLAPPED_ENTRY, 1024> events;
  const auto handle = as<HANDLE>();
  const auto events_data = events.data();
  const auto events_size = static_cast<ULONG>(events.size());
  while (true) {
    ULONG count = 0;
    if (!GetQueuedCompletionStatusEx(handle, events_data, events_size, &count, INFINITE, FALSE)) {
      if (const auto code = GetLastError(); code != ERROR_ABANDONED_WAIT_0) {
        ec = { static_cast<int>(code), error_category() };
      }
      break;
    }
    for (ULONG i = 0; i < count; i++) {
      auto& ev = events[i];
      if (ev.lpOverlapped) {
        auto& handler = *static_cast<event*>(ev.lpOverlapped);
        handler(ev.dwNumberOfBytesTransferred);
      }
    }
  }
  return ec;
}

std::error_code events::close() noexcept {
  if (valid()) {
    if (!CloseHandle(as<HANDLE>())) {
      return { static_cast<int>(GetLastError()), error_category() };
    }
    handle_ = invalid_handle_value;
  }
  return {};
}

}  // namespace coronet

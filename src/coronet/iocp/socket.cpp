#include <coronet/socket.h>
#include <coronet/address.h>
#include <coronet/iocp/event.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>

#include <iostream>

namespace coronet {

std::error_code socket::create(family family, type type, int protocol) noexcept {
  if (!events_.get()) {
    return { static_cast<int>(std::errc::bad_file_descriptor), error_category() };
  }

  // Create a new socket.
  const auto handle = WSASocket(to_int(family), to_int(type), protocol, nullptr, 0, WSA_FLAG_OVERLAPPED);
  if (handle == INVALID_SOCKET) {
    return { WSAGetLastError(), error_category() };
  }
  socket socket(events_, handle);

  // Replace current socket.
  if (const auto ec = close()) {
    return ec;
  }
  *this = std::move(socket);
  return {};
}

std::error_code socket::set(option option, bool enable) noexcept {
  auto sockopt = 0;
  switch (option) {
  case option::nodelay: sockopt = TCP_NODELAY; break;
  }
  BOOL value = enable ? TRUE : FALSE;
  auto value_data = reinterpret_cast<const char*>(&value);
  auto value_size = static_cast<int>(sizeof(value));
  if (::setsockopt(as<SOCKET>(), SOL_SOCKET, sockopt, value_data, value_size) == SOCKET_ERROR) {
    std::cout << "socket: " << handle_ << ' ' << reinterpret_cast<std::intptr_t>(this) << std::endl;
    return { WSAGetLastError(), error_category() };
  }
  return {};
}

// clang-format off

async_generator<std::string_view> socket::recv(void* data, std::size_t size) noexcept {
  ec_.clear();
  WSABUF buffer = {};
  buffer.buf = reinterpret_cast<decltype(buffer.buf)>(data);
  buffer.len = static_cast<decltype(buffer.len)>(size);
  while (true) {
    DWORD bytes = 0;
    DWORD flags = 0;
    event event;
    if (WSARecv(as<SOCKET>(), &buffer, 1, &bytes, &flags, &event, nullptr) == SOCKET_ERROR) {
      if (const auto code = WSAGetLastError(); code != ERROR_IO_PENDING) {
        ec_ = { code, error_category() };
        break;
      }
    }
    bytes = co_await event;
    WSAGetOverlappedResult(as<SOCKET>(), &event, &bytes, FALSE, &flags);
    if (const auto code = WSAGetLastError()) {
      ec_ = { code, error_category() };
      break;
    }
    if (!bytes) {
      ec_ = { static_cast<int>(errc::eof), error_category() };
      break;
    }
    std::string_view result(reinterpret_cast<const char*>(data), bytes);
    co_yield result;
  }
  co_return;
}

async<std::error_code> socket::send(std::string_view message) noexcept {
  WSABUF data = {};
  data.buf = reinterpret_cast<decltype(data.buf)>(const_cast<char*>(message.data()));
  data.len = static_cast<decltype(data.len)>(message.size());
  while (data.len > 0) {
    DWORD bytes = 0;
    event event;
    if (WSASend(as<SOCKET>(), &data, 1, &bytes, 0, &event, nullptr) == SOCKET_ERROR) {
      if (const auto code = WSAGetLastError(); code != ERROR_IO_PENDING) {
        co_return { code, error_category() };
      }
    }
    bytes = co_await event;
    DWORD flags = 0;
    WSAGetOverlappedResult(as<SOCKET>(), &event, &bytes, FALSE, &flags);
    if (const auto code = WSAGetLastError()) {
      co_return { code, error_category() };
    }
    if (!bytes) {
      co_return { static_cast<int>(errc::eof), error_category() };
    }
    data.buf += bytes;
    data.len -= bytes > data.len ? data.len : bytes;
  }
  co_return {};
}

// clang-format on

std::error_code socket::close() noexcept {
  if (valid()) {
    ::shutdown(as<SOCKET>(), SD_BOTH);
    if (::closesocket(as<SOCKET>()) == SOCKET_ERROR) {
      return { WSAGetLastError(), error_category() };
    }
    handle_ = invalid_handle_value;
  }
  return {};
}

}  // namespace coronet

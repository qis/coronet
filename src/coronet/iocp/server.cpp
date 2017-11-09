#include <coronet/server.h>
#include <coronet/address.h>
#include <coronet/iocp/event.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <array>

namespace coronet {

std::error_code server::create(const std::string& host, const std::string& port, type type) noexcept {
  if (!events_.get()) {
    return { static_cast<int>(std::errc::bad_file_descriptor), error_category() };
  }

  // Convert host and port to socket address and options.
  address address;
  if (const auto ec = address.create(host, port, type, AI_PASSIVE)) {
    return ec;
  }

  // Create listening socket.
  server server(events_);
  if (const auto ec = static_cast<socket&>(server).create(address.family(), address.type(), address.protocol())) {
    return ec;
  }

  // Set SO_REUSEADDR socket option.
  BOOL reuseaddr = TRUE;
  const auto reuseaddr_data = reinterpret_cast<const char*>(&reuseaddr);
  const auto reuseaddr_size = static_cast<int>(sizeof(reuseaddr));
  if (::setsockopt(server.as<SOCKET>(), SOL_SOCKET, SO_REUSEADDR, reuseaddr_data, reuseaddr_size) == SOCKET_ERROR) {
    return { WSAGetLastError(), error_category() };
  }

  // Bind listening socket to the given address.
  if (::bind(server.as<SOCKET>(), address.addr(), static_cast<int>(address.addrlen())) == SOCKET_ERROR) {
    return { WSAGetLastError(), error_category() };
  }

  // Create a new completion port for the listening socket.
  if (!CreateIoCompletionPort(server.as<HANDLE>(), events_.get().as<HANDLE>(), 0, 0)) {
    return { static_cast<int>(GetLastError()), error_category() };
  }

  // Replace current socket.
  if (const auto ec = close()) {
    return ec;
  }
  *this = std::move(server);
  protocol_ = address.protocol();
  family_ = address.family();
  type_ = address.type();
  return {};
}

async_generator<socket> server::accept(std::size_t backlog) noexcept {
  constexpr DWORD salen = sizeof(struct sockaddr_storage) + 16;
  ec_.clear();

  // Start listening on the socket.
  if (::listen(as<SOCKET>(), backlog > 0 ? static_cast<int>(backlog) : SOMAXCONN) == SOCKET_ERROR) {
    ec_ = { WSAGetLastError(), error_category() };
    co_return;
  }

  // Accept connections.
  std::array<char, salen * 2> buffer;
  while (true) {
    // Create a socket that will receive the accepted connection.
    socket socket(events_);
    if (const auto ec = socket.create(family_, type_, protocol_)) {
      ec_ = ec;
      co_return;
    }

    // Create a new completion port for the accepted connection socket.
    if (!CreateIoCompletionPort(socket.as<HANDLE>(), events_.get().as<HANDLE>(), 0, 0)) {
      ec_ = { static_cast<int>(GetLastError()), error_category() };
      co_return;
    }

    // Accept connection.
    DWORD bytes = 0;
    DWORD flags = 0;
    event event;
    if (!AcceptEx(as<SOCKET>(), socket.as<SOCKET>(), buffer.data(), 0, salen, salen, &bytes, &event)) {
      if (const auto code = WSAGetLastError(); code != ERROR_IO_PENDING) {
        ec_ = { code, error_category() };
        co_return;
      }
    }
    bytes = co_await event;
    WSAGetOverlappedResult(as<SOCKET>(), &event, &bytes, FALSE, &flags);
    if (const auto code = WSAGetLastError()) {
      ec_ = { code, error_category() };
      co_return;
    }
    co_yield socket;
  }
  co_return;
}

}  // namespace coronet

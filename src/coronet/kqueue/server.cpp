#include <coronet/server.h>
#include <coronet/address.h>
#include <coronet/kqueue/event.h>
#include <sys/types.h>
#include <sys/socket.h>

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
  auto reuseaddr = 1;
  if (::setsockopt(server.value(), SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr)) < 0) {
    return { errno, error_category() };
  }

  // Bind listening socket to the given address.
  if (::bind(server.value(), address.addr(), address.addrlen()) < 0) {
    return { errno, error_category() };
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
  ec_.clear();

  // Start listening on the socket.
  if (::listen(handle_, backlog > 0 ? static_cast<int>(backlog) : SOMAXCONN) < 0) {
    ec_ = { errno, error_category() };
    co_return;
  }

  // Accept connections.
  struct sockaddr_storage storage;
  auto addr = reinterpret_cast<struct sockaddr*>(&storage);
  while (true) {
    auto socklen = static_cast<socklen_t>(sizeof(storage));
    socket socket(events_, ::accept4(handle_, addr, &socklen, SOCK_NONBLOCK));
    if (!socket) {
      if (errno != EAGAIN) {
        ec_ = { errno, error_category() };
        co_return;
      }
      const auto available = co_await queue(events_.get().value(), handle_, EVFILT_READ);
      if (available < 0) {
        ec_ = { static_cast<int>(errc::cancelled), error_category() };
        co_return;
      }
      if (available == 0) {
        ec_ = { static_cast<int>(errc::eof), error_category() };
        co_return;
      }
      socket.reset(::accept4(handle_, addr, &socklen, SOCK_NONBLOCK));
      if (!socket) {
        ec_ = { static_cast<int>(errc::eof), error_category() };
        co_return;
      }
    }
    co_yield socket;
  }
  co_return;
}

}  // namespace coronet

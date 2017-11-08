#pragma once
#include <coronet/async.h>
#include <coronet/events.h>
#include <coronet/error.h>
#include <functional>
#include <string_view>

namespace coronet {

enum class family {
  ipv4,
  ipv6,
};

enum class type {
  tcp,
  udp,
};

class socket : public handle<socket> {
public:
  explicit socket(events& events) noexcept : events_(events) {
  }

  explicit socket(events& events, handle_type value) noexcept : handle(value), events_(events) {
  }

  // Creates socket.
  std::error_code create(family family, type type, int protocol = 0) noexcept;

  // Reads data from socket.
  // Completes range on closed connection.
  // Sets ec_ and completes range on error.
  async_generator<std::string_view> recv(void* data, std::size_t size) noexcept;

  // Writes message to the socket.
  async<std::error_code> send(std::string_view message) noexcept;

  // Returns the last error set by recv(void*, std::size_t).
  std::error_code ec() const noexcept {
    return ec_;
  }

  // Closes socket.
  std::error_code close() noexcept;

protected:
  std::error_code ec_;
  std::reference_wrapper<events> events_;
};

}  // namespace coronet

#pragma once
#include <coronet/socket.h>
#include <string>

namespace coronet {

class server final : public socket {
public:
  using socket::socket;

  // Creates and binds server socket.
  std::error_code create(const std::string& host, const std::string& port, type type) noexcept;

  // Accepts client connections.
  // Completes range and sets ec_ on error. Ignores connection errors.
  async_generator<socket> accept(std::size_t backlog = 0) noexcept;

  // Stops accepting client connections.
  std::error_code stop() noexcept {
    return close();
  }

  // Returns the last error set by accept(std::size_t).
  std::error_code ec() const noexcept {
    return ec_;
  }

private:
  std::error_code ec_;
  family family_ = family::ipv4;
  type type_ = type::tcp;
  int protocol_ = 0;
};

}  // namespace coronet

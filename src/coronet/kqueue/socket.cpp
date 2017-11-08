#include <coronet/socket.h>
#include <coronet/address.h>
#include <coronet/kqueue/event.h>
#include <sys/socket.h>
#include <unistd.h>

namespace coronet {

std::error_code socket::create(family family, type type, int protocol) noexcept {
  if (!events_.get()) {
    return { static_cast<int>(std::errc::bad_file_descriptor), error_category() };
  }

  // Create a new socket.
  socket socket(events_, ::socket(to_int(family), to_int(type) | SOCK_NONBLOCK, protocol));
  if (!socket) {
    return { errno, error_category() };
  }

  // Replace current socket.
  if (const auto ec = close()) {
    return ec;
  }
  *this = std::move(socket);
  return {};
}

// clang-format off

async_generator<std::string_view> socket::recv(void* data, std::size_t size) noexcept {
  ec_.clear();
  while (true) {
    std::int64_t rv = ::read(handle_, data, size);
    if (rv < 0) {
      if (errno != EAGAIN) {
        ec_ = { errno, error_category() };
        break;
      }
      const auto available = co_await queue(events_.get().value(), handle_, EVFILT_READ);
      if (available < 0) {
        ec_ = { static_cast<int>(errc::cancelled), error_category() };
        break;
      }
      if (available == 0) {
        ec_ = { static_cast<int>(errc::eof), error_category() };
        break;
      }
      continue;
    }
    if (rv == 0) {
      ec_ = { static_cast<int>(errc::eof), error_category() };
      break;
    }
    std::string_view result(reinterpret_cast<const char*>(data), static_cast<std::size_t>(rv));
    co_yield result;
  }
  co_return;
}

async<std::error_code> socket::send(std::string_view message) noexcept {
  auto data = message.data();
  auto size = message.size();
  while (size > 0) {
    std::int64_t rv = ::write(handle_, data, size);
    if (rv < 0) {
      if (errno != EAGAIN) {
        co_return { errno, error_category() };
      }
      const auto available = co_await queue(events_.get().value(), handle_, EVFILT_WRITE);
      if (available < 0) {
        co_return { static_cast<int>(errc::cancelled), error_category() };
      }
      if (available == 0) {
        co_return{ static_cast<int>(errc::eof), error_category() };
      }
      continue;
    }
    if (rv == 0) {
      co_return { static_cast<int>(errc::eof), error_category() };
    }
    const auto bytes = static_cast<std::size_t>(rv);
    data += bytes;
    size -= bytes > size ? size : bytes;
  }
  co_return {};
}

// clang-format on

std::error_code socket::close() noexcept {
  if (valid()) {
    ::shutdown(handle_, SHUT_RDWR);
    if (::close(handle_) < 0) {
      return { errno, error_category() };
    }
    handle_ = invalid_handle_value;
  }
  return {};
}

}  // namespace coronet
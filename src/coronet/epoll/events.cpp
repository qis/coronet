#include <coronet/events.h>
#include <coronet/epoll/event.h>
#include <unistd.h>
#include <array>
#include <cerrno>

namespace coronet {

std::error_code events::create() noexcept {
  // Create a new kqueue handle.
  events events(epoll_create1(0));
  if (!events) {
    return { errno, error_category() };
  }

  // Replace current kqueue handle.
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
  std::array<epoll_event, 32> events;
  const auto events_data = events.data();
  const auto events_size = events.size();
  while (true) {
    const auto count = ::epoll_wait(handle_, events_data, events_size, -1);
    if (count < 0) {
      if (errno != EINTR) {
        ec = { errno, error_category() };
      }
      break;
    }
    for (std::size_t i = 0, max = static_cast<std::size_t>(count); i < max; i++) {
      const auto& ev = events[i];
      if (ev.data.ptr) {
        auto& handler = *static_cast<event*>(ev.data.ptr);
        handler();
      }
    }
  }
  return ec;
}

std::error_code events::close() noexcept {
  if (valid()) {
    if (::close(handle_) < 0) {
      return { errno, error_category() };
    }
    handle_ = invalid_handle_value;
  }
  return {};
}

}  // namespace coronet

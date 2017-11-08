#pragma once
#include <sys/event.h>
#include <experimental/coroutine>
#include <cstddef>

namespace coronet {

class event {
public:
  using handle_type = std::experimental::coroutine_handle<>;

  event(int events, int socket, short filter) noexcept : events_(events) {
    EV_SET(&ev_, static_cast<uintptr_t>(socket), filter, EV_ADD | EV_ONESHOT, 0, 0, this);
  }

  event(event&& event) = delete;
  event(const event& other) = delete;

  event& operator=(event&& other) = delete;
  event& operator=(const event& other) = delete;

  constexpr bool await_ready() noexcept {
    return false;
  }

  void await_suspend(handle_type handle) noexcept {
    handle_ = handle;
    ::kevent(events_, &ev_, 1, nullptr, 0, nullptr);
  }

  constexpr auto await_resume() noexcept {
    return result_;
  }

  void operator()(std::int64_t result) noexcept {
    result_ = result;
    handle_.resume();
  }

private:
  std::int64_t result_ = 0;
  handle_type handle_ = nullptr;
  struct kevent ev_ = {};
  int events_ = -1;
};

inline event queue(int events, int socket, short filter) noexcept {
  return { events, socket, filter };
}

}  // namespace coronet

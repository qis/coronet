#pragma once
#include <sys/event.h>
#include <experimental/coroutine>
#include <cstddef>

namespace coronet {

class event final : public kevent {
public:
  using handle_type = std::experimental::coroutine_handle<>;

  event(int events, int socket, short filter) noexcept : kevent({}), events_(events) {
    const auto ev = static_cast<struct ::kevent*>(this);
    const auto ident = static_cast<uintptr_t>(socket);
    EV_SET(ev, ident, filter, EV_ADD | EV_ONESHOT, 0, 0, this);
  }

  event(event&& event) = delete;
  event& operator=(event&& other) = delete;

  ~event() = default;

  constexpr bool await_ready() noexcept {
    return false;
  }

  void await_suspend(handle_type handle) noexcept {
    handle_ = handle;
    ::kevent(events_, this, 1, nullptr, 0, nullptr);
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
  int events_ = -1;
};

}  // namespace coronet

#pragma once
#include <sys/epoll.h>
#include <experimental/coroutine>
#include <cstddef>

namespace coronet {

class event final : public epoll_event {
public:
  using handle_type = std::experimental::coroutine_handle<>;

  event(int events, int socket, uint32_t filter) noexcept : epoll_event({}), events_(events), socket_(socket) {
    const auto ev = static_cast<struct ::epoll_event*>(this);
    ev->events = filter;
    ev->data.ptr = this;
  }

  event(event&& event) = delete;
  event& operator=(event&& other) = delete;

  ~event() = default;

  constexpr bool await_ready() noexcept {
    return false;
  }

  void await_suspend(handle_type handle) noexcept {
    handle_ = handle;
    ::epoll_ctl(events_, EPOLL_CTL_ADD, socket_, this);
  }

  constexpr void await_resume() noexcept {
  }

  void operator()() noexcept {
    ::epoll_ctl(events_, EPOLL_CTL_DEL, socket_, this);
    handle_.resume();
  }

private:
  handle_type handle_ = nullptr;
  int events_ = -1;
  int socket_ = -1;
};

}  // namespace coronet

#pragma once
#include <windows.h>
#include <experimental/coroutine>

namespace coronet {

class event final : public OVERLAPPED {
public:
  using handle_type = std::experimental::coroutine_handle<>;

  event() noexcept : OVERLAPPED({}) {
  }

  event(event&& other) = delete;
  event(const event& other) = delete;

  event& operator=(event&& other) = delete;
  event& operator=(const event& other) = delete;

  constexpr bool await_ready() noexcept {
    return ready_;
  }

  void await_suspend(handle_type handle) noexcept {
    if (ready_) {
      handle.resume();
    } else {
      handle_ = handle;
    }
  }

  constexpr auto await_resume() noexcept {
    return size_;
  }

  void operator()(DWORD size) noexcept {
    size_ = size;
    ready_ = true;
    if (auto handle = std::exchange(handle_, nullptr)) {
      handle.resume();
    }
  }

private:
  DWORD size_ = 0;
  bool ready_ = false;
  handle_type handle_ = nullptr;
};

}  // namespace coronet

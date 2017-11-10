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
    handle_ = handle;
  }

  constexpr auto await_resume() noexcept {
    return result_;
  }

  void operator()(DWORD size) noexcept {
    result_ = size;
    ready_ = true;
    if (auto handle = std::exchange(handle_, nullptr)) {
      handle.resume();
    }
  }

  void reset() noexcept {
    static_cast<OVERLAPPED&>(*this) = {};
    ready_ = false;
    result_ = 0;
  }

private:
  bool ready_ = false;
  DWORD result_ = 0;
  handle_type handle_ = nullptr;
};

}  // namespace coronet

#pragma once
#include <cppcoro/generator.hpp>
#include <cppcoro/async_generator.hpp>
#include <experimental/coroutine>
#include <optional>
#include <utility>

namespace coronet {

using cppcoro::generator;
using cppcoro::async_generator;

using std::experimental::coroutine_handle;
using std::experimental::suspend_always;
using std::experimental::suspend_never;

class task {
public:
  struct promise_type {
    task get_return_object() noexcept {
      return {};
    }

    constexpr auto initial_suspend() noexcept {
      return suspend_never{};
    }

    constexpr auto final_suspend() noexcept {
      return suspend_never{};
    }

    constexpr void return_void() noexcept {
    }

    void unhandled_exception() noexcept {
      std::abort();
    }
  };
};

template <typename T>
class async {
public:
  struct promise_type {
    async get_return_object() noexcept {
      return { *this };
    }

    constexpr auto initial_suspend() noexcept {
      return suspend_never{};
    }

    constexpr auto final_suspend() noexcept {
      return suspend_never{};
    }

    void return_value(T value) noexcept {
      value_ = std::move(value);
      if (auto handle = std::exchange(handle_, nullptr)) {
        handle.resume();
      }
    }

    void unhandled_exception() noexcept {
      std::abort();
    }

    std::optional<T> value_;
    coroutine_handle<> handle_ = nullptr;
  };

  using handle_type = coroutine_handle<promise_type>;

  async(promise_type& promise) noexcept : handle_(handle_type::from_promise(promise)) {
  }

  async(async&& other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {
  }

  async& operator=(async&& other) noexcept {
    if (&other != this) {
      handle_ = std::exchange(other.handle_, nullptr);
    }
  }

  ~async() = default;

  bool await_ready() noexcept {
    return !handle_ || handle_.promise().value_;
  }

  void await_suspend(coroutine_handle<> handle) noexcept {
    handle_.promise().handle_ = handle;
  }

  auto& await_resume() noexcept {
    return *handle_.promise().value_;
  }

private:
  coroutine_handle<promise_type> handle_ = nullptr;
};

}  // namespace coronet

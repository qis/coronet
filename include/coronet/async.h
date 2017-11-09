#pragma once
#include <experimental/coroutine>
#include <optional>
#include <utility>

namespace coronet {

using std::experimental::coroutine_handle;
using std::experimental::suspend_always;
using std::experimental::suspend_never;

// ================================================================================================

class task {
public:
  struct promise_type {
    task get_return_object() noexcept {
      return { *this };
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

  using handle_type = coroutine_handle<promise_type>;

  task(promise_type& promise) noexcept : handle_(handle_type::from_promise(promise)) {
  }

private:
  handle_type handle_ = nullptr;
};

// ================================================================================================

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
      if (consumer_) {
        // Resume the consumer if it is suspended.
        consumer_.resume();
      }
    }

    void unhandled_exception() noexcept {
      std::abort();
    }

    std::optional<T> value_;
    coroutine_handle<> consumer_ = nullptr;
  };

  using handle_type = coroutine_handle<promise_type>;

  async(promise_type& promise) noexcept : handle_(handle_type::from_promise(promise)) {
  }

  bool await_ready() noexcept {
    return !!handle_.promise().value_;
  }

  void await_suspend(coroutine_handle<> consumer) noexcept {
    handle_.promise().consumer_ = consumer;
  }

  auto& await_resume() noexcept {
    return *handle_.promise().value_;
  }

private:
  coroutine_handle<promise_type> handle_ = nullptr;
};

// ================================================================================================

template <typename T>
class generator {
public:
  struct promise_type {
    generator get_return_object() noexcept {
      return { *this };
    }

    constexpr auto initial_suspend() noexcept {
      return suspend_never{};
    }

    constexpr auto final_suspend() noexcept {
      return suspend_always{};
    }

    constexpr auto return_void() noexcept {
      return suspend_never{};
    }

    auto yield_value(T& value) noexcept {
      value_ = std::addressof(value);
      return suspend_always{};
    }

    void unhandled_exception() noexcept {
      std::abort();
    }

    T* value_ = nullptr;
  };

  using handle_type = coroutine_handle<promise_type>;

  struct iterator {
    using iterator_category = std::input_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = T;
    using pointer = T*;
    using reference = T&;

    iterator() noexcept = default;

    iterator(handle_type handle) noexcept : handle_(handle) {
    }

    iterator& operator++() noexcept {
      handle_.promise().value_ = nullptr;
      handle_.resume();
      if (handle_.done()) {
        handle_ = nullptr;
      }
      return *this;
    }

    iterator operator++(int) = delete;

    bool operator==(const iterator& other) const noexcept {
      return handle_ == other.handle_;
    }

    bool operator!=(const iterator& other) const noexcept {
      return handle_ != other.handle_;
    }

    reference operator*() noexcept {
      return *handle_.promise().value_;
    }

    pointer operator->() noexcept {
      return handle_.promise().value_;
    }

    handle_type handle_ = nullptr;
  };

  generator(promise_type& promise) noexcept : handle_(handle_type::from_promise(promise)) {
  }

  generator(generator&& other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {
  }

  generator& operator=(generator&& other) noexcept {
    handle_ = std::exchange(other.handle_, nullptr);
    return *this;
  }

  ~generator() {
    if (handle_) {
      handle_.destroy();
    }
  }

  iterator begin() noexcept {
    return { handle_ };
  }

  iterator end() noexcept {
    return {};
  }

private:
  handle_type handle_ = nullptr;
};

// ================================================================================================

template <typename T>
class async_generator {
public:
  struct promise_type {
    async_generator get_return_object() noexcept {
      return { *this };
    }

    constexpr auto initial_suspend() noexcept {
      return suspend_never{};
    }

    constexpr auto final_suspend() noexcept {
      return suspend_always{};
    }

    constexpr auto return_void() noexcept {
      return suspend_never{};
    }

    auto yield_value(T& value) noexcept {
      value_ = std::addressof(value);
      if (auto consumer = std::exchange(consumer_, nullptr)) {
        consumer.resume();
      }
      return suspend_always{};
    }

    void unhandled_exception() noexcept {
      std::abort();
    }

    T* value_ = nullptr;
    coroutine_handle<> consumer_ = nullptr;
  };

  using handle_type = coroutine_handle<promise_type>;

  struct iterator {
    using iterator_category = std::input_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = T;
    using pointer = T*;
    using reference = T&;

    iterator() noexcept = default;

    iterator(handle_type handle) noexcept : handle_(handle) {
    }

    iterator& operator++() noexcept {
      handle_.promise().value_ = nullptr;
      if (handle_.done()) {
        handle_ = nullptr;
      }
      return *this;
    }

    iterator operator++(int) = delete;

    bool operator==(const iterator& other) const noexcept {
      return handle_ == other.handle_;
    }

    bool operator!=(const iterator& other) const noexcept {
      return handle_ != other.handle_;
    }

    reference operator*() noexcept {
      return *handle_.promise().value_;
    }

    pointer operator->() noexcept {
      return handle_.promise().value_;
    }

    bool await_ready() noexcept {
      return !handle_ || handle_.promise().value_;
    }

    void await_suspend(coroutine_handle<> consumer) noexcept {
      handle_.promise().consumer_ = consumer;
      handle_.resume();
    }

    iterator await_resume() noexcept {
      return { handle_ };
    }

    handle_type handle_ = nullptr;
  };

  async_generator(promise_type& promise) noexcept : handle_(handle_type::from_promise(promise)) {
  }

  async_generator(async_generator&& other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {
  }

  async_generator& operator=(async_generator&& other) noexcept {
    handle_ = std::exchange(other.handle_, nullptr);
    return *this;
  }

  ~async_generator() {
    if (handle_) {
      handle_.destroy();
    }
  }

  iterator begin() noexcept {
    return { handle_ };
  }

  iterator end() noexcept {
    return {};
  }

private:
  handle_type handle_ = nullptr;
};

}  // namespace coronet

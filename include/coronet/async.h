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

    constexpr void return_void() noexcept {
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
    if (&other != this) {
      handle_ = std::exchange(other.handle_, nullptr);
    }
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

template <typename T, typename GeneratorPromise>
struct await_iterator;

template <typename T, typename GeneratorPromise>
struct async_iterator;

template <typename T, typename GeneratorPromise>
struct await_consumer;

template <typename T>
struct async_generator {
  struct promise_type {
    async_generator get_return_object() noexcept {
      return { *this };
    }

    constexpr auto initial_suspend() noexcept {
      return suspend_always{};
    }

    constexpr auto final_suspend() noexcept {
      return suspend_never{};
    }

    await_consumer<T, promise_type> yield_value(T& value) noexcept {
      value_ = std::addressof(value);
      return { coroutine_handle<promise_type>::from_promise(*this) };
    }

    void return_void() noexcept {
      value_ = nullptr;
      if (auto await_iterator_handle = std::exchange(await_iterator_handle_, nullptr)) {
        await_iterator_handle.resume();
      }
    }

    void unhandled_exception() noexcept {
      std::abort();
    }

    coroutine_handle<> await_iterator_handle_ = nullptr;
    coroutine_handle<> await_consumer_handle_ = nullptr;
    T* value_ = nullptr;
  };

  async_generator() noexcept = default;

  async_generator(promise_type& promise) noexcept : handle_(coroutine_handle<promise_type>::from_promise(promise)) {
  }

  async_generator(async_generator&& other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {
  }

  async_generator& operator=(async_generator&& other) noexcept {
    if (&other != this) {
      handle_ = std::exchange(other.handle_, nullptr);
    }
    return *this;
  }

  ~async_generator() = default;

  await_iterator<T, promise_type> begin() noexcept {
    return { handle_ };
  }

  async_iterator<T, promise_type> end() noexcept {
    return {};
  }

private:
  coroutine_handle<promise_type> handle_ = nullptr;
};

template <typename T, typename GeneratorPromise>
struct await_consumer {
  await_consumer() noexcept = default;

  await_consumer(coroutine_handle<GeneratorPromise> handle) noexcept : generator_handle_(handle) {
  }

  await_consumer(await_consumer&& other) noexcept : generator_handle_(std::exchange(other.generator_handle_, nullptr)) {
  }

  await_consumer& operator=(await_consumer&& other) noexcept {
    if (&other != this) {
      generator_handle_ = std::exchange(other.generator_handle_, nullptr);
    }
    return *this;
  }

  ~await_consumer() = default;

  constexpr bool await_ready() noexcept {
    return false;
  }

  void await_suspend(coroutine_handle<> await_consumer_handle) noexcept {
    generator_handle_.promise().await_consumer_handle_ = await_consumer_handle;
    if (auto await_iterator_handle = std::exchange(generator_handle_.promise().await_iterator_handle_, nullptr)) {
      await_iterator_handle.resume();
    }
  }

  constexpr void await_resume() noexcept {
  }

  coroutine_handle<GeneratorPromise> generator_handle_ = nullptr;
};

template <typename T, typename GeneratorPromise>
struct await_iterator {
  await_iterator() noexcept = default;

  await_iterator(coroutine_handle<GeneratorPromise> handle) noexcept : generator_handle_(handle) {
  }

  await_iterator(async_iterator<T, GeneratorPromise>* iterator) noexcept :
    generator_handle_(iterator->generator_handle_), iterator_(iterator) {
  }

  await_iterator(await_iterator&& other) noexcept :
    generator_handle_(std::exchange(other.generator_handle_, nullptr)),
    iterator_(std::exchange(other.iterator_, nullptr)) {
  }

  await_iterator& operator=(await_iterator&& other) noexcept {
    if (&other != this) {
      generator_handle_ = std::exchange(other.generator_handle_, nullptr);
      iterator_ = std::exchange(other.iterator_, nullptr);
    }
    return *this;
  }

  ~await_iterator() = default;

  constexpr bool await_ready() noexcept {
    return false;
  }

  void await_suspend(coroutine_handle<> await_iterator_handle) noexcept {
    generator_handle_.promise().await_iterator_handle_ = await_iterator_handle;
    if (auto await_consumer_handle = std::exchange(generator_handle_.promise().await_consumer_handle_, nullptr)) {
      generator_handle_.promise().value_ = nullptr;
      await_consumer_handle.resume();
    } else {
      generator_handle_.resume();
    }
  }

  async_iterator<T, GeneratorPromise> await_resume() noexcept {
    if (generator_handle_.done() || !generator_handle_.promise().value_) {
      generator_handle_ = nullptr;
    }
    if (iterator_) {
      iterator_->generator_handle_ = generator_handle_;
      return { *iterator_ };
    }
    return { generator_handle_ };
  }

  coroutine_handle<GeneratorPromise> generator_handle_ = nullptr;
  async_iterator<T, GeneratorPromise>* iterator_ = nullptr;
};

template <typename T, typename GeneratorPromise>
struct async_iterator {
  using iterator_category = std::input_iterator_tag;
  using difference_type = std::ptrdiff_t;
  using value_type = T;
  using pointer = T*;
  using reference = T&;

  async_iterator() noexcept = default;

  async_iterator(coroutine_handle<GeneratorPromise> handle) noexcept : generator_handle_(handle) {
  }

  await_iterator<T, GeneratorPromise> operator++() noexcept {
    return { this };
  }

  async_iterator operator++(int) = delete;

  bool operator==(const async_iterator& other) const noexcept {
    return generator_handle_ == other.generator_handle_;
  }

  bool operator!=(const async_iterator& other) const noexcept {
    return generator_handle_ != other.generator_handle_;
  }

  T& operator*() const {
    return *generator_handle_.promise().value_;
  }

  T* operator->() const {
    return generator_handle_.promise().value_;
  }

  coroutine_handle<GeneratorPromise> generator_handle_ = nullptr;
};

}  // namespace coronet

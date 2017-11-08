#pragma once
#include <experimental/coroutine>
#include <optional>
#include <utility>
#include <cstdio>

namespace coronet {

#if 0
#define print(x) puts(x)
#else
#define print(x)
#endif

using std::experimental::coroutine_handle;
using std::experimental::suspend_always;
using std::experimental::suspend_never;

class task {
public:
  struct promise_type {
    promise_type& get_return_object() {
      print("promise_type& task::promise_type::get_return_object()");
      return *this;
    }

    suspend_never initial_suspend() {
      print("suspend_never task::promise_type::initial_suspend()");
      return {};
    }

    suspend_never final_suspend() {
      print("suspend_never task::promise_type::final_suspend()");
      return {};
    }

    void return_void() {
      print("void task::promise_type::return_void()");
    }

    void unhandled_exception() {
      std::abort();
    }

    promise_type() {
      print("task::promise_type::promise_type()");
    }

    ~promise_type() {
      print("task::promise_type::~promise_type()");
    }
  };

  task(promise_type& promise) {
    print("task::task(promise_type& promise)");
  }

  ~task() {
    print("task::~task()");  // the promise remains until the coroutine returns
  }
};

template <typename T>
class async {
public:
  struct promise_type {
    promise_type& get_return_object() {
      print("promise_type& async::promise_type::get_return_object()");
      return *this;
    }

    suspend_never initial_suspend() {
      print("suspend_never async::promise_type::initial_suspend()");
      return {};
    }

    suspend_never final_suspend() {
      print("suspend_never async::promise_type::final_suspend()");
      return {};
    }

    void return_value(T value) {
      print("void async::promise_type::return_value(T value)");
      value_ = std::move(value);
      if (handle_) {
        handle_.resume();
      }
    }

    void unhandled_exception() {
      std::abort();
    }

    promise_type() {
      print("async::promise_type::promise_type()");
    }

    ~promise_type() {
      print("async::promise_type::~promise_type()");
    }

    std::optional<T> value_;
    coroutine_handle<> handle_ = nullptr;
  };

  async(promise_type& promise) : handle_(coroutine_handle<promise_type>::from_promise(promise)) {
    print("async::async(promise_type& promise)");
  }

  async() {
    print("async::async()");
  }

  async(async&& other) : handle_(std::exchange(other.handle_, nullptr)) {
    print("async::async(async&& other)");
  }

  async& operator=(async&& other) {
    print("async& async::operator=(async&& other)");
    if (&other != this) {
      handle_ = std::exchange(other.handle_, nullptr);
    }
  }

  ~async() {
    print("async::~async()");
  }

  bool await_ready() {
    print("bool async::await_ready()");
    return !!handle_.promise().value_;
  }

  void await_suspend(coroutine_handle<> consumer) {
    print("void async::await_suspend(coroutine_handle<> consumer)");
    handle_.promise().handle_ = consumer;
    if (await_ready()) {
      handle_.promise().handle_.resume();
    }
  }

  auto& await_resume() {
    print("auto& async::await_resume()");
    return *handle_.promise().value_;
  }

private:
  coroutine_handle<promise_type> handle_ = nullptr;
};

//
// This async_generator is a modified version of https://github.com/kirkshoop/await
//

template <typename T, typename generator_promise>
struct await_iterator;

template <typename T, typename generator_promise>
struct async_iterator;

template <typename T, typename generator_promise>
struct await_consumer;

template <typename T>
class async_generator {
public:
  struct promise_type {
    promise_type& get_return_object() {
      print("promise_type& async_generator::promise_type::get_return_object()");
      return *this;
    }

    suspend_always initial_suspend() {
      print("suspend_always async_generator::promise_type::initial_suspend()");
      return {};
    }

    suspend_always final_suspend() {
      print("suspend_always async_generator::promise_type::final_suspend()");
      return {};
    }

    await_consumer<T, promise_type> yield_value(T& value) {
      print("await_consumer<T, promise_type> async_generator::promise_type::yield_value(T& value)");
      value_ = std::addressof(value);
      return { coroutine_handle<promise_type>::from_promise(*this) };
    }

    void return_void() {
      print("void async_generator::promise_type::return_void()");
      if (auto await_iterator = std::exchange(await_iterator_, nullptr)) {
        await_iterator.resume();
      }
    }

    void unhandled_exception() {
      print("void async_generator::promise_type::unhandled_exception()");
      std::abort();
    }

    promise_type() {
      print("async_generator::promise_type::promise_type()");
    }

    ~promise_type() {
      print("async_generator::promise_type::~promise_type()");
    }

    coroutine_handle<> await_iterator_ = nullptr;
    coroutine_handle<> await_consumer_ = nullptr;
    T* value_ = nullptr;
  };

  await_iterator<T, promise_type> begin() {
    print("await_iterator<T, promise_type> async_generator::begin()");
    return handle_;
  }

  async_iterator<T, promise_type> end() {
    print("await_iterator<T, promise_type> async_generator::end()");
    return { nullptr };
  }

  async_generator(promise_type& promise) : handle_(coroutine_handle<promise_type>::from_promise(promise)) {
    print("async_generator::async_generator(promise_type& promise)");
  }

  async_generator() {
    print("async_generator::async_generator()");
  }

  async_generator(async_generator&& other) : handle_(std::exchange(other.handle_, nullptr)) {
    print("async_generator::async_generator(async_generator&& other)");
  }

  async_generator& operator=(async_generator&& other) {
    print("async_generator& async_generator::operator=(async_generator&& other)");
    if (&other != this) {
      if (handle_) {
        handle_.destroy();
      }
      handle_ = std::exchange(other.handle_, nullptr);
    }
  }

  ~async_generator() {
    print("async_generator::~async_generator()");
    if (handle_) {
      handle_.destroy();
    }
  }

private:
  coroutine_handle<promise_type> handle_ = nullptr;
};

template <typename T, typename GeneratorPromise>
struct await_consumer {
  await_consumer(coroutine_handle<GeneratorPromise> coro) : generator_coro_(coro) {
    print("await_consumer::await_consumer(coroutine_handle<GeneratorPromise> coro)");
  }

  await_consumer() {
    print("await_consumer::await_consumer()");
  }

  await_consumer(await_consumer&& other) : generator_coro_(std::exchange(other.generator_coro_, nullptr)) {
    print("await_consumer::await_consumer(await_consumer&& other)");
  }

  await_consumer& operator=(await_consumer&& other) {
    print("await_consumer& await_consumer::operator=(await_consumer&& other)");
    if (&other != this) {
      generator_coro_ = std::exchange(other.generator_coro_, nullptr);
    }
    return *this;
  }

  ~await_consumer() {
    print("await_consumer::~await_consumer()");
  }

  bool await_ready() {
    print("bool await_consumer::await_ready()");
    return false;
  }

  void await_suspend(coroutine_handle<> await_consumer) {
    print("void await_consumer::await_suspend(coroutine_handle<> await_consumer)");
    generator_coro_.promise().await_consumer_ = await_consumer;
    if (auto await_iterator = std::exchange(generator_coro_.promise().await_iterator_, nullptr)) {
      await_iterator.resume();
    }
  }

  void await_resume() {
    print("void await_consumer::await_resume()");
  }

  coroutine_handle<GeneratorPromise> generator_coro_ = nullptr;
};

template <typename T, typename GeneratorPromise>
struct await_iterator {
  await_iterator(coroutine_handle<GeneratorPromise> coro) : generator_coro_(coro) {
    print("await_iterator::await_iterator(coroutine_handle<GeneratorPromise> coro)");
  }

  // operator++ needs to update itself
  await_iterator(async_iterator<T, GeneratorPromise>* it) : generator_coro_(it->generator_coro_), iterator_(it) {
    print("await_iterator::await_iterator(async_iterator<T, GeneratorPromise>* it)");
  }

  await_iterator() {
    print("await_iterator::await_iterator()");
  }

  await_iterator(await_iterator&& other) : generator_coro_(std::exchange(other.generator_coro_, nullptr)), iterator_(std::exchange(other.iterator_, nullptr)) {
    print("await_iterator::await_iterator(await_iterator&& other)");
  }

  await_iterator& operator=(await_iterator&& other) {
    print("await_iterator& await_iterator::operator=(await_iterator&& other)");
    if (&other != this) {
      generator_coro_ = std::exchange(other.generator_coro_, nullptr);
      iterator_ = std::exchange(other.iterator_, nullptr);
    }
    return *this;
  }

  ~await_iterator() {
    print("await_iterator::~await_iterator()");
  }

  bool await_ready() {
    print("bool await_iterator::await_ready()");
    return generator_coro_.promise().await_consumer_ && generator_coro_.promise().await_consumer_.done();
  }

  void await_suspend(coroutine_handle<> await_iterator) {
    print("void await_iterator::await_suspend(coroutine_handle<> await_iterator)");
    generator_coro_.promise().await_iterator_ = await_iterator;
    if (auto await_consumer = std::exchange(generator_coro_.promise().await_consumer_, nullptr)) {
      print("resume co_yield");
      generator_coro_.promise().value_ = nullptr;
      await_consumer.resume();
    } else {
      print("first resume");
      generator_coro_.resume();
    }
  }

  async_iterator<T, GeneratorPromise> await_resume() {
    print("async_iterator<T, GeneratorPromise> await_iterator::await_resume()");
    if (generator_coro_.done() || !generator_coro_.promise().value_) {
      generator_coro_ = nullptr;
    }
    if (iterator_) {
      iterator_->generator_coro_ = generator_coro_;
      return { *iterator_ };
    }
    return { generator_coro_ };
  }

  coroutine_handle<GeneratorPromise> generator_coro_ = nullptr;
  async_iterator<T, GeneratorPromise>* iterator_ = nullptr;
};

template <typename T, typename GeneratorPromise>
struct async_iterator {
  using iterator_category = std::input_iterator_tag;
  using difference_type = std::ptrdiff_t;
  using value_type = T;
  using pointer = T*;
  using reference = T&;

  async_iterator(std::nullptr_t) : generator_coro_(nullptr) {
    print("async_iterator::async_iterator(std::nullptr_t)");
  }

  async_iterator(coroutine_handle<GeneratorPromise> coro) : generator_coro_(coro) {
    print("async_iterator::async_iterator(coroutine_handle<GeneratorPromise> coro)");
  }

  ~async_iterator() {
    print("async_iterator::~async_iterator()");
  }

  await_iterator<T, GeneratorPromise> operator++() {
    print("await_iterator<T, GeneratorPromise> async_iterator::operator++()");
    if (!generator_coro_) {
      std::abort();
    }
    return { this };
  }

  // Generator iterator current_value is a reference to a temporary on the coroutine frame
  // implementing postincrement will require storing a copy of the value in the iterator.
  //
  //   async_iterator operator++(int) {
  //     auto result = *this;
  //     ++(*this);
  //     return result;
  //   }
  //
  async_iterator operator++(int) = delete;

  bool operator==(const async_iterator& other) const {
    print("bool async_iterator::operator==(const async_iterator& other)");
    return generator_coro_ == other.generator_coro_;
  }

  bool operator!=(const async_iterator& other) const {
    print("bool async_iterator::operator!=(const async_iterator& other)");
    return generator_coro_ != other.generator_coro_;
  }

  reference operator*() {
    print("reference async_iterator::operator*()");
    return *generator_coro_.promise().value_;
  }

  pointer operator->() {
    print("pointer async_iterator::operator->()");
    return std::addressof(generator_coro_.promise().value_);
  }

  coroutine_handle<GeneratorPromise> generator_coro_ = nullptr;
};

#if 0
class task {
public:
  struct promise_type {
    constexpr auto& get_return_object() noexcept {
      return *this;
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

  task(promise_type& promise) noexcept {
  }
};

template <typename T>
class async {
public:
  struct promise_type {
    auto& get_return_object() noexcept {
      return *this;
    }

    constexpr auto initial_suspend() noexcept {
      return suspend_never{};
    }

    constexpr auto final_suspend() noexcept {
      return suspend_never{};
    }

    void return_value(T value) noexcept {
      value_ = std::move(value);
      if (handle_) {
        handle_.resume();
      }
    }

    void unhandled_exception() noexcept {
      std::abort();
    }

    std::optional<T> value_;
    coroutine_handle<> handle_;
  };

  async(promise_type& promise) noexcept : handle_(coroutine_handle<promise_type>::from_promise(promise)) {
  }

  async() noexcept = default;

  async(async&& other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {
  }

  async& operator=(async&& other) noexcept {
    if (&other != this) {
      handle_ = std::exchange(other.handle_, nullptr);
    }
  }

  ~async() {
  }

  bool await_ready() noexcept {
    return !!handle_.promise().value_;
  }

  void await_suspend(coroutine_handle<> consumer) noexcept {
    handle_.promise().handle_ = consumer;
    if (await_ready()) {
      handle_.promise().handle_.resume();
    }
  }

  auto& await_resume() noexcept {
    return *handle_.promise().value_;
  }

private:
  coroutine_handle<promise_type> handle_ = nullptr;
};

//
// This async_generator is a modified version of https://github.com/kirkshoop/await
//

template <typename T, typename generator_promise>
struct await_iterator;

template <typename T, typename generator_promise>
struct async_iterator;

template <typename T, typename generator_promise>
struct await_consumer;

template <typename T>
class async_generator {
public:
  struct promise_type {
    auto& get_return_object() noexcept {
      return *this;
    }

    constexpr auto initial_suspend() noexcept {
      return suspend_always{};
    }

    constexpr auto final_suspend() noexcept {
      return suspend_always{};
    }

    await_consumer<T, promise_type> yield_value(T& value) noexcept {
      value_ = std::addressof(value);
      return { coroutine_handle<promise_type>::from_promise(*this) };
    }

    void return_void() noexcept {
      if (auto await_iterator = std::exchange(await_iterator_, nullptr)) {
        await_iterator.resume();
      }
    }

    void unhandled_exception() noexcept {
      std::abort();
    }

    coroutine_handle<> await_iterator_;
    coroutine_handle<> await_consumer_;
    T* value_ = nullptr;
  };

  await_iterator<T, promise_type> begin() noexcept {
    return handle_;
  }

  async_iterator<T, promise_type> end() noexcept {
    return { nullptr };
  }

  async_generator(promise_type& promise) noexcept :
    handle_(coroutine_handle<promise_type>::from_promise(promise)) {
  }

  async_generator() noexcept = default;

  async_generator(async_generator&& other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {
  }

  async_generator& operator=(async_generator&& other) noexcept {
    if (&other != this) {
      if (handle_) {
        handle_.destroy();
      }
      handle_ = std::exchange(other.handle_, nullptr);
    }
  }

  ~async_generator() {
    if (handle_) {
      handle_.destroy();
    }
  }

private:
  coroutine_handle<promise_type> handle_ = nullptr;
};

template <typename T, typename GeneratorPromise>
struct await_consumer {
  await_consumer(coroutine_handle<GeneratorPromise> coro) noexcept : generator_coro_(coro) {
  }

  await_consumer() noexcept = default;

  await_consumer(await_consumer&& other) noexcept : generator_coro_(std::exchange(other.generator_coro_, nullptr)) {
  }

  await_consumer& operator=(await_consumer&& other) noexcept {
    if (&other != this) {
      generator_coro_ = std::exchange(other.generator_coro_, nullptr);
    }
    return *this;
  }

  ~await_consumer() {
  }

  constexpr bool await_ready() noexcept {
    return false;
  }

  void await_suspend(coroutine_handle<> await_consumer) noexcept {
    generator_coro_.promise().await_consumer_ = await_consumer;
    if (auto await_iterator = std::exchange(generator_coro_.promise().await_iterator_, nullptr)) {
      await_iterator.resume();
    }
  }

  constexpr void await_resume() noexcept {
  }

  coroutine_handle<GeneratorPromise> generator_coro_;
};

template <typename T, typename GeneratorPromise>
struct await_iterator {
  coroutine_handle<GeneratorPromise> generator_coro_;
  async_iterator<T, GeneratorPromise>* iterator_ = nullptr;

  await_iterator(coroutine_handle<GeneratorPromise> coro) noexcept : generator_coro_(coro) {
  }

  // operator++ needs to update itself
  await_iterator(async_iterator<T, GeneratorPromise>* it) noexcept :
    generator_coro_(it->generator_coro_), iterator_(it) {
  }

  await_iterator() noexcept = default;

  await_iterator(await_iterator&& other) noexcept :
    generator_coro_(std::exchange(other.generator_coro_, nullptr)), iterator_(std::exchange(other.iterator_, nullptr)) {
  }

  await_iterator& operator=(await_iterator&& other) noexcept {
    if (&other != this) {
      generator_coro_ = std::exchange(other.generator_coro_, nullptr);
      iterator_ = std::exchange(other.iterator_, nullptr);
    }
    return *this;
  }

  ~await_iterator() {
  }

  constexpr bool await_ready() noexcept {
    return false;
  }

  void await_suspend(coroutine_handle<> await_iterator) noexcept {
    generator_coro_.promise().await_iterator_ = await_iterator;
    if (auto await_consumer = std::exchange(generator_coro_.promise().await_consumer_, nullptr)) {
      // resume co_yield
      generator_coro_.promise().value_ = nullptr;
      //if (await_consumer.done()) {  // TODO: Find out if this is correct.
        await_consumer.resume();
      //}
    } else {
      // first resume
      generator_coro_.resume();
    }
  }

  async_iterator<T, GeneratorPromise> await_resume() noexcept {
    if (generator_coro_.done() || !generator_coro_.promise().value_) {
      generator_coro_ = nullptr;
    }
    if (iterator_) {
      iterator_->generator_coro_ = generator_coro_;
      return { *iterator_ };
    }
    return { generator_coro_ };
  }
};

template <typename T, typename GeneratorPromise>
struct async_iterator {
  using iterator_category = std::input_iterator_tag;
  using difference_type = std::ptrdiff_t;
  using value_type = T;
  using pointer = T*;
  using reference = T&;

  coroutine_handle<GeneratorPromise> generator_coro_;

  async_iterator(std::nullptr_t) noexcept : generator_coro_(nullptr) {
  }

  async_iterator(coroutine_handle<GeneratorPromise> coro) noexcept : generator_coro_(coro) {
  }

  await_iterator<T, GeneratorPromise> operator++() noexcept {
    if (!generator_coro_) {
      std::abort();
    }
    return { this };
  }

  // Generator iterator current_value is a reference to a temporary on the coroutine frame
  // implementing postincrement will require storing a copy of the value in the iterator.
  //
  // async_iterator operator++(int) noexcept {
  //   auto result = *this;
  //   ++(*this);
  //   return result;
  // }
  async_iterator operator++(int) = delete;

  bool operator==(const async_iterator& other) const noexcept {
    return generator_coro_ == other.generator_coro_;
  }

  bool operator!=(const async_iterator& other) const noexcept {
    return !(*this == other);
  }

  reference operator*() noexcept {
    return *generator_coro_.promise().value_;
  }

  pointer operator->() noexcept {
    return std::addressof(operator*());
  }
};
#endif

}  // namespace coronet

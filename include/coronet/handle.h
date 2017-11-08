#pragma once
#include <iosfwd>
#include <memory>
#include <type_traits>
#include <utility>
#include <cstdint>

namespace coronet {

template <typename I>
class handle {
public:
#ifdef WIN32
  using handle_type = std::intptr_t;
  constexpr static handle_type invalid_handle_value = 0;
#else
  using handle_type = int;
  constexpr static handle_type invalid_handle_value = -1;
#endif

  constexpr handle() noexcept = default;

#ifdef WIN32
  template <typename T>
  constexpr explicit handle(T handle) noexcept {
    if constexpr (std::is_pointer_v<T>) {
      handle_ = reinterpret_cast<handle_type>(handle);
    } else {
      handle_ = static_cast<handle_type>(handle);
    }
  }
#else
  constexpr explicit handle(handle_type handle) noexcept : handle_(handle) {
  }
#endif

  handle(handle&& other) noexcept : handle_(other.release()) {
  }

  handle& operator=(handle&& other) noexcept {
    if (this != std::addressof(other)) {
      reset(other.release());
    }
    return *this;
  }

  handle(const handle& other) = delete;
  handle& operator=(const handle& other) = delete;

  virtual ~handle() {
    static_assert(noexcept(static_cast<I*>(this)->close()), "close function must be noexcept");
    if (handle_ != invalid_handle_value) {
      static_cast<I*>(this)->close();
    }
  }

  constexpr bool operator==(const handle& other) const noexcept {
    return handle_ == other.handle_;
  }

  constexpr bool operator!=(const handle& other) const noexcept {
    return handle_ != other.handle_;
  }

  constexpr bool valid() const noexcept {
    return handle_ != invalid_handle_value;
  }

  constexpr explicit operator bool() const noexcept {
    return valid();
  }

#ifdef WIN32
  template <typename T>
  constexpr T as() const noexcept {
    if constexpr (std::is_pointer_v<T>) {
      return reinterpret_cast<T>(handle_);
    } else {
      return static_cast<T>(handle_);
    }
  }
#endif

  constexpr handle_type value() const noexcept {
    return handle_;
  }

  void reset(handle_type handle = invalid_handle_value) {
    static_assert(noexcept(static_cast<I*>(this)->close()), "close function must be noexcept");
    if (handle_ != invalid_handle_value) {
      static_cast<I*>(this)->close();
    }
    handle_ = handle;
  }

  handle_type release() noexcept {
    return std::exchange(handle_, invalid_handle_value);
  }

protected:
  handle_type handle_ = invalid_handle_value;
};

template <typename I>
constexpr std::true_type handle_detector(const handle<I>*) {
  return {};
}

constexpr std::false_type handle_detector(const void*) {
  return {};
}

template <typename T>
struct is_handle : decltype(handle_detector(std::declval<const T*>())) {};

template <typename T>
constexpr bool is_handle_v = is_handle<T>::value;

template <typename Formatter, typename Handle, typename = std::enable_if_t<is_handle_v<Handle>>>
void format_arg(Formatter& formatter, const char*&, const Handle& handle) {
#ifdef WIN32
  const auto value = handle.template as<std::uint64_t>();
#else
  const auto value = static_cast<std::uint64_t>(handle.value());
#endif
  formatter.writer().write("{:016X}", value);
}

template <typename Char, typename Handle, typename = std::enable_if_t<is_handle_v<Handle>>>
std::basic_ostream<Char>& operator<<(std::basic_ostream<Char>& os, const Handle& handle) {
  const auto fill = os.fill('0');
  const auto width = os.width(16);
  const auto flags = os.flags();
  os.setf(os.uppercase);
  os.setf(os.hex, os.basefield);
#ifdef WIN32
  os << handle.template as<std::uint64_t>();
#else
  os << static_cast<std::uint64_t>(handle.value());
#endif
  os.flags(flags);
  os.width(width);
  os.fill(fill);
  return os;
}

}  // namespace coronet

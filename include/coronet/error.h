#pragma once
#include <system_error>

namespace coronet {

enum class errc {
  eof = -1,
  cancelled = -2,
};

const std::error_category& error_category() noexcept;

inline std::error_code make_error_code(errc code) noexcept {
  return { static_cast<int>(code), error_category() };
}

}  // namespace coronet

namespace std {

template<>
struct is_error_code_enum<coronet::errc> : true_type {};

}  // namespace std

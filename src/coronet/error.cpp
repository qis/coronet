#include <coronet/error.h>
#include <algorithm>
#include <string>
#include <cctype>

namespace coronet {

class error_category_impl : public std::error_category {
public:
  const char* name() const noexcept override {
    return "coronet";
  }

  std::string message(int condition) const override {
    switch (static_cast<errc>(condition)) {
    case errc::eof: return "end of file";
    case errc::cancelled: return "cancelled";
    }
    auto str = std::system_category().message(condition);
    std::transform(str.begin(), str.end(), str.begin(), [](char c) noexcept {
      return static_cast<char>(std::tolower(c));
    });
    return str;
  }
};

static const error_category_impl error_category_inst;

const std::error_category& error_category() noexcept {
  return error_category_inst;
}

}  // namespace coronet

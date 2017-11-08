#pragma once
#include <coronet/error.h>
#include <coronet/socket.h>
#include <memory>
#include <string>

#ifdef WIN32
#include <ws2tcpip.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#endif

namespace coronet {

inline int to_int(family family) noexcept {
  switch (family) {
  case family::ipv4: return AF_INET;
  case family::ipv6: return AF_INET6;
  }
  return static_cast<int>(family);
}

inline int to_int(type type) noexcept {
  switch (type) {
  case type::tcp: return SOCK_STREAM;
  case type::udp: return SOCK_DGRAM;
  }
  return static_cast<int>(type);
}

class address_error_category : public std::error_category {
public:
  const char* name() const noexcept override {
    return "address";
  }

  std::string message(int condition) const override {
#ifdef WIN32
    return ::gai_strerrorA(condition);
#else
    return ::gai_strerror(condition);
#endif
  }
};

inline const std::error_category& address_category() noexcept {
  static const address_error_category category;
  return category;
}

class address {
public:
  address() noexcept : info_(nullptr, freeaddrinfo) {
  }

  std::error_code create(const std::string& host, const std::string& port, type type, int flags) noexcept {
    struct addrinfo* info = nullptr;
    struct addrinfo hints = {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = to_int(type);
    hints.ai_flags = flags;
    if (const auto rv = ::getaddrinfo(host.data(), port.data(), &hints, &info)) {
      return { rv, address_category() };
    }
    info_.reset(info);
    return {};
  }

  explicit operator bool() const noexcept {
    return !!info_;
  }

  auto family() const noexcept {
    switch (info_->ai_family) {
    case AF_INET: return family::ipv4;
    case AF_INET6: return family::ipv6;
    }
    return static_cast<coronet::family>(info_->ai_family);
  }

  auto type() const noexcept {
    switch (info_->ai_socktype) {
    case SOCK_STREAM: return type::tcp;
    case SOCK_DGRAM: return type::udp;
    }
    return static_cast<coronet::type>(info_->ai_socktype);
  }

  auto addr() const noexcept {
    return info_->ai_addr;
  }

  auto addrlen() const noexcept {
    return info_->ai_addrlen;
  }

  auto protocol() const noexcept {
    return info_->ai_protocol;
  }

private:
  std::unique_ptr<addrinfo, decltype(&::freeaddrinfo)> info_;
};

}  // namespace coronet

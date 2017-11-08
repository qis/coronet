#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

namespace net {

struct wsa {
  wsa() {
    WSADATA wsadata = {};
    WSAStartup(MAKEWORD(2, 2), &wsadata);
  }
  ~wsa() {
    WSACleanup();
  }
};

class socket {
public:
  constexpr socket() noexcept = default;

  socket(socket&& other) noexcept : handle_(std::exchange(other.handle_, INVALID_SOCKET)) {
  }

  socket& operator=(socket&& other) noexcept {
    close();
    handle_ = std::exchange(other.handle_, INVALID_SOCKET);
    return *this;
  }

  constexpr socket(SOCKET handle) noexcept : handle_(handle) {
  }

  socket& operator=(SOCKET handle) noexcept {
    close();
    handle_ = handle;
    return *this;
  }

  ~socket() {
    close();
  }

  constexpr operator SOCKET() const noexcept {
    return handle_;
  }

  constexpr explicit operator bool() const noexcept {
    return handle_ != INVALID_SOCKET;
  }

  void close() noexcept {
    if (handle_ != INVALID_SOCKET) {
      ::shutdown(handle_, SD_BOTH);
      ::closesocket(handle_);
    }
  }

private:
  SOCKET handle_ = INVALID_SOCKET;
};

}  // namespace net

int main(int argc, char* argv[]) {
  const auto host = argc > 1 ? argv[1] : "127.0.0.1";
  const auto port = argc > 2 ? argv[2] : "8080";
  const auto bufs = argc > 3 ? std::stoull(argv[3]) : 40960ull;

  net::wsa wsa;

  struct addrinfo* result = nullptr;
  struct addrinfo hints = {};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = 0;
  if (const auto rv = ::getaddrinfo(host, port, &hints, &result)) {
    std::cerr << "could not get address info for " << host << ':' << port << std::endl;
    return rv;
  }
  std::unique_ptr<struct addrinfo, decltype(&::freeaddrinfo)> info(result, ::freeaddrinfo);

  const net::socket server = ::socket(info->ai_family, info->ai_socktype, info->ai_protocol);
  if (!server) {
    std::cerr << "could not create server socket" << std::endl;
    return WSAGetLastError();
  }

  if (::bind(server, info->ai_addr, static_cast<int>(info->ai_addrlen)) == SOCKET_ERROR) {
    std::cerr << "could not bind to " << host << ':' << port << std::endl;
    return WSAGetLastError();
  }

  if (::listen(server, SOMAXCONN) == SOCKET_ERROR) {
    std::cerr << "could not listen on server socket" << std::endl;
    return WSAGetLastError();
  }

  std::string buffer;
  buffer.resize(bufs);
  const auto buffer_data = buffer.data();
  const auto buffer_size = static_cast<int>(buffer.size());

  while (true) {
    struct sockaddr_storage addr;
    auto addrlen = static_cast<int>(sizeof(addr));
    const net::socket socket = ::accept(server, reinterpret_cast<struct sockaddr*>(&addr), &addrlen);
    if (!socket) {
      std::cerr << "could not accept client connection" << std::endl;
      continue;
    }

    auto done = false;

    while (!done) {
      auto data = buffer_data;
      auto size = ::recv(socket, data, buffer_size, 0);
      if (size <= 0) {
        if (size != 0) {
          std::cerr << "could not recv data" << std::endl;
        }
        break;
      }
      while (size > 0) {
        const auto rv = ::send(socket, data, size, 0);
        if (rv <= 0) {
          if (rv != 0) {
            std::cerr << "could not send data" << std::endl;
          }
          done = true;
          break;
        }
        data += rv;
        size -= rv;
      }
    }
  }
}

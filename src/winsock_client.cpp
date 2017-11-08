// ユニコード
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

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
  using namespace std::chrono;

  using clock = system_clock;

  const auto host = argc > 1 ? argv[1] : "127.0.0.1";
  const auto port = argc > 2 ? argv[2] : "8080";
  const auto bufs = argc > 3 ? std::stoull(argv[3]) : 40960ull;
  const auto dura = argc > 4 ? std::stoull(argv[4]) : 3ull;  // seconds

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

  net::socket socket = ::socket(info->ai_family, info->ai_socktype, info->ai_protocol);
  if (!socket) {
    std::cerr << "could not create socket" << std::endl;
    return WSAGetLastError();
  }

  if (::connect(socket, info->ai_addr, static_cast<int>(info->ai_addrlen)) == SOCKET_ERROR) {
    std::cerr << "could not connect to " << host << ':' << port << std::endl;
    return WSAGetLastError();
  }

  DWORD timeout = 1000;  // milliseconds
  const auto timeout_data = reinterpret_cast<const char*>(&timeout);
  const auto timeout_size = static_cast<int>(sizeof(timeout));
  if (::setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, timeout_data, timeout_size) == SOCKET_ERROR) {
    std::cerr << "could not set socket timeout" << std::endl;
    return WSAGetLastError();
  }

  std::string buffer;
  buffer.resize(bufs);
  std::generate(buffer.begin(), buffer.end(), [c = '0']() mutable {
    c = c > '9' ? '0' : c;
    return c++;
  });

  clock::time_point beg;
  clock::time_point end;

  std::vector<std::optional<clock::time_point>> send_tp;
  send_tp.resize(dura);

  std::vector<std::optional<clock::time_point>> recv_tp;
  recv_tp.resize(dura);

  std::atomic<bool> done = false;
  auto bytes_send = 0ull;
  auto bytes_recv = 0ull;

  std::mutex mutex;
  std::condition_variable cv;
  std::unique_lock<std::mutex> lock(mutex);

  auto thread = std::thread([&]() {
    std::string buffer;
    buffer.resize(bufs);

    auto index = 0ull;
    const auto buffer_data = buffer.data();
    const auto buffer_size = static_cast<int>(buffer.size());

    {
      std::lock_guard<std::mutex> lock(mutex);
      cv.notify_one();
    }

    while (!done || bytes_recv < bytes_send) {
      const auto rv = ::recv(socket, buffer_data, buffer_size, 0);
      if (rv == SOCKET_ERROR) {
        std::cerr << "could not recv data" << std::endl;
        break;
      }
      const auto bytes = static_cast<unsigned long long>(rv);
      for (auto i = 0ull; i < bytes; i++) {
        if (buffer_data[i] == 'X') {
          recv_tp[index++] = clock::now();
        }
      }
      bytes_recv += bytes;
    }

    end = clock::now();
    socket.close();
  });

  cv.wait(lock);  // wait for the recv thread

  beg = clock::now();

  auto index = 0ull;
  const auto buffer_data = buffer.data();
  const auto buffer_size = static_cast<int>(buffer.size());

  while (index < dura) {
    const auto now = clock::now();
    const auto sec = static_cast<unsigned long long>(duration_cast<seconds>(now - beg).count());
    if (sec > index) {
      buffer_data[0] = 'X';
      send_tp[index] = now;
    }
    const auto rv = ::send(socket, buffer_data, buffer_size, 0);
    if (rv == SOCKET_ERROR) {
      std::cerr << "could not send data" << std::endl;
      break;
    }
    bytes_send += static_cast<unsigned long long>(rv);
    if (sec > index) {
      buffer_data[0] = '0';
      index++;
    }
  }

  done = true;

  if (thread.joinable()) {
    thread.join();
  }

  if (bytes_recv < bytes_send) {
    std::cerr << (bytes_send - bytes_recv) << " bytes missing" << std::endl;
  }

  const auto duration = end - beg;
  const auto s = duration_cast<milliseconds>(duration).count() / 1000.0;

  const auto bytes_per_second = bytes_recv / s;
  const auto kilobytes_per_second = bytes_recv / 1024 / s;
  const auto megabytes_per_second = bytes_recv / 1024 / 1024 / s;
  const auto gigabytes_per_second = bytes_recv / 1024 / 1024 / 1024 / s;

  std::cout << std::setfill('0') << std::fixed;

  if (gigabytes_per_second > 1.0) {
    std::cout << std::setprecision(3) << gigabytes_per_second << " GiB/s";
  } else if (megabytes_per_second > 1.0) {
    std::cout << std::setprecision(3) << megabytes_per_second << " MiB/s";
  } else if (kilobytes_per_second > 1.0) {
    std::cout << std::setprecision(3) << kilobytes_per_second << " KiB/s";
  } else {
    std::cout << std::setprecision(3) << bytes_per_second << " Bytes/s";
  }

  std::cout << " (" << bytes_recv << " bytes in " << std::setprecision(0) << s << " seconds)\n\n";

  for (auto i = 0ull; i < index; i++) {
    std::cout << ' ' << i << ": ";
    if (send_tp[i] && recv_tp[i]) {
      const auto dur = recv_tp[i].value() - send_tp[i].value();
      if (duration_cast<seconds>(dur).count() > 1) {
        std::cout << std::setprecision(3) << (duration_cast<milliseconds>(dur).count() / 1000.0) << " s";
      } else if (duration_cast<milliseconds>(dur).count() > 1) {
        std::cout << std::setprecision(3) << (duration_cast<microseconds>(dur).count() / 1000.0) << " ms";
      } else if (duration_cast<microseconds>(dur).count() > 1) {
        std::wcout << std::setprecision(3) << (duration_cast<nanoseconds>(dur).count() / 1000.0) << L" µs";
      } else {
        std::cout << std::setprecision(3) << duration_cast<nanoseconds>(dur).count() << " ns";
      }
    } else {
      std::cout << '-';
    }
    std::cout << '\n';
  }
  std::cout << std::endl;
}

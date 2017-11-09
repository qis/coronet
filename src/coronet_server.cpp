#include <coronet/events.h>
#include <coronet/server.h>
#include <coronet/socket.h>
#include <coronet/signal.h>
#include <iostream>
#include <string>

// clang-format off

std::ostream& operator<<(std::ostream& os, const std::error_code& ec) noexcept {
  return os << '[' << ec.category().name() << ':' << ec.value() << ']';
}

coronet::task handle(coronet::socket socket, std::size_t bufs) noexcept {
  std::string buffer;
  buffer.resize(bufs);
  for co_await(const auto data : socket.recv(buffer.data(), buffer.size())) {
    if (const auto ec = co_await socket.send(data)) {
      std::cerr << socket << ": " << ec << " send error: " << ec.message() << '\n';
      break;
    }
  }
  if (const auto ec = socket.ec(); ec != coronet::errc::eof) {
    std::cerr << socket << ": " << ec << " recv error: " << ec.message() << '\n';
  }
  co_return;
}

coronet::task accept(coronet::server& server, std::size_t bufs) noexcept {
  for co_await(auto& socket : server.accept()) {
    if (const auto ec = socket.set(coronet::option::nodelay, true)) {
      std::cerr << socket << ": " << ec << " set nodelay error: " << ec.message() << '\n';
    }
    handle(std::move(socket), bufs);
  }
  if (const auto ec = server.ec()) {
    std::cerr << ec << " accept error: " << ec.message() << '\n';
  }
  std::cout << "server stopped\n";
  co_return;
}

// clang-format on

int main(int argc, char* argv[]) {
  const auto host = argc > 1 ? argv[1] : "127.0.0.1";
  const auto port = argc > 2 ? argv[2] : "8080";
  const auto bufs = argc > 3 ? std::stoull(argv[3]) : 40960ull;

  // Create event loop.
  coronet::events events;
  if (const auto ec = events.create()) {
    std::cerr << ec << " create events error: " << ec.message() << std::endl;
    return ec.value();
  }

  // Trap SIGINT signal.
  coronet::signal(SIGINT, [&](int signum) { events.close(); });

  // Create TCP Server.
  coronet::server server(events);
  if (const auto ec = server.create(host, port, coronet::type::tcp)) {
    std::cerr << ec << " create server error: " << ec.message() << std::endl;
    return ec.value();
  }

  // Accept incoming connections.
  accept(server, bufs);

  // Run event loop.
  std::cout << host << ':' << port << '\n';
  if (const auto ec = events.run()) {
    std::cerr << ec << ' ' << ec.message() << std::endl;
    return ec.value();
  }
}

#include <coronet/signal.h>
#include <mutex>
#include <unordered_map>

namespace coronet {
namespace {

std::unordered_map<int, std::function<void(int)>> g_signal_handlers;
std::mutex g_signal_handlers_mutex;

void signal_callback(int signum) {
  std::function<void(int)> handler;
  {
    std::lock_guard<std::mutex> lock(g_signal_handlers_mutex);
    auto it = g_signal_handlers.find(signum);
    if (it != g_signal_handlers.end()) {
      handler = it->second;
    }
  }
  if (handler) {
    handler(signum);
  }
}

}  // namespace

void signal(int signum, std::function<void(int)> handler) {
  std::lock_guard<std::mutex> lock(g_signal_handlers_mutex);
  if (handler) {
    std::signal(signum, signal_callback);
  } else {
    std::signal(signum, SIG_IGN);
  }
  g_signal_handlers[signum] = std::move(handler);
}

}  // namespace coronet

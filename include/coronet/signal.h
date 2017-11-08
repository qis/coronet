#pragma once
#include <functional>
#include <csignal>

namespace coronet {

// Permanently installs a signal handler.
void signal(int signum, std::function<void(int)> handler = {});

}  // namespace coronet

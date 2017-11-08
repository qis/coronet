#pragma once
#include <coronet/async.h>
#include <coronet/error.h>
#include <coronet/handle.h>

namespace coronet {

class events final : public handle<events> {
public:
  using handle::handle;

  // Creates events queue that processes max. size number of events at once.
  std::error_code create() noexcept;

  // Runs events queue on the given processor.
  std::error_code run(int processor = -1);

  // Closes events queue.
  std::error_code close() noexcept;
};

}  // namespace coronet

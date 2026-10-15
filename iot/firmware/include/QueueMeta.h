#pragma once
#include <array>
#include <stdint.h>
#include <string>
namespace irrigation {
struct QueueCancellation {
  std::string digest, messageId, path, reason, recordedAt;
  uint64_t generation = 0;
};
struct QueueMeta {
  unsigned head = 0, count = 0;
  uint64_t generation = 0, cancelled = 0;
  std::array<QueueCancellation, 8> cancellations{};
  bool valid() const {
    return head < 16 && count <= 16 && generation < UINT64_MAX;
  }
  bool enqueue() {
    if (!valid() || count == 16)
      return false;
    ++count;
    ++generation;
    return true;
  }
  bool pop() {
    if (!valid() || !count)
      return false;
    head = (head + 1) % 16;
    --count;
    ++generation;
    return true;
  }
  bool cancel(const QueueCancellation &entry) {
    if (!valid() || !count || entry.digest.size() != 64 ||
        entry.reason.empty() || entry.reason.size() > 120 ||
        cancelled == UINT64_MAX)
      return false;
    const unsigned index = cancelled % 8;
    if (!pop())
      return false;
    cancellations[index] = entry;
    cancellations[index].generation = generation;
    ++cancelled;
    return true;
  }
};
} // namespace irrigation

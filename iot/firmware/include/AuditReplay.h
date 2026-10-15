#pragma once
#include <stdint.h>
namespace irrigation {
struct AuditReplay {
  uint64_t cursor = 0, overwritten = 0;
  bool fence(uint64_t last, uint64_t destinationFloor) {
    if (cursor < destinationFloor)
      cursor = destinationFloor;
    const uint64_t first = last > 63 ? last - 63 : 1;
    if (cursor < first - 1) {
      const uint64_t delta = first - 1 - cursor;
      if (UINT64_MAX - overwritten < delta)
        return false;
      overwritten += delta;
      cursor = first - 1;
    }
    return true;
  }
};
} // namespace irrigation

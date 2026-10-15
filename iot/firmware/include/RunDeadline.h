#pragma once
#include <stdint.h>
namespace irrigation {
// Pure monotonic duration primitive; not yet a complete irrigation controller.
class RunDeadline {
  uint32_t began_ = 0, duration_ = 0;
  bool active_ = false;

public:
  bool start(uint32_t now, uint32_t duration, uint32_t limit) {
    if (active_ || duration == 0 || duration > limit || duration >= 0x80000000u)
      return false;
    began_ = now;
    duration_ = duration;
    active_ = true;
    return true;
  }
  bool expired(uint32_t now) const {
    return active_ && uint32_t(now - began_) >= duration_;
  }
  void stop() { active_ = false; }
  bool active() const { return active_; }
};
} // namespace irrigation

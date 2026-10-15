#pragma once
#include <cmath>
#include <stddef.h>
#include <stdint.h>
namespace irrigation {
enum class Mode { Static, Dynamic, Hybrid };
enum class Source { Static, Dynamic, Hybrid, Manual, Test };
struct Reading {
  float value = 0;
  uint32_t at = 0;
  bool valid = false;
};
struct Zone {
  bool enabled = false;
  Mode mode = Mode::Static;
  int8_t sensor = -1;
  float below = 30, target = 45;
  uint32_t maxMs = 600000, pauseMs = 1800000;
};
struct Window {
  bool open = false, startNow = false;
  uint64_t occurrence = 0;
  uint32_t remainingMs = 0;
  int64_t begins = 0;
  bool retired = false;
};
struct Run {
  bool active = false;
  uint32_t began = 0, duration = 0;
  Source source = Source::Manual;
};
struct Backend {
  virtual ~Backend() = default;
  // Success means electrical output accepted, not water flow confirmed.
  virtual bool on(size_t slot, uint32_t duration) = 0;
  virtual void off(size_t slot) = 0;
};
struct Journal {
  virtual ~Journal() = default;
  // Persist occurrence BEFORE energizing; failure must fail closed.
  virtual bool consume(size_t slot, uint64_t occurrence) = 0;
  virtual bool consumeWindow(size_t slot, uint64_t occurrence, int64_t) {
    return consume(slot, occurrence);
  }
  virtual bool contains(size_t, uint64_t) const { return false; }
  virtual bool reserve(size_t, Source) { return true; }
  virtual void event(size_t slot, const char *kind, const char *reason,
                     Source source) = 0;
};
class Controller {
  Backend &io;
  Journal &journal;
  Zone zones_[4];
  Reading readings_[4];
  Run runs_[4];
  uint32_t stopped_[4] = {};
  bool hasStopped_[4] = {};
  uint64_t consumed_[4][128] = {};
  uint8_t consumedCount_[4] = {};
  bool used(size_t slot, uint64_t key) const {
    if (journal.contains(slot, key)) return true;
    for (unsigned i = 0; i < consumedCount_[slot]; ++i)
      if (consumed_[slot][i] == key)
        return true;
    return false;
  }
  uint8_t count_ = 0, capacity_ = 1;
  bool clock_ = false;
  uint32_t age_ = 30000;
  bool moisture(size_t slot, uint32_t now) const {
    const int s = zones_[slot].sensor;
    return s >= 0 && s < 4 && readings_[s].valid &&
           std::isfinite(readings_[s].value) && readings_[s].value >= 0 &&
           readings_[s].value <= 100 && uint32_t(now - readings_[s].at) <= age_;
  }
  bool start(size_t slot, uint32_t now, uint32_t duration, Source source) {
    if (slot >= count_)
      return false;
    const char *reason = nullptr;
    if (!clock_)
      reason = "clock_invalid";
    else if (!zones_[slot].enabled)
      reason = "actuator_disabled";
    else if (runs_[slot].active)
      reason = "conflict";
    else if (!duration || duration > zones_[slot].maxMs ||
             duration >= 0x80000000u)
      reason = "duration_invalid";
    else if (activeCount() >= capacity_)
      reason = "capacity";
    if (reason) {
      journal.event(slot, "rejected", reason, source);
      return false;
    }
    if (!journal.reserve(slot, source)) {
      journal.event(slot, "failed", "audit_unavailable", source);
      return false;
    }
    if (!io.on(slot, duration)) {
      stopped_[slot] = now;
      hasStopped_[slot] = true;
      journal.event(slot, "failed", "output_unavailable", source);
      return false;
    }
    runs_[slot].active = true;
    runs_[slot].began = now;
    runs_[slot].duration = duration;
    runs_[slot].source = source;
    journal.event(slot, "started", "electrical_output", source);
    return true;
  }

public:
  Controller(Backend &b, Journal &j) : io(b), journal(j) {}
  bool configure(const Zone *z, size_t count, uint8_t capacity) {
    if (activeCount() || count > 4 || capacity < 1 || capacity > 4)
      return false;
    for (size_t i = 0; i < count; ++i) {
      if (!z[i].maxMs || z[i].maxMs >= 0x80000000u ||
          z[i].pauseMs >= 0x80000000u ||
          (z[i].mode != Mode::Static &&
           (z[i].sensor < 0 || z[i].sensor >= 4 || !std::isfinite(z[i].below) ||
            !std::isfinite(z[i].target) || z[i].below < 0 ||
            z[i].target > 100 || z[i].below >= z[i].target)))
        return false;
    }
    count_ = uint8_t(count);
    capacity_ = capacity;
    for (size_t i = 0; i < 4; ++i) {
      zones_[i] = i < count ? z[i] : Zone{};
      consumedCount_[i] = 0;
    }
    return true;
  }
  void restoreOccurrence(size_t slot, uint64_t key) {
    if (slot >= 4 || !key || used(slot, key)) return;
    if (consumedCount_[slot] == 128) {
      for (size_t i=1;i<128;++i) consumed_[slot][i-1]=consumed_[slot][i];
      consumedCount_[slot] = 127;
    }
    consumed_[slot][consumedCount_[slot]++] = key;
  }
  void clearOccurrenceCache() {
    for (auto &count : consumedCount_) count = 0;
  }
  void clockValid(bool valid) { clock_ = valid; }
  void reading(size_t slot, const Reading &r) {
    if (slot < 4)
      readings_[slot] = r;
  }
  const Run &run(size_t slot) const { return runs_[slot]; }
  uint8_t activeCount() const {
    uint8_t n = 0;
    for (auto &r : runs_)
      n += r.active;
    return n;
  }
  bool manual(size_t slot, uint32_t now, uint32_t duration, bool test) {
    if (test && duration > 30000) {
      journal.event(slot, "rejected", "test_limit", Source::Test);
      return false;
    }
    return start(slot, now, duration, test ? Source::Test : Source::Manual);
  }
  void stop(size_t slot, uint32_t now, const char *reason = "authorized_stop") {
    if (slot >= 4)
      return;
    io.off(slot);
    if (runs_[slot].active) {
      journal.event(slot, "completed", reason, runs_[slot].source);
      runs_[slot].active = false;
      stopped_[slot] = now;
      hasStopped_[slot] = true;
    }
  }
  void tick(uint32_t now, const Window windows[4]) {
    for (size_t i = 0; i < count_; ++i) {
      auto &z = zones_[i];
      auto &r = runs_[i];
      const auto &w = windows[i];
      if (r.active) {
        if (uint32_t(now - r.began) >= r.duration)
          stop(i, now, "duration_limit");
        else if (r.source == Source::Dynamic || r.source == Source::Hybrid) {
          if (!clock_) {
            journal.event(i, "interrupted", "clock_invalid", r.source);
            r.active = false;
            io.off(i);
            stopped_[i] = now;
            hasStopped_[i] = true;
          } else if (!moisture(i, now)) {
            journal.event(i, "interrupted", "sensor_invalid", r.source);
            r.active = false;
            io.off(i);
            stopped_[i] = now;
            hasStopped_[i] = true;
          } else if (readings_[z.sensor].value >= z.target)
            stop(i, now, "target_reached");
          else if (r.source == Source::Hybrid && !w.open)
            stop(i, now, "window_end");
        }
        if (r.active) continue;
      }
      if (!clock_ || !z.enabled)
        continue;
      if (z.mode != Mode::Dynamic && w.retired) continue;
      if (z.mode == Mode::Hybrid && w.startNow && !used(i, w.occurrence)) {
        if (!moisture(i, now))
          journal.event(i, "omitted", "sensor_invalid", Source::Hybrid);
        else if (readings_[z.sensor].value >= z.below)
          journal.event(i, "omitted", "moisture_sufficient", Source::Hybrid);
      }
      if (z.mode == Mode::Static) {
        if (!w.startNow || !w.occurrence || used(i, w.occurrence))
          continue;
      } else {
        if (!moisture(i, now) || readings_[z.sensor].value >= z.below)
          continue;
        if (hasStopped_[i] && uint32_t(now - stopped_[i]) < z.pauseMs)
          continue;
        if (z.mode == Mode::Hybrid &&
            (!w.open || !w.occurrence || used(i, w.occurrence)))
          continue;
      }
      if (z.mode != Mode::Dynamic) {
        if (!journal.consumeWindow(i, w.occurrence, w.begins)) {
          journal.event(i, "failed", "persistence_failed",
                        z.mode == Mode::Static ? Source::Static
                                               : Source::Hybrid);
          continue;
        }
        restoreOccurrence(i, w.occurrence);
      }
      uint32_t duration = z.maxMs;
      if (z.mode != Mode::Dynamic && w.remainingMs < duration)
        duration = w.remainingMs;
      start(i, now, duration,
            z.mode == Mode::Static    ? Source::Static
            : z.mode == Mode::Dynamic ? Source::Dynamic
                                      : Source::Hybrid);
    }
  }
};
} // namespace irrigation

#pragma once
#include "Controller.h"
#include "TimePolicy.h"
#include <stdint.h>
#include <time.h>
namespace irrigation {
struct Schedule {
  uint8_t zone = 0, weekday = 0;
  int32_t date = 0;
  uint32_t startSecond = 0, durationSecond = 0;
  uint64_t id = 0;
  bool enabled = false;
};
inline int32_t civilDate(const tm &t) {
  return (t.tm_year + 1900) * 10000 + (t.tm_mon + 1) * 100 + t.tm_mday;
}
inline bool validTime(const char *s, uint32_t &seconds) {
  if (!s || !s[0] || !s[1] || s[2] != ':' || !s[3] || !s[4] || s[5])
    return false;
  if (s[0] < '0' || s[0] > '9' || s[1] < '0' || s[1] > '9' || s[3] < '0' ||
      s[3] > '9' || s[4] < '0' || s[4] > '9')
    return false;
  const unsigned h = (s[0] - '0') * 10 + s[1] - '0',
                 m = (s[3] - '0') * 10 + s[4] - '0';
  if (h > 23 || m > 59)
    return false;
  seconds = h * 3600 + m * 60;
  return true;
}
// Timezone supplied by caller; overnight windows include the previous civil
// day.
inline Window evaluate(const Schedule &s, time_t epoch, int64_t previousEpoch,
                       uint32_t revision) {
  Window w;
  if (!s.enabled || !s.durationSecond || s.durationSecond > 86400)
    return w;
  tm local{};
  localtime_r(&epoch, &local);
  for (int offset = 0; offset < 2; ++offset) {
    tm day = local;
    day.tm_hour = 12;
    day.tm_min = 0;
    day.tm_sec = 0;
    day.tm_mday -= offset;
    day.tm_isdst = -1;
    time_t dayEpoch = mktime(&day);
    if (dayEpoch == time_t(-1))
      continue;
    const int weekday = day.tm_wday == 0 ? 7 : day.tm_wday;
    if (s.date ? civilDate(day) != s.date : weekday != s.weekday)
      continue;
    tm start = day;
    start.tm_hour = s.startSecond / 3600;
    start.tm_min = (s.startSecond % 3600) / 60;
    start.tm_sec = s.startSecond % 60;
    start.tm_isdst = -1;
    time_t begins = mktime(&start);
    if (begins == time_t(-1) ||
        unsigned(start.tm_hour) != s.startSecond / 3600 ||
        unsigned(start.tm_min) != (s.startSecond % 3600) / 60)
      continue;
    const int64_t ends = int64_t(begins) + s.durationSecond;
    if (int64_t(epoch) < begins || int64_t(epoch) >= ends)
      continue;
    w.open = true;
    w.begins = int64_t(begins);
    w.startNow = previousEpoch < begins && int64_t(epoch) - begins <= 2;
    w.remainingMs = uint32_t(ends - epoch) * 1000;
    // Stable per schedule/date/revision, FNV based; persistence journal stores
    // it.
    uint64_t key = s.id;
    key ^= uint64_t(civilDate(day));
    key *= 1099511628211ULL;
    key ^= revision;
    w.occurrence = key ? key : 1;
    return w;
  }
  return w;
}
} // namespace irrigation

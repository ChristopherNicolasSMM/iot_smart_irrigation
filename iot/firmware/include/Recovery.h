#pragma once
#include "Schedule.h"
namespace irrigation {
// Count fully closed windows since the durable checkpoint. Weekly history is
// bounded to seven days; explicit dates are inspected even outside that range.
struct RecoveryCount { unsigned windows = 0; bool truncated = false; };
inline RecoveryCount unobserved(const Schedule &s, int64_t from, time_t until) {
  RecoveryCount result;
  if (!s.enabled || !s.durationSecond || s.durationSecond > 86400 ||
      from <= 0 || int64_t(until) <= from) return result;
  const int64_t floor = int64_t(until) - 7 * 86400;
  result.truncated = !s.date && from < floor;
  tm day{};
  localtime_r(&until, &day);
  for (unsigned offset = 0; offset < (s.date ? 1u : 9u); ++offset) {
    tm start = day;
    if (s.date) {
      start.tm_year = s.date / 10000 - 1900;
      start.tm_mon = (s.date / 100) % 100 - 1;
      start.tm_mday = s.date % 100;
    } else start.tm_mday -= offset;
    start.tm_hour = s.startSecond / 3600;
    start.tm_min = (s.startSecond % 3600) / 60;
    start.tm_sec = s.startSecond % 60;
    start.tm_isdst = -1;
    const time_t begins = mktime(&start);
    if (begins == time_t(-1) || unsigned(start.tm_hour) != s.startSecond / 3600 ||
        unsigned(start.tm_min) != (s.startSecond % 3600) / 60) continue;
    if (!s.date && (start.tm_wday == 0 ? 7 : start.tm_wday) != s.weekday) continue;
    if (!s.date && int64_t(begins) < floor) continue;
    if (int64_t(begins) > from && int64_t(begins) + s.durationSecond <= int64_t(until))
      ++result.windows;
  }
  return result;
}
} // namespace irrigation

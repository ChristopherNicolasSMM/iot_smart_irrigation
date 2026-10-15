#pragma once
#include "Schedule.h"
namespace irrigation {
// Count fully closed windows since the durable checkpoint. Weekly history is
// bounded to seven days; explicit dates are inspected even outside that range.
struct RecoveryCount { unsigned windows = 0; bool truncated = false; };
template<class Visit>
inline bool visitUnobserved(const Schedule &s, int64_t from, time_t until,
                            uint32_t revision, Visit visit) {
  if (!s.enabled || !s.durationSecond || s.durationSecond > 86400 ||
      from <= 0 || int64_t(until) <= from) return true;
  const int64_t floor = int64_t(until) - 7 * 86400;
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
    {
      Window window;
      window.begins = int64_t(begins);
      window.occurrence = occurrenceKey(s.id, civilDate(start), revision);
      if (!visit(window)) return false;
    }
  }
  return true;
}
inline RecoveryCount unobserved(const Schedule &s, int64_t from, time_t until) {
  RecoveryCount result;
  if (!s.enabled || !s.durationSecond || s.durationSecond > 86400 ||
      from <= 0 || int64_t(until) <= from) return result;
  result.truncated = !s.date && from < int64_t(until)-7*86400;
  visitUnobserved(s,from,until,0,[&](const Window &) { ++result.windows;return true; });
  return result;
}
} // namespace irrigation

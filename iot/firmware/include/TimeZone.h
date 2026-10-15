#pragma once
#include "TimeZoneData.h"
#include <cstdlib>
#include <cstring>
#include <time.h>
namespace irrigation {
inline const TzRule *activeTimezone = nullptr;
inline const TzRule *findTimezone(const char *name) {
  unsigned lo = 0, hi = timezoneCount;
  while (lo < hi) {
    unsigned mid = (lo + hi) / 2;
    int order = strcmp(name, timezoneNames[mid].name);
    if (!order)
      return timezoneNames[mid].rule;
    if (order < 0)
      hi = mid;
    else
      lo = mid + 1;
  }
  return nullptr;
}
inline bool selectTimezone(const char *name) {
  const TzRule *rule = findTimezone(name);
  if (!rule)
    return false;
  // Keep libc TZ consistent for diagnostics; scheduling uses our pinned rules.
  if (setenv("TZ", rule->future[0] ? rule->future : "UTC0", 1))
    return false;
  tzset();
  activeTimezone = rule;
  return true;
}
inline int64_t civilEpoch(const tm &value);
// TZif POSIX footers in this pinned catalog use M.month.week.day rules.
// Interpret them here so ESP32 Newlib and host libc have identical behavior,
// including negative or >24h transitions and negative daylight saving time.
inline bool tzNumber(const char *&p, int &value) {
  if (*p < '0' || *p > '9')
    return false;
  value = 0;
  while (*p >= '0' && *p <= '9') {
    value = value * 10 + (*p++ - '0');
    if (value > 10000)
      return false;
  }
  return true;
}
inline bool tzName(const char *&p) {
  if (*p == '<') {
    ++p;
    const char *begin = p;
    while (*p && *p != '>')
      ++p;
    if (p == begin || *p != '>')
      return false;
    ++p;
    return true;
  }
  const char *begin = p;
  while ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z'))
    ++p;
  return p - begin >= 3;
}
inline bool tzSeconds(const char *&p, int &seconds) {
  int sign = 1;
  if (*p == '-' || *p == '+') {
    if (*p == '-')
      sign = -1;
    ++p;
  }
  int h = 0, m = 0, s = 0;
  if (!tzNumber(p, h) || h > 167)
    return false;
  if (*p == ':') {
    ++p;
    if (!tzNumber(p, m) || m > 59)
      return false;
    if (*p == ':') {
      ++p;
      if (!tzNumber(p, s) || s > 59)
        return false;
    }
  }
  seconds = sign * (h * 3600 + m * 60 + s);
  return true;
}
inline bool tzBoundary(const char *&p, int year, int offset, int64_t &epoch) {
  int month = 0, week = 0, day = 0, seconds = 7200;
  if (*p++ != 'M' || !tzNumber(p, month) || *p++ != '.' || !tzNumber(p, week) ||
      *p++ != '.' || !tzNumber(p, day) || month < 1 || month > 12 || week < 1 ||
      week > 5 || day > 6)
    return false;
  if (*p == '/') {
    ++p;
    if (!tzSeconds(p, seconds))
      return false;
  }
  tm first{};
  first.tm_year = year - 1900;
  first.tm_mon = month - 1;
  first.tm_mday = 1;
  const int64_t base = civilEpoch(first);
  const int weekday = int(((base / 86400 + 4) % 7 + 7) % 7);
  int date = 1 + (day - weekday + 7) % 7 + (week - 1) * 7;
  tm next = first;
  next.tm_mon++;
  const int days = int((civilEpoch(next) - base) / 86400);
  if (date > days)
    date -= 7;
  epoch = base + (date - 1) * 86400LL + seconds - offset;
  return true;
}
inline bool tzFuture(const char *rule, time_t epoch, int32_t &offset,
                     bool &dst) {
  const char *p = rule;
  int standard = 0, summer = 0;
  if (!tzName(p) || !tzSeconds(p, standard))
    return false;
  standard = -standard;
  offset = standard;
  dst = false;
  if (!*p)
    return true;
  if (!tzName(p))
    return false;
  summer = standard + 3600;
  if (*p != ',') {
    int west = 0;
    if (!tzSeconds(p, west))
      return false;
    summer = -west;
  }
  tm utc{};
  if (!gmtime_r(&epoch, &utc))
    return false;
  // Shifted boundaries may fall in an adjacent UTC year. Use the most recent
  // transition across neighboring civil years, rather than a month shortcut.
  bool found = false;
  int64_t latest = 0;
  for (int year = utc.tm_year + 1899; year <= utc.tm_year + 1901; ++year) {
    const char *bound = p;
    int64_t begin = 0, end = 0;
    if (*bound++ != ',' || !tzBoundary(bound, year, standard, begin) ||
        *bound++ != ',' || !tzBoundary(bound, year, summer, end) || *bound)
      return false;
    if (begin <= int64_t(epoch) && (!found || begin > latest)) {
      latest = begin;
      dst = true;
      found = true;
    }
    if (end <= int64_t(epoch) && (!found || end > latest)) {
      latest = end;
      dst = false;
      found = true;
    }
  }
  if (!found)
    return false;
  offset = dst ? summer : standard;
  return true;
}

inline tm *zoneLocaltime(const time_t *epoch, tm *out) {
  if (!activeTimezone)
    return localtime_r(epoch, out);
  const TzRule &rule = *activeTimezone;
  if (int64_t(*epoch) < rule.transitions[0].epoch)
    return nullptr;
  if (rule.future[0] &&
      int64_t(*epoch) > rule.transitions[rule.count - 1].epoch) {
    int32_t offset = 0;
    bool dst = false;
    if (!tzFuture(rule.future, *epoch, offset, dst))
      return nullptr;
    time_t wall = *epoch + offset;
    if (!gmtime_r(&wall, out))
      return nullptr;
    out->tm_isdst = dst;
    return out;
  }
  unsigned lo = 0, hi = rule.count;
  while (lo + 1 < hi) {
    unsigned mid = (lo + hi) / 2;
    if (rule.transitions[mid].epoch <= int64_t(*epoch))
      lo = mid;
    else
      hi = mid;
  }
  const auto &transition = rule.transitions[lo];
  time_t wall = *epoch + transition.offset;
  if (!gmtime_r(&wall, out))
    return nullptr;
  out->tm_isdst = transition.dst;
  return out;
}
inline int64_t floorDivision(int64_t x, int64_t y) {
  const auto q = x / y, r = x % y;
  return q - (r < 0);
}
inline int64_t civilEpoch(const tm &value) {
  int64_t year = value.tm_year + 1900, month = value.tm_mon;
  year += floorDivision(month, 12);
  month -= floorDivision(month, 12) * 12;
  month += 1;
  year -= month <= 2;
  const int64_t era = floorDivision(year, 400), yoe = year - era * 400;
  const int64_t doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5;
  const int64_t days = era * 146097 + yoe * 365 + yoe / 4 - yoe / 100 + doy -
                       719468 + value.tm_mday - 1;
  return days * 86400 + value.tm_hour * 3600 + value.tm_min * 60 + value.tm_sec;
}
inline bool sameCivil(const tm &a, const tm &b) {
  return a.tm_year == b.tm_year && a.tm_mon == b.tm_mon &&
         a.tm_mday == b.tm_mday && a.tm_hour == b.tm_hour &&
         a.tm_min == b.tm_min && a.tm_sec == b.tm_sec;
}
inline time_t zoneMktime(tm *value) {
  if (!activeTimezone)
    return mktime(value);
  // Enumerate valid offsets, validating each against the IANA history/future.
  // Gaps have no candidate. Folds always choose the first instant, on all
  // libcs.
  const time_t wall = time_t(civilEpoch(*value));
  tm target{};
  if (!gmtime_r(&wall, &target))
    return time_t(-1);
  bool found = false;
  time_t best = 0;
  tm selected{};
  for (unsigned i = 0; i < activeTimezone->offsetCount; ++i) {
    time_t candidate = wall - activeTimezone->offsets[i];
    tm local{};
    if (zoneLocaltime(&candidate, &local) && sameCivil(local, target) &&
        (value->tm_isdst < 0 || value->tm_isdst == local.tm_isdst) &&
        (!found || candidate < best)) {
      found = true;
      best = candidate;
      selected = local;
    }
  }
  if (!found)
    return time_t(-1);
  *value = selected;
  return best;
}
} // namespace irrigation

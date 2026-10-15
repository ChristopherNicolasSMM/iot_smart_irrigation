#include "Recovery.h"
#include "Schedule.h"
#include <cassert>
#include <cstring>
using namespace irrigation;
time_t utc(int y, int m, int d, int h = 0, int minute = 0) {
  tm t{};
  t.tm_year = y - 1900;
  t.tm_mon = m - 1;
  t.tm_mday = d;
  t.tm_hour = h;
  t.tm_min = minute;
  return civilEpoch(t);
}
time_t local(int y, int m, int d, int h, int minute) {
  tm t{};
  t.tm_year = y - 1900;
  t.tm_mon = m - 1;
  t.tm_mday = d;
  t.tm_hour = h;
  t.tm_min = minute;
  t.tm_isdst = -1;
  return zoneMktime(&t);
}
int main() {
  assert(timezoneCount > 500);
  assert(!selectTimezone("invalid/Timezone"));
  assert(selectTimezone("America/Sao_Paulo"));
  assert(local(2040, 1, 1, 0, 0) == utc(2040, 1, 1, 3));
  assert(selectTimezone("America/New_York"));
  assert(local(2026, 3, 8, 2, 30) == time_t(-1));
  assert(local(2026, 11, 1, 1, 30) == utc(2026, 11, 1, 5, 30));
  // Future recurring rules go beyond the explicit TZif transitions.
  assert(local(2100, 7, 1, 12, 0) == utc(2100, 7, 1, 16, 0));
  assert(local(9999, 7, 1, 12, 0) == utc(9999, 7, 1, 16, 0));
  Schedule s;
  s.enabled = true;
  s.id = 99;
  s.date = 20261101;
  s.startSecond = 5400;
  s.durationSecond = 1200;
  assert(evaluate(s, utc(2026, 11, 1, 5, 30), utc(2026, 11, 1, 5, 29), 1)
             .startNow);
  assert(
      !evaluate(s, utc(2026, 11, 1, 6, 30), utc(2026, 11, 1, 6, 29), 1).open);
  assert(selectTimezone("Australia/Lord_Howe"));
  assert(local(2026, 10, 4, 2, 15) == time_t(-1));
  assert(local(2026, 4, 5, 1, 45) == utc(2026, 4, 4, 14, 45));
  assert(selectTimezone("Pacific/Chatham"));
  assert(local(2026, 1, 1, 12, 0) == utc(2025, 12, 31, 22, 15));
  assert(selectTimezone("Asia/Kathmandu"));
  assert(local(2040, 1, 1, 0, 0) == utc(2039, 12, 31, 18, 15));
  assert(selectTimezone("Europe/London"));
  assert(local(2026, 3, 29, 1, 30) == time_t(-1));
  assert(selectTimezone("UTC"));
  assert(local(2020, 2, 29, 12, 0) == utc(2020, 2, 29, 12));
}

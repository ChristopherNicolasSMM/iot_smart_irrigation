#pragma once
#include <stdint.h>
#include <time.h>
#include <limits>
#include "TimeZone.h"
namespace irrigation {
// Do not emulate a 64-bit clock on top of a 32-bit libc/SNTP/TLS clock.
static_assert(sizeof(time_t) >= 8 && std::numeric_limits<time_t>::is_signed,
              "Firmware requires signed 64-bit time_t (Arduino-ESP32 3.x / ESP-IDF 5+)");
constexpr int minCalendarYear = 2020;
constexpr int maxCalendarYear = 9999; // four-digit ISO dates, not a CPU limit
constexpr int64_t minClockEpoch = 1577836800LL;
constexpr int64_t maxClockEpoch = 253402300799LL;
inline bool validClockEpoch(int64_t epoch) {
  return epoch >= minClockEpoch && epoch <= maxClockEpoch;
}
inline bool validCivilDate(int year, int month, int day) {
  if (year < minCalendarYear || year > maxCalendarYear ||
      month < 1 || month > 12 || day < 1 || day > 31) return false;
  const unsigned days[]={31,28,31,30,31,30,31,31,30,31,30,31};
  const bool leap=year%4==0 && (year%100!=0 || year%400==0);
  return unsigned(day)<=days[month-1]+(month==2 && leap);
}
} // namespace irrigation

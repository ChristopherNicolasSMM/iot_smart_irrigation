#include "TimeZone.h"
#include <iostream>
#include <string>
int main() {
  std::string name;
  int64_t stamp;
  while (std::cin >> name >> stamp) {
    if (!irrigation::selectTimezone(name.c_str()))
      return 1;
    time_t epoch = stamp;
    tm local{};
    if (!irrigation::zoneLocaltime(&epoch, &local))
      return 2;
    std::cout << irrigation::civilEpoch(local) - stamp << ' ' << local.tm_isdst
              << '\n';
  }
  return 0;
}

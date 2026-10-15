#include "Recovery.h"
#include "TimePolicy.h"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
using namespace irrigation;
time_t instant(int y, int m, int d, int h=0, int min=0, int sec=0) {
  tm t{}; t.tm_year=y-1900; t.tm_mon=m-1; t.tm_mday=d;
  t.tm_hour=h; t.tm_min=min; t.tm_sec=sec; t.tm_isdst=-1;
  return mktime(&t);
}
void checkSchedule(int year) {
  Schedule s; s.enabled=true; s.date=year*10000+101;
  s.startSecond=3600; s.durationSecond=60; s.id=9;
  const auto start=instant(year,1,1,1);
  auto w=evaluate(s,start,int64_t(start)-1,1);
  assert(w.open && w.startNow && w.remainingMs==60000 && w.occurrence);
  assert(unobserved(s,int64_t(start)-1,start+60).windows==1);
  assert(!evaluate(s,start+60,start,1).open);
}
int main() {
  setenv("TZ","UTC0",1);tzset();
  assert(instant(2038,1,19,3,14,7)==2147483647LL);
  assert(instant(2038,1,19,3,14,8)==2147483648LL);
  assert(validClockEpoch(2147483648LL));
  assert(validClockEpoch(minClockEpoch) && validClockEpoch(maxClockEpoch));
  assert(!validClockEpoch(minClockEpoch-1) && !validClockEpoch(maxClockEpoch+1));
  assert(!validClockEpoch(-2147483648LL)); // wrapped 32-bit epoch must not pass
  for (int y : {2038,2040,2100,2400,9999}) checkSchedule(y);
  assert(validCivilDate(2040,2,29) && validCivilDate(2400,2,29));
  assert(!validCivilDate(2100,2,29) && !validCivilDate(2026,2,29));
  assert(validCivilDate(9999,12,31) && !validCivilDate(10000,1,1));
  assert(!validCivilDate(2019,12,31) && !validCivilDate(2040,4,31));
  const time_t end=maxClockEpoch;
  tm utc{}; assert(gmtime_r(&end,&utc));
  char iso[25]; assert(strftime(iso,sizeof(iso),"%Y-%m-%dT%H:%M:%SZ",&utc));
  assert(!strcmp(iso,"9999-12-31T23:59:59Z"));
  Schedule overnight; overnight.enabled=true;overnight.date=20380118;
  overnight.startSecond=23*3600;overnight.durationSecond=6*3600;
  const auto crossing=instant(2038,1,19,3,14,8);
  auto w=evaluate(overnight,crossing,crossing,1);
  assert(w.open && !w.startNow && w.remainingMs>0);
  setenv("TZ","BRT3",1);tzset();
  checkSchedule(2040);checkSchedule(2100);
  const auto brt=instant(2040,1,1);
  assert(gmtime_r(&brt,&utc) && utc.tm_hour==3);
  // Far recovery is bounded; a specific old date remains reported.
  overnight.date=20400101;
  assert(unobserved(overnight,instant(2039,12,31),instant(2100,1,1)).windows==1);
}

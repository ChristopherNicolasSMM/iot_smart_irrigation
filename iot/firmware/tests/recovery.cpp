#include "Recovery.h"
#include <cassert>
#include <cstdlib>
using namespace irrigation;
struct Output : Backend {
  unsigned starts = 0;
  bool on(size_t, uint32_t) override { ++starts; return true; }
  void off(size_t) override {}
};
struct Log : Journal {
  bool consume(size_t, uint64_t) override { return true; }
  void event(size_t, const char *, const char *, Source) override {}
};
time_t at(int y,int m,int d,int h=0) {
  tm t{};t.tm_year=y-1900;t.tm_mon=m-1;t.tm_mday=d;t.tm_hour=h;t.tm_isdst=-1;
  return mktime(&t);
}
int main() {
  setenv("TZ","UTC0",1);tzset();
  Schedule s;s.enabled=true;s.durationSecond=3600;s.startSecond=3600;
  s.date=20260102;
  assert(unobserved(s,at(2026,1,1),at(2026,4,1)).windows==1);
  assert(unobserved(s,at(2026,1,2,1),at(2026,4,1)).windows==0);
  assert(unobserved(s,at(2026,1,1),at(2026,1,2,1)).windows==0);
  assert(unobserved(s,at(2026,1,1),at(2026,1,2,2)).windows==1);
  assert(unobserved(s,at(2026,4,1),at(2026,1,1)).windows==0);
  s.date=0;s.weekday=5;
  auto r=unobserved(s,at(2026,1,1),at(2026,4,1));
  assert(r.windows==1 && r.truncated);
  s.enabled=false;assert(unobserved(s,at(2026,1,1),at(2026,4,1)).windows==0);
  setenv("TZ","BRT3",1);tzset();
  s.enabled=true;s.date=20260102;s.startSecond=23*3600;s.durationSecond=7200;
  assert(unobserved(s,at(2026,1,1),at(2026,1,3)).windows==0);
  assert(unobserved(s,at(2026,1,1),at(2026,1,3,1)).windows==1);
  Output output; Log log; Controller controller(output, log);
  Zone zones[1]; zones[0].enabled=true; zones[0].mode=Mode::Hybrid;
  zones[0].sensor=0;
  assert(controller.configure(zones,1,1));
  controller.clockValid(true);
  controller.reading(0, Reading{10,100,true});
  Window windows[4];
  windows[0]=evaluate(s,at(2026,1,3),at(2026,1,3),1);
  assert(windows[0].open && !windows[0].startNow);
  controller.restoreOccurrence(0,windows[0].occurrence);
  controller.tick(100,windows);controller.tick(200,windows);
  assert(output.starts==0); // recovery consumption also blocks later hybrid ticks
}

#include "ScheduleDiagnostics.h"
#include "Recovery.h"
#include "Controller.h"
#include <cassert>
#include <cstdlib>
using namespace irrigation;
time_t at(int y,int m,int d,int h=0) {
  tm t{};t.tm_year=y-1900;t.tm_mon=m-1;t.tm_mday=d;t.tm_hour=h;t.tm_isdst=-1;
  return mktime(&t);
}
struct IO:Backend {
  unsigned starts=0;bool on(size_t,uint32_t) override {++starts;return true;}
  void off(size_t) override {}
};
struct Log:Journal {
  bool consume(size_t,uint64_t) override {return true;}
  void event(size_t,const char*,const char*,Source) override {}
};
int main() {
  setenv("TZ","UTC0",1);tzset();
  Schedule first;first.enabled=true;first.id=10;first.date=20400101;
  first.startSecond=3600;first.durationSecond=3600;
  Schedule next=first;next.id=20;
  auto a=evaluate(first,at(2040,1,1,1),at(2040,1,1),4);
  auto b=evaluate(next,at(2040,1,1,1),at(2040,1,1),4);
  assert(preferWindow(a,b)); // preserve existing tie policy: last startNow wins
  assert(scheduleConflict(a,b,false));
  assert(!scheduleConflict(b,b,false));
  assert(scheduleConflict(a,Window{},true)); // manual run in progress
  b.retired=true;assert(!preferWindow(a,b));assert(!scheduleConflict(b,a,false));
  assert(diagnosticKey(first,a,4,"schedule_overlap")==diagnosticKey(first,a,4,"schedule_overlap"));
  assert(diagnosticKey(first,a,4,"schedule_overlap")!=diagnosticKey(first,a,5,"schedule_overlap"));
  assert(diagnosticKey(first,a,4,"schedule_overlap")!=diagnosticKey(first,a,4,"unobserved_window"));
  unsigned visits=0;Window recovered;
  assert(visitUnobserved(first,at(2039,12,31),at(2041,1,1),4,[&](const Window &w){++visits;recovered=w;return true;}));
  assert(visits==1 && recovered.occurrence==a.occurrence && recovered.begins==a.begins);
  assert(!visitUnobserved(first,at(2039,12,31),at(2041,1,1),4,[](const Window&){return false;}));
  Run run;run.active=true;run.began=0xfffffff0u;run.duration=1000;
  assert(runBlocksSchedule(run,983));assert(!runBlocksSchedule(run,984));
  // At the exact deadline adjacent windows must not be marked as overlapping.
  run.began=0;assert(!runBlocksSchedule(run,1000));
  // An active hybrid must close at its original window even if another is open.
  IO io;Log log;Controller controller(io,log);Zone zones[1];zones[0].enabled=true;
  zones[0].mode=Mode::Hybrid;zones[0].sensor=0;
  assert(controller.configure(zones,1,1));controller.clockValid(true);
  controller.reading(0,Reading{10,0,true});Window windows[4];windows[0]=a;
  controller.tick(0,windows);assert(io.starts==1);
  Schedule schedules[2]={first,next};Window candidates[2]={Window{},a};
  Window planned[4];int selected[4];int activeSlots[4]={0,-1,-1,-1};
  selectScheduleWindows(schedules,2,candidates,controller,1,activeSlots,1,planned,selected);
  assert(selected[0]==0 && !planned[0].open); // original expired, other still open
  controller.tick(1,planned);assert(!controller.run(0).active);
  selectScheduleWindows(schedules,2,candidates,controller,1,activeSlots,2,planned,selected);
  assert(selected[0]==1 && planned[0].open);
  assert(controller.manual(0,10,1000,true));
  selectScheduleWindows(schedules,2,candidates,controller,1,activeSlots,1009,planned,selected);
  assert(selected[0]==-1 && !planned[0].open);
  selectScheduleWindows(schedules,2,candidates,controller,1,activeSlots,1010,planned,selected);
  assert(selected[0]==1 && planned[0].open); // adjacent window is eligible
  IO adjacentIO;Log adjacentLog;Controller adjacent(adjacentIO,adjacentLog);
  Zone stat[1];stat[0].enabled=true;stat[0].maxMs=1000;
  assert(adjacent.configure(stat,1,1));adjacent.clockValid(true);
  Window consecutive[4];consecutive[0].open=true;consecutive[0].startNow=true;
  consecutive[0].remainingMs=1000;consecutive[0].occurrence=100;
  adjacent.tick(0,consecutive);assert(adjacentIO.starts==1);
  consecutive[0].occurrence=200;
  adjacent.tick(1000,consecutive);
  assert(adjacentIO.starts==2 && adjacent.run(0).active);
}

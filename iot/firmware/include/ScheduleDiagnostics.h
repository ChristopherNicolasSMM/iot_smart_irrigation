#pragma once
#include "Schedule.h"
namespace irrigation {
inline bool runBlocksSchedule(const Run &run,uint32_t now) {
  return run.active && uint32_t(now-run.began)<run.duration;
}
inline uint64_t diagnosticKey(const Schedule &s, const Window &w,
                              uint32_t revision, const char *reason) {
  uint64_t key=s.id ^ uint64_t(w.begins);
  key=(key ^ revision)*1099511628211ULL;
  for (const char *p=reason;*p;++p) key=(key ^ uint8_t(*p))*1099511628211ULL;
  return key ? key : 1;
}
inline bool preferWindow(const Window &current,const Window &next) {
  return next.open && (!current.open ||
      (!next.retired && (next.startNow || current.retired)));
}
// A running hybrid follows its own original window, never another overlapping
// agenda. Manual/test/dynamic runs have no schedule window to attach.
inline bool scheduleConflict(const Window &candidate,const Window &chosen,
                             bool running) {
  return candidate.open && candidate.startNow && !candidate.retired &&
         (running ? candidate.occurrence!=chosen.occurrence
                  : chosen.open && candidate.occurrence!=chosen.occurrence);
}
inline void selectScheduleWindows(const Schedule *slots,size_t count,
    const Window *candidates,const Controller &controller,size_t zones,
    const int *activeSlots,uint32_t now,Window *windows,int *selected) {
  for (size_t z=0;z<4;++z) { windows[z]=Window{};selected[z]=-1; }
  for (size_t i=0;i<count;++i) {
    const auto zone=slots[i].zone;
    if (preferWindow(windows[zone],candidates[i])) {
      windows[zone]=candidates[i];selected[zone]=int(i);
    }
  }
  for (size_t z=0;z<zones;++z) {
    const auto &run=controller.run(z);
    if (!runBlocksSchedule(run,now)) continue;
    const int active=(run.source==Source::Static || run.source==Source::Hybrid)
                         ? activeSlots[z] : -1;
    selected[z]=active;
    windows[z]=active>=0 && size_t(active)<count ? candidates[active] : Window{};
  }
}
} // namespace irrigation

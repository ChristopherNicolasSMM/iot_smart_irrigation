#include "AuditReplay.h"
#include <cassert>
using namespace irrigation;
int main() {
  AuditReplay state;
  assert(state.fence(100, 0));
  assert(state.cursor == 36 && state.overwritten == 36);
  auto persisted = state;
  assert(state.fence(100, 0));
  assert(state.overwritten == 36);
  assert(persisted.cursor == state.cursor);
  assert(state.fence(110, 0));
  assert(state.cursor == 46 && state.overwritten == 46);
  state.cursor = 60;
  assert(state.fence(110, 100));
  assert(state.cursor == 100 && state.overwritten == 46);
  assert(state.fence(0, 0));
  state.cursor = 0;
  state.overwritten = UINT64_MAX;
  assert(!state.fence(100, 0));
}

#include "RunDeadline.h"
#include <cassert>
int main() {
  irrigation::RunDeadline d;
  assert(!d.start(0, 0, 30000));
  assert(!d.start(0, 30001, 30000));
  assert(d.start(0xfffffff0u, 32, 30000));
  assert(!d.start(0, 1, 30000));
  assert(!d.expired(0x0fu));
  assert(d.expired(0x10u));
  d.stop();
  d.stop();
  assert(!d.active());
  assert(!d.expired(100));
  assert(d.start(100, 5000, 30000));
  assert(!d.expired(5099));
  assert(d.expired(5100));
}

#include "QueueMeta.h"
#include <cassert>
using namespace irrigation;
int main() {
  QueueMeta q;
  for (unsigned i = 0; i < 16; ++i)
    assert(q.enqueue());
  assert(!q.enqueue());
  assert(q.count == 16);
  QueueCancellation c;
  c.digest = std::string(64, 'a');
  c.reason = "administrative_cancel";
  c.messageId = "synthetic";
  for (unsigned i = 0; i < 10; ++i)
    assert(q.cancel(c));
  assert(q.cancelled == 10 && q.count == 6 && q.head == 10);
  assert(q.cancellations[1].generation == 26);
  while (q.count)
    assert(q.pop());
  assert(!q.cancel(c));
  q.generation = UINT64_MAX;
  assert(!q.enqueue());
  assert(!q.valid());
}

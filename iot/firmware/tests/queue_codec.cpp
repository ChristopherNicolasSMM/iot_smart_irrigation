#include "QueueCodec.h"
#include <cassert>
using namespace irrigation;
int main() {
  QueueMeta meta;
  for (unsigned i = 0; i < 12; ++i) {
    assert(meta.enqueue());
    QueueCancellation e;
    e.digest = std::string(64, 'a');
    e.reason = "Retirado pelo administrador";
    e.messageId = "same-id";
    e.path = "/iot/v1/telemetry";
    assert(meta.cancel(e));
  }
  DynamicJsonDocument doc(6144);
  queueMetaJson(doc.to<JsonObject>(), meta);
  assert(!doc.overflowed());
  QueueMeta parsed;
  assert(decodeQueueMeta(doc.as<JsonObjectConst>(), parsed));
  assert(parsed.count == 0 && parsed.cancelled == 12 &&
         parsed.generation == 24 &&
         parsed.cancellations[3].messageId == "same-id");
  doc["count"] = 17;
  assert(!decodeQueueMeta(doc.as<JsonObjectConst>(), parsed));
  doc["count"] = 0;
  doc["cancellations"][0]["reason"] = "";
  assert(!decodeQueueMeta(doc.as<JsonObjectConst>(), parsed));
}

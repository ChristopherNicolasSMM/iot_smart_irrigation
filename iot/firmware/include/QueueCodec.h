#pragma once
#include "QueueMeta.h"
#include <ArduinoJson.h>
namespace irrigation {
inline void queueMetaJson(JsonObject obj, const QueueMeta &meta) {
  obj["schema_version"] = 1;
  obj["head"] = meta.head;
  obj["count"] = meta.count;
  obj["generation"] = meta.generation;
  obj["cancelled"] = meta.cancelled;
  JsonArray log = obj.createNestedArray("cancellations");
  const uint64_t first = meta.cancelled > 8 ? meta.cancelled - 8 : 0;
  for (uint64_t i = first; i < meta.cancelled; ++i) {
    const auto &entry = meta.cancellations[i % 8];
    JsonObject row = log.createNestedObject();
    row["digest"] = entry.digest.c_str();
    row["message_id"] = entry.messageId.c_str();
    row["path"] = entry.path.c_str();
    row["reason"] = entry.reason.c_str();
    row["recorded_at"] = entry.recordedAt.c_str();
    row["generation"] = entry.generation;
    row["actor"] = "admin";
  }
}
inline bool decodeQueueMeta(JsonObjectConst obj, QueueMeta &meta) {
  if (obj["schema_version"] != 1 || !obj["head"].is<unsigned>() ||
      !obj["count"].is<unsigned>() || !obj["generation"].is<uint64_t>() ||
      !obj["cancelled"].is<uint64_t>() ||
      !obj["cancellations"].is<JsonArrayConst>())
    return false;
  QueueMeta parsed;
  parsed.head = obj["head"];
  parsed.count = obj["count"];
  parsed.generation = obj["generation"];
  parsed.cancelled = obj["cancelled"];
  if (!parsed.valid() || obj["cancellations"].size() !=
                             (parsed.cancelled > 8 ? 8 : parsed.cancelled))
    return false;
  uint64_t index = parsed.cancelled > 8 ? parsed.cancelled - 8 : 0;
  for (JsonObjectConst row : obj["cancellations"].as<JsonArrayConst>()) {
    if (!row["digest"].is<const char *>() ||
        !row["reason"].is<const char *>() ||
        !row["message_id"].is<const char *>() ||
        !row["path"].is<const char *>() ||
        !row["recorded_at"].is<const char *>() ||
        !row["generation"].is<uint64_t>())
      return false;
    auto &entry = parsed.cancellations[index++ % 8];
    entry.digest = row["digest"].as<std::string>();
    entry.reason = row["reason"].as<std::string>();
    entry.messageId = row["message_id"].as<std::string>();
    entry.path = row["path"].as<std::string>();
    entry.recordedAt = row["recorded_at"].as<std::string>();
    entry.generation = row["generation"];
    if (entry.digest.size() != 64 || entry.reason.empty() ||
        entry.reason.size() > 120 || entry.path.size() > 160 ||
        entry.messageId.size() > 100 || entry.recordedAt.size() > 25 ||
        entry.generation > parsed.generation || !entry.generation)
      return false;
  }
  meta = parsed;
  return true;
}
} // namespace irrigation

#include "Auth.h"
#include "../portal/LocalPortalPage.h"
#include "Controller.h"
#include "FirmwareConfig.h"
#include "HardwareConfig.h"
#include "JsonSpan.h"
#include "Provision.h"
#include "RemoteProtocol.h"
#include "Recovery.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <atomic>
#include <ctype.h>
#include <esp_sntp.h>
#include <esp_system.h>
#include <mbedtls/base64.h>
#include <mbedtls/sha256.h>
#include <memory>
#include <time.h>
#if !IRRIGATION_ROLE_MATRIX
#include <DallasTemperature.h>
#include <OneWire.h>
OneWire oneWire(hardware::temperaturePin);
DallasTemperature temperatures(&oneWire);
#endif
#ifndef IRRIGATION_REMOTE_NODE
#define IRRIGATION_REMOTE_NODE 0
#endif
using namespace irrigation;
constexpr size_t bodyLimit = 16384;
constexpr uint32_t linkLeaseMs = 3000;
Preferences store;
SemaphoreHandle_t storeMutex;
QueueHandle_t incoming;
WebServer web(80);
HardwareSerial remoteSerial(2);
wire::Parser remoteParser;
const char *actuatorBinding[4] = {IOT_ACTUATOR_ID_1, IOT_ACTUATOR_ID_2,
                                  IOT_ACTUATOR_ID_3, IOT_ACTUATOR_ID_4};
const char *moistureBinding[4] = {IOT_MOISTURE_ID_1, IOT_MOISTURE_ID_2,
                                  IOT_MOISTURE_ID_3, IOT_MOISTURE_ID_4};
const char *temperatureBinding[4] = {IOT_TEMPERATURE_ID_1, IOT_TEMPERATURE_ID_2,
                                     IOT_TEMPERATURE_ID_3,
                                     IOT_TEMPERATURE_ID_4};
FirmwareConfig config;
wire::Lease nodeLease;
std::atomic<bool> authBlocked{false}, clockSynchronized{false};
String guid, bootId, configRaw, configHash, csrf;
uint64_t nodeTokens[4] = {}, nodeBootToken = 0;
uint32_t revision = 0, sequence = 0, session = 0, remoteSequence = 0,
         lostEvents = 0;
bool remoteCommands[4] = {};
String commands[4]; // command associated with each running zone
uint8_t journalCount = 0;
uint32_t lastTick = 0, lastSample = 0, lastPoll = 0, lastRead = 0;
int64_t previousEpoch = 0;
bool recoveryPending = true;
uint32_t checkpointAt = 0;
bool storageReady = false, bootHold = true;
Reading moisture[4], tempReadings[4];
String temperatureRom[4];
struct Input {
  String kind, body;
};
String uuid() {
  uint8_t b[16];
  esp_fill_random(b, 16);
  b[6] = (b[6] & 15) | 64;
  b[8] = (b[8] & 63) | 128;
  char s[37];
  snprintf(
      s, sizeof(s),
      "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
      b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7], b[8], b[9], b[10], b[11],
      b[12], b[13], b[14], b[15]);
  return s;
}
String iso(time_t now) {
  tm t{};
  gmtime_r(&now, &t);
  char s[25];
  strftime(s, sizeof(s), "%Y-%m-%dT%H:%M:%SZ", &t);
  return s;
}
bool clockReady() {
  time_t n = time(nullptr);
  return clockSynchronized.load() && int64_t(n) >= 1767225600LL &&
         int64_t(n) <= 2147483646LL;
}
String digest(const String &raw) {
  uint8_t hash[32];
  mbedtls_sha256_ret(reinterpret_cast<const unsigned char *>(raw.c_str()),
                     raw.length(), hash, 0);
  char out[65];
  for (int i = 0; i < 32; ++i)
    snprintf(out + i * 2, 3, "%02x", hash[i]);
  return String(out);
}
bool lockStore() {
  return xSemaphoreTake(storeMutex, pdMS_TO_TICKS(50)) == pdTRUE;
}
void unlockStore() { xSemaphoreGive(storeMutex); }
bool queueJob(const String &path, const String &body) {
  if (body.length() > 4096 || !storageReady || !lockStore()) {
    ++lostEvents;
    return false;
  }
  unsigned count = 0, head = 0;
  String meta = store.getString("qmeta", "0,0");
  bool valid = sscanf(meta.c_str(), "%u,%u", &head, &count) == 2 && head < 16 &&
               count <= 16;
  bool ok = false;
  if (valid && count < 16) {
    DynamicJsonDocument j(body.length() + 512);
    j["path"] = path;
    j["body"] = body;
    String raw;
    serializeJson(j, raw);
    String key = "q" + String((head + count) % 16);
    ok = store.putString(key.c_str(), raw) == raw.length() &&
         store.putString("qmeta", String(head) + "," + String(count + 1)) > 0;
  }
  unlockStore();
  if (!ok)
    ++lostEvents;
  return ok;
}
const char *sourceName(Source s) {
  switch (s) {
  case Source::Static:
    return "static";
  case Source::Dynamic:
    return "dynamic";
  case Source::Hybrid:
    return "hybrid";
  case Source::Test:
    return "test";
  default:
    return "manual";
  }
}
bool commandState(const String &id, const String &status, int64_t expiry = 0) {
  if (!lockStore())
    return false;
  unsigned count = store.getUChar("ccount", 0);
  int row = -1, reusable = -1;
  String prior;
  for (unsigned i = 0; i < count && i < 32; ++i) {
    String key = "c" + String(i), raw = store.getString(key.c_str(), "");
    DynamicJsonDocument d(1024);
    deserializeJson(d, raw);
    String state = d["status"].as<String>();
    if (d["id"].as<String>() == id) {
      row = i;
      prior = raw;
      break;
    }
    if (clockReady() && state != "started" && state != "received" &&
        d["expires_at"].as<int64_t>() > 0 &&
        d["expires_at"].as<int64_t>() < time(nullptr))
      reusable = i;
  }
  if (row < 0) {
    if (count < 32)
      row = count++;
    else
      row = reusable;
  }
  bool ok = false;
  if (row >= 0) {
    DynamicJsonDocument d(1024);
    if (prior.length())
      deserializeJson(d, prior);
    d["id"] = id;
    d["status"] = status;
    if (expiry)
      d["expires_at"] = expiry;
    if (status == "started" && d["started_at"].isNull() && clockReady())
      d["started_at"] = iso(time(nullptr));
    if (status != "received" && status != "started" &&
        d["terminal_at"].isNull() && clockReady())
      d["terminal_at"] = iso(time(nullptr));
    String raw;
    serializeJson(d, raw);
    String key = "c" + String(row);
    ok = store.putString(key.c_str(), raw) == raw.length() &&
         store.putUChar("ccount", count) == 1;
  }
  unlockStore();
  return ok;
}
String commandTime(const String &id, const String &status) {
  if (!lockStore())
    return String();
  String timestamp;
  unsigned count = store.getUChar("ccount", 0);
  for (unsigned i = 0; i < count && i < 32; ++i) {
    DynamicJsonDocument d(1024);
    String key = "c" + String(i);
    deserializeJson(d, store.getString(key.c_str(), ""));
    if (d["id"].as<String>() == id) {
      timestamp =
          d[status == "started" ? "started_at" : "terminal_at"].as<String>();
      break;
    }
  }
  unlockStore();
  return timestamp;
}
String previousCommand(const String &id) {
  if (!lockStore())
    return "storage_fault";
  String status;
  unsigned count = store.getUChar("ccount", 0);
  for (unsigned i = 0; i < count; ++i) {
    DynamicJsonDocument d(768);
    String key = "c" + String(i);
    deserializeJson(d, store.getString(key.c_str(), ""));
    if (d["id"].as<String>() == id) {
      status = d["status"].as<String>();
      break;
    }
  }
  unlockStore();
  return status;
}
void commandAck(const String &id, const char *status, const char *reason) {
  DynamicJsonDocument d(512);
  d["status"] = status;
  d["reason"] = reason;
  String when = commandTime(id, status);
  if (when.length())
    d["executed_at"] = when;
  else if (clockReady())
    d["executed_at"] = iso(time(nullptr));
  String raw;
  serializeJson(d, raw);
  queueJob("/iot/v1/commands/" + id + "/events", raw);
}
void txFrame(const wire::Frame &frame) {
  uint8_t bytes[wire::frameSize];
  wire::encode(frame, bytes);
  digitalWrite(IOT_RS485_DE, HIGH);
  remoteSerial.write(bytes, sizeof(bytes));
  remoteSerial.flush();
  digitalWrite(IOT_RS485_DE, LOW);
}
bool exchange(wire::Frame request, wire::Frame &reply) {
  if (!IOT_RS485_ENABLED)
    return false;
  while (remoteSerial.available())
    remoteSerial.read();
  remoteParser.reset();
  txFrame(request);
  uint32_t start = millis();
  while (uint32_t(millis() - start) < 60) {
    while (remoteSerial.available())
      if (remoteParser.push(remoteSerial.read(), reply) &&
          reply.node == request.node && reply.session == request.session &&
          reply.sequence == request.sequence && reply.type == wire::Reply)
        return true;
    delay(1);
  }
  return false;
}
struct Outputs : Backend {
  bool known[4] = {true, true, true, true};
  bool intent(size_t i, bool on) {
    if (!storageReady || !lockStore())
      return false;
    String key = "run" + String(i);
    bool ok = store.putBool(key.c_str(), on) == 1;
    unlockStore();
    return ok;
  }
  bool on(size_t i, uint32_t duration) override {
    int phys = config.actuatorSlot[i];
    if (phys < 0 || bootHold || !IOT_PHYSICAL_OUTPUTS || !intent(i, true))
      return false;
#if IRRIGATION_ROLE_MATRIX
    wire::Frame q;
    q.node = phys + 1;
    q.type = wire::Start;
    q.session = session;
    q.sequence = ++remoteSequence;
    q.durationMs = duration;
    q.token = nodeTokens[phys];
    wire::Frame a;
    if (!nodeTokens[phys] || !exchange(q, a) || a.token != nodeTokens[phys] ||
        !(a.value & 8) || !(a.value & 16) || !(a.value & 1)) {
      known[i] = false;
      return false;
    }
    known[i] = true;
    return true;
#else
    if (!(IOT_RELAY_MASK & (1u << phys))) {
      intent(i, false);
      return false;
    }
    digitalWrite(hardware::relayPins[phys], IOT_RELAY_ACTIVE_LOW ? LOW : HIGH);
    known[i] = true;
    return true;
#endif
  }
  void off(size_t i) override {
    int phys = config.actuatorSlot[i];
    if (phys < 0) {
      known[i] = false;
      return;
    }
#if IRRIGATION_ROLE_MATRIX
    if (IOT_PHYSICAL_OUTPUTS && IOT_RS485_ENABLED) {
      wire::Frame q;
      q.node = phys + 1;
      q.type = wire::Stop;
      q.session = session;
      q.sequence = ++remoteSequence;
      wire::Frame a;
      known[i] = exchange(q, a) && (a.value & 16) && !(a.value & 1);
    }
#else
    if (IOT_PHYSICAL_OUTPUTS && (IOT_RELAY_MASK & (1u << phys)))
      digitalWrite(hardware::relayPins[phys],
                   IOT_RELAY_ACTIVE_LOW ? HIGH : LOW);
#endif
    if (known[i])
      intent(i, false);
  }
  bool allKnown() const {
    for (size_t i = 0; i < config.count; ++i)
      if (!known[i])
        return false;
    return true;
  }
} outputs;
bool persistAudit(size_t slot, const char *kind, const char *reason,
                  Source source) {
  if (!storageReady || !lockStore()) {
    bootHold = true;
    return false;
  }
  uint64_t seq = store.getULong64("aseq", 0) + 1;
  DynamicJsonDocument d(768);
  d["sequence"] = seq;
  d["zone_id"] = config.zoneIds[slot];
  d["kind"] = kind;
  d["reason"] = reason;
  d["source"] = sourceName(source);
  d["uptime_ms"] = millis();
  d["revision"] = revision;
  if (clockReady())
    d["recorded_at"] = iso(time(nullptr));
  else
    d["recorded_at"] = nullptr;
  String raw;
  serializeJson(d, raw);
  String key = "a" + String(uint32_t(seq % 64));
  bool ok = store.putString(key.c_str(), raw) == raw.length() &&
            store.putULong64("aseq", seq) == 8;
  unlockStore();
  if (!ok)
    bootHold = true;
  return ok;
}
struct EventJournal : Journal {
  bool reserve(size_t slot, Source source) override {
    return persistAudit(slot, "requested", "output_attempt", source);
  }
  bool consume(size_t slot, uint64_t occurrence) override {
    if (!lockStore())
      return false;
    String k = "occ" + String(slot);
    uint64_t keys[128] = {};
    size_t length = store.getBytesLength(k.c_str());
    bool ok = length % 8 == 0 && length <= sizeof(keys);
    if (ok && length)
      store.getBytes(k.c_str(), keys, length);
    size_t n = length / 8;
    for (size_t i = 0; i < n; ++i)
      if (keys[i] == occurrence) {
        unlockStore();
        return true;
      }
    if (ok && n < 128) {
      keys[n++] = occurrence;
      ok = store.putBytes(k.c_str(), keys, n * 8) == n * 8;
    } else
      ok = false;
    unlockStore();
    return ok;
  }
  void event(size_t slot, const char *kind, const char *reason,
             Source source) override {
    String eventCommand = commands[slot];
    const char *actual = kind;
    const char *why = reason;
    if (!outputs.known[slot]) {
      actual = "failed";
      why = "remote_output_unknown";
    }
    persistAudit(slot, actual, why, source);
    Serial.printf("event slot=%u kind=%s reason=%s\n", unsigned(slot), actual,
                  why);
    if (commands[slot].length()) {
      commandState(commands[slot], actual);
      if (remoteCommands[slot])
        commandAck(commands[slot], actual, why);
      if (String(actual) != "started")
        commands[slot] = "";
    }
    if (!clockReady() || !revision)
      return;
    DynamicJsonDocument doc(2048);
    doc["schema_version"] = 1;
    doc["message_id"] = uuid();
    doc["device_guid"] = guid;
    doc["boot_id"] = bootId;
    doc["sequence"] = ++sequence;
    doc["recorded_at"] = iso(time(nullptr));
    doc["time_quality"] = "synchronized";
    doc["configuration_revision"] = revision;
    doc["simulated"] = false;
    doc.createNestedArray("readings");
    JsonArray events = doc.createNestedArray("events");
    JsonObject e = events.createNestedObject();
    e["event_id"] = uuid();
    e["device_guid"] = guid;
    e["zone_id"] = config.zoneIds[slot];
    e["actuator_id"] = config.actuatorIds[slot];
    e["kind"] = actual;
    e["reason"] = why;
    e["source"] = sourceName(source);
    e["recorded_at"] = iso(time(nullptr));
    e["revision"] = revision;
    if (eventCommand.length())
      e["command_id"] = eventCommand;
    String raw;
    serializeJson(doc, raw);
    queueJob("/iot/v1/telemetry", raw);
  }
} journal;
Controller controller(outputs, journal);
// A checkpoint describes observation, not proof of valve execution. The last
// minute before a power loss may be reported again as unobserved after boot.
bool recoverSchedules(time_t epoch, uint32_t now) {
  if (!clockReady()) return false;
  int64_t from = previousEpoch;
  if (recoveryPending) {
    if (!lockStore()) { bootHold = true; return false; }
    String raw = store.getString("observation", "");
    unlockStore();
    DynamicJsonDocument checkpoint(256);
    from = epoch;
    if (raw.length()) {
      if (deserializeJson(checkpoint, raw) ||
          !checkpoint["epoch"].is<int64_t>() ||
          !checkpoint["revision"].is<uint32_t>() ||
          checkpoint["epoch"].as<int64_t>() < 1767225600LL ||
          checkpoint["epoch"].as<int64_t>() > 2147483646LL) {
        bootHold = true;
        return false;
      }
      if (checkpoint["revision"].as<uint32_t>() == revision)
        from = checkpoint["epoch"].as<int64_t>();
    }
  }
  // Never advance a checkpoint backwards or allow old windows after rollback.
  if (int64_t(epoch) < from) return false;
  if (recoveryPending || int64_t(epoch) - from > 3) {
    unsigned counts[4] = {};
    bool truncated[4] = {};
    for (size_t i = 0; i < config.slotCount; ++i) {
      const auto &s = config.slots[i];
      if (!config.zones[s.zone].enabled ||
          config.zones[s.zone].mode == Mode::Dynamic) continue;
      auto result = unobserved(s, from, epoch);
      counts[s.zone] += result.windows;
      truncated[s.zone] = truncated[s.zone] || result.truncated;
    }
    for (size_t i = 0; i < config.count; ++i) {
      const Source source = config.zones[i].mode == Mode::Hybrid
                                ? Source::Hybrid : Source::Static;
      if (counts[i]) {
        String reason = "unobserved_closed_windows_" + String(counts[i]);
        if (!persistAudit(i, "recovery_summary", reason.c_str(), source)) return false;
      }
      if (truncated[i] &&
          !persistAudit(i, "recovery_summary", "weekly_history_limited_7d", source))
        return false;
    }
  }
  if (recoveryPending || int64_t(epoch) - from > 3) {
    for (size_t i = 0; i < config.slotCount; ++i) {
      const auto &s = config.slots[i];
      if (!config.zones[s.zone].enabled ||
          config.zones[s.zone].mode == Mode::Dynamic) continue;
      const auto window = evaluate(s, epoch, epoch, revision);
      if (!window.open) continue;
      // Also consume an interrupted open window: hybrid must not start late
      // on the next tick simply because moisture is below its threshold.
      if (!journal.consume(s.zone, window.occurrence)) {
        bootHold = true;
        return false;
      }
      controller.restoreOccurrence(s.zone, window.occurrence);
      if (!persistAudit(s.zone, "recovery_summary", "open_window_not_resumed",
                        config.zones[s.zone].mode == Mode::Hybrid
                            ? Source::Hybrid : Source::Static)) return false;
    }
  }
  if (recoveryPending || uint32_t(now - checkpointAt) >= 60000) {
    DynamicJsonDocument checkpoint(256);
    checkpoint["revision"] = revision;
    checkpoint["epoch"] = int64_t(epoch);
    String raw;
    serializeJson(checkpoint, raw);
    if (!lockStore()) { bootHold = true; return false; }
    bool ok = store.putString("observation", raw) == raw.length();
    unlockStore();
    if (!ok) { bootHold = true; return false; }
    checkpointAt = now;
  }
  if (recoveryPending) previousEpoch = epoch;
  recoveryPending = false;
  return true;
}

bool applyEnvelope(const String &body, String &error) {
  if (controller.activeCount() ||
      (!outputs.allKnown() && IOT_PHYSICAL_OUTPUTS)) {
    error = "controller_not_idle";
    return false;
  }
  DynamicJsonDocument env(24576);
  if (body.length() > bodyLimit ||
      deserializeJson(env, body, DeserializationOption::NestingLimit(12))) {
    error = "json_invalid_or_large";
    return false;
  }
  if (env["schema_version"] != 1 || env["device_guid"].as<String>() != guid ||
      !env["revision"].is<unsigned>() || env["revision"].as<unsigned>() < 1 ||
      env["revision"].as<unsigned>() < revision) {
    error = "identity_or_revision_invalid";
    return false;
  }
  size_t begin = 0, end = 0;
  std::string bytes(body.c_str(), body.length());
  if (!objectMember(bytes, "configuration", begin, end)) {
    error = "configuration_bytes_missing";
    return false;
  }
  String raw = body.substring(begin, end);
  // Verify server bytes, not a reserialization that could change numeric
  // formatting.
  String hash = env["configuration_hash"].as<String>();
  if (hash.length() != 64 || digest(raw) != hash) {
    error = "configuration_hash_mismatch";
    return false;
  }
  std::unique_ptr<FirmwareConfig> candidate(new FirmwareConfig());
  if (!parseConfiguration(env["configuration"].as<JsonObjectConst>(),
                          *candidate, error))
    return false;
  for (unsigned i = 0; i < candidate->count; ++i) {
    for (unsigned j = 0; j < 4; ++j)
      if (candidate->actuatorIds[i] == actuatorBinding[j] &&
          strlen(actuatorBinding[j]))
        candidate->actuatorSlot[i] = j;
    if (IOT_PHYSICAL_OUTPUTS && candidate->zones[i].enabled &&
        candidate->actuatorSlot[i] < 0) {
      error = "actuator_binding_missing";
      return false;
    }
  }
  for (unsigned i = 0; i < candidate->moistureCount; ++i)
    for (unsigned j = 0; j < 4; ++j)
      if (candidate->moistureIds[i] == moistureBinding[j] &&
          strlen(moistureBinding[j]))
        candidate->moistureSlot[i] = j;
  for (unsigned i = 0; i < candidate->temperatureCount; ++i)
    for (unsigned j = 0; j < 4; ++j)
      if (candidate->temperatureIds[i] == temperatureBinding[j] &&
          strlen(temperatureBinding[j]))
        candidate->temperatureSlot[i] = j;

  const uint32_t next = env["revision"];
  if (next == revision && configRaw.length()) {
    if (hash != configHash) {
      error = "same_revision_changed";
      return false;
    }
    return true;
  }
  if (!lockStore()) {
    error = "storage_busy";
    return false;
  }
  bool saved = store.putString("config", body) == body.length();
  unlockStore();
  if (!saved) {
    error = "persistence_failed";
    return false;
  }
  if (!controller.configure(candidate->zones, candidate->count,
                            candidate->capacity)) {
    error = "controller_rejected";
    return false;
  }
  config = *candidate;
  for (unsigned i = 0; i < 4; ++i)
    outputs.known[i] = !IOT_PHYSICAL_OUTPUTS ? false : outputs.known[i];
  configRaw = raw;
  configHash = hash;
  revision = next;
  setenv("TZ", config.timezone == "America/Sao_Paulo" ? "BRT3" : "UTC0", 1);
  tzset();
  previousEpoch = time(nullptr);
  recoveryPending = true;
  if (lockStore()) {
    for (unsigned i = 0; i < 4; ++i) {
      String k = "occ" + String(i);
      uint64_t keys[128] = {};
      size_t len = store.getBytesLength(k.c_str());
      if (len <= sizeof(keys) && len % 8 == 0) {
        if (len)
          store.getBytes(k.c_str(), keys, len);
        for (size_t j = 0; j < len / 8; ++j)
          controller.restoreOccurrence(i, keys[j]);
      }
    }
    unlockStore();
  }
  return true;
}
void handleCommand(JsonObjectConst c, bool local) {
  String id = c["command_id"].as<String>(), zone = c["zone_id"].as<String>();
  if (!validId(id))
    return;
  int slot = -1;
  for (unsigned i = 0; i < config.count; ++i)
    if (config.zoneIds[i] == zone)
      slot = i;
  if (slot < 0) {
    if (!local)
      commandAck(id, "rejected", "zone_missing");
    return;
  }
  String old = previousCommand(id);
  if (old.length()) {
    if (old == "received" || old == "started") {
      bool current = commands[slot] == id && controller.run(slot).active;
      old = current ? "started" : "interrupted";
      if (!current)
        commandState(id, old);
    }
    if (!local && old != "storage_fault")
      commandAck(id, old.c_str(), "reconciliation");
    return;
  }
  if (c["reconcile_only"] | false) {
    if (!local)
      commandAck(id, "failed", "reconciliation_history_unavailable");
    return;
  }
  if (!local && (c["device_guid"].as<String>() != guid ||
                 c["configuration_revision"].as<unsigned>() != revision ||
                 c["actuator_id"].as<String>() != config.actuatorIds[slot])) {
    commandAck(id, "rejected", "configuration_mismatch");
    return;
  }
  String action = c["action"].as<String>(), source = c["source"].as<String>();
  if (action != "stop" &&
      (!clockReady() ||
       (!local && (!c["expires_at"].is<int64_t>() ||
                   c["expires_at"].as<int64_t>() <= time(nullptr))))) {
    if (!local)
      commandAck(id, "expired", "expired_or_clock_invalid");
    return;
  }
  if (!commandState(id, "received",
                    local ? int64_t(time(nullptr)) + 30
                          : c["expires_at"].as<int64_t>())) {
    if (!local)
      commandAck(id, "rejected", "command_journal_full_or_fault");
    return;
  }
  if (action == "stop") {
    controller.stop(slot, millis());
    commandState(id, outputs.known[slot] ? "completed" : "failed");
    if (!local)
      commandAck(id, outputs.known[slot] ? "completed" : "failed",
                 "authorized_stop");
    return;
  }
  if (action != "start" || (source != "test" && source != "manual") ||
      !c["duration_s"].is<unsigned>() ||
      c["duration_s"].as<unsigned>() > 86400) {
    commandState(id, "rejected");
    if (!local)
      commandAck(id, "rejected", "command_invalid");
    return;
  }
  commands[slot] = id;
  remoteCommands[slot] = !local;
  bool ok = controller.manual(
      slot, millis(), c["duration_s"].as<unsigned>() * 1000, source == "test");
  (void)ok;
}
String statusJson(bool csrfToken) {
  DynamicJsonDocument d(4096);
  d["firmware"] = "0.2.2-experimental";
  d["guid"] = guid;
  d["revision"] = revision;
  d["clock_valid"] = clockReady();
  d["hardware_enabled"] = bool(IOT_PHYSICAL_OUTPUTS);
  d["boot_hold"] = bootHold;
  d["lost_events"] = lostEvents;
  d["portal_auth_blocked"] = authBlocked.load();
  d["remote_output_known"] = outputs.allKnown();
  if (csrfToken)
    d["csrf"] = csrf;
  JsonArray zones = d.createNestedArray("zones");
  for (unsigned i = 0; i < config.count; ++i) {
    JsonObject z = zones.createNestedObject();
    z["id"] = config.zoneIds[i];
    z["active"] = controller.run(i).active;
    z["output_known"] = outputs.known[i];
  }
  String s;
  serializeJson(d, s);
  return s;
}
int authenticate() {
  // Strict scheme gate avoids accepting arbitrary Authorization text in older
  // WebServer.
  if (!web.header("Authorization").startsWith("Basic ")) {
    web.requestAuthentication(BASIC_AUTH, "Irrigation");
    return 0;
  }
  auto matches = [&](const char *user, const char *password) {
    String credential = String(user) + ":" + password;
    unsigned char b64[256];
    size_t size = 0;
    if (credential.length() > 180 ||
        mbedtls_base64_encode(
            b64, sizeof(b64) - 1, &size,
            reinterpret_cast<const unsigned char *>(credential.c_str()),
            credential.length()))
      return false;
    b64[size] = 0;
    String expected = String("Basic ") + reinterpret_cast<const char *>(b64),
           provided = web.header("Authorization");
    return credentialMatches(provided.c_str(), expected.c_str());
  };
  if (strlen(IOT_ADMIN_PASSWORD) >= 12 && matches("admin", IOT_ADMIN_PASSWORD))
    return 2;
  if (strlen(IOT_OPERATOR_PASSWORD) >= 12 &&
      strcmp(IOT_OPERATOR_PASSWORD, IOT_ADMIN_PASSWORD) &&
      matches("operador", IOT_OPERATOR_PASSWORD))
    return 1;
  web.requestAuthentication(BASIC_AUTH, "Irrigation");
  return 0;
}
bool administrator() {
  int role = authenticate();
  if (!role)
    return false;
  if (role != 2) {
    web.send(403, "application/json", "{\"error\":\"administrator_required\"}");
    return false;
  }
  return true;
}
void localPortal() {
  const char *headers[] = {"Authorization", "X-CSRF-Token"};
  web.collectHeaders(headers, 2);
  web.on("/", HTTP_GET, [] {
    if (!authenticate())
      return;
    web.sendHeader("Cache-Control", "no-store");
    web.send_P(200, "text/html; charset=utf-8", localPortalPage);
  });
  web.on("/api/state", HTTP_GET, [] {
    if (!authenticate())
      return;
    web.sendHeader("Cache-Control", "no-store");
    web.send(200, "application/json", statusJson(true));
  });
  web.on("/api/configuration", HTTP_GET, [] {
    if (!administrator())
      return;
    web.send(200, "application/json", configRaw.length() ? configRaw : "{}");
  });
  web.on("/api/outbox", HTTP_GET, [] {
    if (!administrator())
      return;
    if (!lockStore()) {
      web.send(503, "application/json", "{}");
      return;
    }
    unsigned head = 0, count = 0;
    String meta = store.getString("qmeta", "0,0");
    sscanf(meta.c_str(), "%u,%u", &head, &count);
    DynamicJsonDocument d(1024), row(6144);
    d["count"] = count;
    d["capacity"] = 16;
    if (count && head < 16) {
      String key = "q" + String(head);
      deserializeJson(row, store.getString(key.c_str(), ""));
      d["head_attempts"] = row["attempts"] | 0;
      d["head_blocked"] = (row["attempts"] | 0) >= 5;
    }
    unlockStore();
    String raw;
    serializeJson(d, raw);
    web.send(200, "application/json", raw);
  });
  web.on("/api/outbox/retry", HTTP_POST, [] {
    if (!administrator())
      return;
    if (web.header("X-CSRF-Token") != csrf || controller.activeCount()) {
      web.send(409, "application/json", "{}");
      return;
    }
    if (!lockStore()) {
      web.send(503, "application/json", "{}");
      return;
    }
    unsigned head = 0, count = 0;
    String meta = store.getString("qmeta", "0,0");
    sscanf(meta.c_str(), "%u,%u", &head, &count);
    bool ok = false;
    if (count && head < 16) {
      String key = "q" + String(head);
      DynamicJsonDocument row(6144);
      if (!deserializeJson(row, store.getString(key.c_str(), ""))) {
        row["attempts"] = 0;
        String raw;
        serializeJson(row, raw);
        ok = store.putString(key.c_str(), raw) == raw.length();
      }
    }
    unlockStore();
    web.send(ok ? 200 : 409, "application/json",
             ok ? "{\"retry_pending\":true}" : "{}");
  });
  web.on("/api/events", HTTP_GET, [] {
    if (!authenticate())
      return;
    if (!lockStore()) {
      web.send(503, "application/json", "{}");
      return;
    }
    DynamicJsonDocument d(32768);
    JsonArray events = d.createNestedArray("events");
    uint64_t last = store.getULong64("aseq", 0),
             first = last > 63 ? last - 63 : 1;
    d["total_since_provisioning"] = last;
    d["retained_limit"] = 64;
    for (uint64_t seq = first; seq <= last; ++seq) {
      String key = "a" + String(uint32_t(seq % 64));
      DynamicJsonDocument row(768);
      if (!deserializeJson(row, store.getString(key.c_str(), "")) &&
          row["sequence"].as<uint64_t>() == seq)
        events.add(row.as<JsonObjectConst>());
    }
    unlockStore();
    String raw;
    serializeJson(d, raw);
    web.send(200, "application/json", raw);
  });
  web.on("/api/sensors", HTTP_GET, [] {
    if (!administrator())
      return;
    DynamicJsonDocument d(2048);
    JsonArray raw = d.createNestedArray("raw"),
              roms = d.createNestedArray("roms");
    (void)raw;
    (void)roms;
#if !IRRIGATION_ROLE_MATRIX
    for (unsigned i = 0; i < 4; ++i)
      raw.add(analogRead(hardware::moisturePins[i]));
    for (unsigned i = 0; i < temperatures.getDeviceCount() && i < 4; ++i) {
      DeviceAddress addr;
      if (temperatures.getAddress(addr, i)) {
        char hex[17];
        for (unsigned j = 0; j < 8; ++j)
          snprintf(hex + j * 2, 3, "%02x", addr[j]);
        roms.add(hex);
      }
    }
#endif
    String s;
    serializeJson(d, s);
    web.send(200, "application/json", s);
  });
  web.on("/api/calibration", HTTP_POST, [] {
    if (!administrator())
      return;
    if (web.header("X-CSRF-Token") != csrf || controller.activeCount() ||
        nodeLease.active()) {
      web.send(409, "application/json", "{}");
      return;
    }
    DynamicJsonDocument d(1024);
    String raw = web.arg("plain");
    if (raw.length() > 512 || deserializeJson(d, raw) ||
        !d["slot"].is<unsigned>() || d["slot"].as<unsigned>() > 3) {
      web.send(400, "application/json", "{}");
      return;
    }
    unsigned i = d["slot"];
    String rom = d["rom"].as<String>();
    bool hasRom = rom.length();
    if (hasRom) {
      if (rom.length() != 16) {
        web.send(400, "application/json", "{}");
        return;
      }
      for (unsigned j = 0; j < 16; ++j)
        if (!isxdigit(rom[j])) {
          web.send(400, "application/json", "{}");
          return;
        }
    } else if (!d["dry"].is<int>() || !d["wet"].is<int>() ||
               d["dry"].as<int>() < 0 || d["dry"].as<int>() > 4095 ||
               d["wet"].as<int>() < 0 || d["wet"].as<int>() > 4095 ||
               abs(d["dry"].as<int>() - d["wet"].as<int>()) < 200) {
      web.send(400, "application/json", "{}");
      return;
    }
    if (!lockStore()) {
      web.send(503, "application/json", "{}");
      return;
    }
    bool ok;
    if (hasRom) {
      String key = "rom" + String(i);
      ok = store.putString(key.c_str(), rom) == rom.length();
      if (ok)
        temperatureRom[i] = rom;
    } else {
      String key = "cal" + String(i);
      String pair =
          String(d["dry"].as<int>()) + "," + String(d["wet"].as<int>());
      ok = store.putString(key.c_str(), pair) == pair.length();
    }
    unlockStore();
    web.send(ok ? 200 : 500, "application/json",
             ok ? "{\"saved\":true}" : "{}");
  });
  web.on("/api/configuration", HTTP_POST, [] {
    if (!administrator())
      return;
    if (web.header("X-CSRF-Token") != csrf) {
      web.send(403, "application/json", "{}");
      return;
    }
    if (IRRIGATION_REMOTE_NODE || strlen(IOT_PORTAL_URL)) {
      web.send(409, "application/json",
               "{\"error\":\"portal_is_configuration_authority\"}");
      return;
    }
    String error;
    bool ok = applyEnvelope(web.arg("plain"), error);
    DynamicJsonDocument d(512);
    d["applied"] = ok;
    if (!ok)
      d["error"] = error;
    String raw;
    serializeJson(d, raw);
    web.send(ok ? 200 : 400, "application/json", raw);
  });
  web.on("/api/command", HTTP_POST, [] {
    if (!authenticate())
      return;
    if (IRRIGATION_REMOTE_NODE) {
      web.send(405, "application/json",
               "{\"error\":\"command_from_matrix_required\"}");
      return;
    }
    if (web.header("X-CSRF-Token") != csrf) {
      web.send(403, "application/json", "{\"error\":\"csrf_invalid\"}");
      return;
    }
    String raw = web.arg("plain");
    DynamicJsonDocument d(1024);
    if (raw.length() > 512 || deserializeJson(d, raw) || !d.is<JsonObject>()) {
      web.send(400, "application/json", "{\"error\":\"command_invalid\"}");
      return;
    }
    d["command_id"] = uuid();
    controller.clockValid(clockReady() && !bootHold && outputs.allKnown());
    handleCommand(d.as<JsonObjectConst>(), true);
    DynamicJsonDocument result(4096);
    deserializeJson(result, statusJson(false));
    result["command_id"] = d["command_id"];
    result["command_status"] = previousCommand(d["command_id"].as<String>());
    String response;
    serializeJson(result, response);
    web.send(200, "application/json", response);
  });
  web.onNotFound(
      [] { web.send(404, "application/json", "{\"error\":\"not_found\"}"); });
  web.begin();
}
// Network runs in its own task: TLS/HTTP waits cannot prolong local run
// duration.
int api(const String &path, const String &body, String &response) {
  if (authBlocked.load())
    return -3;
  if (WiFi.status() != WL_CONNECTED ||
      !String(IOT_PORTAL_URL).startsWith("https://") ||
      !strlen(IOT_PORTAL_CA) || !strlen(IOT_DEVICE_TOKEN) || !clockReady())
    return -1;
  WiFiClientSecure tls;
  tls.setCACert(IOT_PORTAL_CA);
  tls.setTimeout(3);
  HTTPClient http;
  http.setConnectTimeout(2000);
  http.setTimeout(3000);
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  if (!http.begin(tls, String(IOT_PORTAL_URL) + path))
    return -1;
  http.addHeader("Authorization", String("Bearer ") + IOT_DEVICE_TOKEN);
  http.addHeader("Content-Type", "application/json");
  int code = body.length() ? http.POST(body) : http.GET();
  if (code == 401 || code == 403)
    authBlocked.store(true);
  int length = http.getSize();
  // Require bounded Content-Length; reject chunked/unknown rather than allocate
  // unbounded String.
  if (code > 0 && length >= 0 && size_t(length) <= bodyLimit)
    response = http.getString();
  else
    code = -2;
  http.end();
  return code;
}
void networkTask(void *) {
  uint32_t configAt = millis() - 10000, commandsAt = millis() - 1000;
  uint32_t retryAt = 0;
  for (;;) {
    String response;
    if (lockStore()) {
      unsigned count = 0, head = 0;
      String meta = store.getString("qmeta", "0,0");
      sscanf(meta.c_str(), "%u,%u", &head, &count);
      if (head >= 16 || count > 16) {
        count = 0;
        head = 0;
      }
      String key = "q" + String(head),
             raw = count ? store.getString(key.c_str(), "") : String();
      unlockStore();
      if (raw.length() && int32_t(millis() - retryAt) >= 0 &&
          WiFi.status() == WL_CONNECTED && clockReady() &&
          !authBlocked.load()) {
        DynamicJsonDocument j(bodyLimit + 1024);
        if (!deserializeJson(j, raw) && (j["attempts"] | 0) < 5) {
          int attempts = (j["attempts"] | 0) + 1;
          j["attempts"] = attempts;
          String updated;
          serializeJson(j, updated);
          bool saved = false;
          if (lockStore()) {
            if (store.getString(key.c_str(), "") == raw)
              saved = store.putString(key.c_str(), updated) == updated.length();
            unlockStore();
          }
          if (!saved) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
          }
          raw = updated;
          retryAt = millis() + uint32_t(2000u << (attempts - 1));
          int code =
              api(j["path"].as<String>(), j["body"].as<String>(), response);
          DynamicJsonDocument ack(1024);
          if (code >= 200 && code < 300 && !deserializeJson(ack, response) &&
              ack["accepted"] == true) {
            bool matching = true;
            if (j["path"] == "/iot/v1/telemetry") {
              DynamicJsonDocument sent(bodyLimit + 1024);
              matching = !deserializeJson(sent, j["body"].as<String>()) &&
                         ack["message_id"].as<String>() ==
                             sent["message_id"].as<String>();
            }
            if (matching && lockStore()) {
              if (store.getString(key.c_str(), "") == raw) {
                unsigned currentHead = 0, currentCount = 0;
                String meta = store.getString("qmeta", "0,0");
                if (sscanf(meta.c_str(), "%u,%u", &currentHead,
                           &currentCount) == 2 &&
                    currentHead == head && currentCount > 0) {
                  if (store.putString("qmeta", String((head + 1) % 16) + "," +
                                                   String(currentCount - 1)) >
                      0)
                    store.remove(key.c_str());
                }
              }
              unlockStore();
            }
          }
        }
      }
    }
    response = "";
    if (uint32_t(millis() - configAt) >= 10000) {
      configAt = millis();
      if (api("/iot/v1/configuration", "", response) == 200) {
        Input *input = new Input{"configuration", response};
        if (xQueueSend(incoming, &input, 0) != pdTRUE)
          delete input;
      }
    }
    response = "";
    if (uint32_t(millis() - commandsAt) >= 1000) {
      commandsAt = millis();
      if (api("/iot/v1/commands", "", response) == 200) {
        DynamicJsonDocument d(bodyLimit + 1024);
        if (!deserializeJson(d, response) &&
            d["device_guid"].as<String>() == guid && d["schema_version"] == 1) {
          unsigned n = 0;
          for (JsonObjectConst command : d["commands"].as<JsonArrayConst>()) {
            if (n++ == 4)
              break;
            String id = command["command_id"].as<String>();
            if (!validId(id))
              continue;
            String ack;
            bool reconcile = command["reconcile_only"] | false;
            if (!reconcile) {
              if (api("/iot/v1/commands/" + id + "/events",
                      "{\"status\":\"received\"}", ack) != 200)
                continue;
              DynamicJsonDocument a(1024);
              if (deserializeJson(a, ack) || a["accepted"] != true)
                continue;
            }
            String raw;
            serializeJson(command, raw);
            Input *input = new Input{"command", raw};
            if (xQueueSend(incoming, &input, 0) != pdTRUE)
              delete input;
          }
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}
void reportRuntime() {
  if (!revision || !clockReady() || !outputs.allKnown())
    return;
  DynamicJsonDocument d(8192);
  JsonArray active = d.createNestedArray("active_groups"),
            runs = d.createNestedArray("runs");
  uint32_t now = millis();
  time_t epoch = time(nullptr);
  for (unsigned i = 0; i < config.count; ++i)
    if (controller.run(i).active) {
      auto r = controller.run(i);
      active.add(config.zoneIds[i]);
      JsonObject row = runs.createNestedObject();
      row["group_id"] = config.zoneIds[i];
      row["source"] = sourceName(r.source);
      double start = double(epoch) - uint32_t(now - r.began) / 1000.0;
      row["started_at"] = start;
      row["deadline"] = start + r.duration / 1000.0;
      if (commands[i].length())
        row["command_id"] = commands[i];
    }
  JsonObject status = d.createNestedObject("controller_status");
  status["revision"] = revision;
  status["clock_valid"] = clockReady() && !bootHold;
  status["now"] = int64_t(epoch);
  status["uptime"] = now / 1000.0;
  JsonObject sensors = status.createNestedObject("sensors");
  for (unsigned i = 0; i < config.moistureCount; ++i) {
    JsonObject s = sensors.createNestedObject(config.moistureIds[i]);
    auto r = moisture[i];
    bool valid =
        config.moistureEnabled[i] && r.valid && uint32_t(now - r.at) <= 30000;
    if (valid)
      s["value"] = r.value;
    else
      s["value"] = nullptr;
    s["quality"] = valid ? "valid" : "missing";
    if (r.at <= now)
      s["updated_uptime"] = r.at / 1000.0;
  }
  for (unsigned i = 0; i < config.temperatureCount; ++i) {
    JsonObject s = sensors.createNestedObject(config.temperatureIds[i]);
    auto r = tempReadings[i];
    bool valid = config.temperatureEnabled[i] && r.valid &&
                 uint32_t(now - r.at) <= 30000;
    if (valid)
      s["value"] = r.value;
    else
      s["value"] = nullptr;
    s["quality"] = valid ? "valid" : "missing";
    if (r.at <= now)
      s["updated_uptime"] = r.at / 1000.0;
  }
  status.createNestedObject("last_stop_uptime");
  status.createNestedArray("consumed_occurrences");
  String raw;
  serializeJson(d, raw);
  if (!d.overflowed())
    queueJob("/iot/v1/runtime", raw);
}
void reportSample() {
  if (!revision || !clockReady())
    return;
  DynamicJsonDocument d(8192);
  d["schema_version"] = 1;
  d["message_id"] = uuid();
  d["device_guid"] = guid;
  d["boot_id"] = bootId;
  d["sequence"] = ++sequence;
  d["recorded_at"] = iso(time(nullptr));
  d["time_quality"] = "synchronized";
  d["configuration_revision"] = revision;
  d["simulated"] = false;
  JsonArray readings = d.createNestedArray("readings");
  auto reading = [&](const String &id, const Reading &r, bool enabled,
                     const char *metric, const char *unit) {
    JsonObject s = readings.createNestedObject();
    s["component_id"] = id;
    s["metric"] = metric;
    s["unit"] = unit;
    bool valid = enabled && r.valid && uint32_t(millis() - r.at) <= 30000;
    s["quality"] = valid ? "valid" : "missing";
    if (valid)
      s["value"] = r.value;
    else
      s["value"] = nullptr;
    s["recorded_at"] = iso(time(nullptr));
  };
  for (unsigned i = 0; i < config.moistureCount; ++i)
    reading(config.moistureIds[i], moisture[i], config.moistureEnabled[i],
            "soil_moisture", "%relative");
  for (unsigned i = 0; i < config.temperatureCount; ++i)
    reading(config.temperatureIds[i], tempReadings[i],
            config.temperatureEnabled[i], "soil_temperature", "degC");
  JsonArray out = d.createNestedArray("outputs");
  for (unsigned i = 0; i < config.count; ++i)
    if (outputs.known[i]) {
      JsonObject o = out.createNestedObject();
      o["component_id"] = config.actuatorIds[i];
      o["commanded_state"] = controller.run(i).active ? "on" : "off";
      o["feedback_kind"] = "electrical_output";
    }
  d.createNestedArray("events");
  String raw;
  serializeJson(d, raw);
  if (!d.overflowed())
    queueJob("/iot/v1/telemetry", raw);
}
#if !IRRIGATION_ROLE_MATRIX
bool pendingTemperature = false;
uint32_t conversionAt = 0;
void readSensors(uint32_t now) {
  if (pendingTemperature && uint32_t(now - conversionAt) >= 400) {
    for (unsigned i = 0; i < config.temperatureCount; ++i) {
      DeviceAddress rom;
      bool valid = false;
      float value = 0;
      int phys = config.temperatureSlot[i];
      if (phys >= 0 && temperatureRom[phys].length() == 16) {
        for (unsigned j = 0; j < 8; ++j)
          rom[j] =
              strtoul(temperatureRom[phys].substring(j * 2, j * 2 + 2).c_str(),
                      nullptr, 16);
        value = temperatures.getTempC(rom);
        valid = value != DEVICE_DISCONNECTED_C && isfinite(value) &&
                value >= -55 && value <= 125;
      }
      tempReadings[i] = {value, now, valid};
    }
    pendingTemperature = false;
  }
  if (uint32_t(now - lastRead) < 10000)
    return;
  lastRead = now;
  for (unsigned i = 0; i < config.moistureCount; ++i) {
    int phys = config.moistureSlot[i];
    if (phys < 0) {
      moisture[i] = {0, now, false};
      continue;
    }
    String key = "cal" + String(phys);
    int dry = -1, wet = -1;
    if (lockStore()) {
      String pair = store.getString(key.c_str(), "");
      sscanf(pair.c_str(), "%d,%d", &dry, &wet);
      unlockStore();
    }
    int raw = analogRead(hardware::moisturePins[phys]);
    bool valid = config.moistureEnabled[i] && dry >= 0 && wet >= 0 &&
                 abs(wet - dry) >= 200 && raw > 10 && raw < 4085;
    float value = valid ? 100.0f * (raw - dry) / (wet - dry) : 0;
    value = fmaxf(0, fminf(100, value));
    moisture[i] = {value, now, valid};
  }
  if (config.temperatureCount && !pendingTemperature) {
    temperatures.requestTemperatures();
    conversionAt = now;
    pendingTemperature = true;
  }
}
#endif
void remotePoll(uint32_t now) {
  if (!IOT_RS485_ENABLED || uint32_t(now - lastPoll) < 250)
    return;
  lastPoll = now;
  static unsigned phys = 0;
  int zone = -1;
  for (unsigned i = 0; i < config.count; ++i)
    if (config.actuatorSlot[i] == int(phys))
      zone = i;
  bool bound = zone >= 0;
  for (unsigned i = 0; i < config.moistureCount; ++i)
    bound |= config.moistureSlot[i] == int(phys);
  for (unsigned i = 0; i < config.temperatureCount; ++i)
    bound |= config.temperatureSlot[i] == int(phys);
  if (bound) {
    wire::Frame q;
    q.node = phys + 1;
    q.type = wire::Poll;
    q.session = session;
    q.sequence = ++remoteSequence;
    wire::Frame a;
    if (exchange(q, a)) {
      nodeTokens[phys] = a.token;
      if (zone >= 0) {
        if (!(a.value & 16)) {
          outputs.known[zone] = false;
          if (controller.run(zone).active)
            controller.stop(zone, now, "remote_driver_unavailable");
        } else if (bool(a.value & 1) != controller.run(zone).active) {
          outputs.known[zone] = false;
          controller.stop(zone, now, "remote_output_mismatch");
        } else
          outputs.known[zone] = true;
      }
      for (unsigned i = 0; i < config.moistureCount; ++i)
        if (config.moistureSlot[i] == int(phys)) {
          float v = float(a.durationMs >> 16) / 100;
          moisture[i] = {v, now, bool(a.value & 2) && v >= 0 && v <= 100};
        }
      for (unsigned i = 0; i < config.temperatureCount; ++i)
        if (config.temperatureSlot[i] == int(phys)) {
          float v = int16_t(a.durationMs & 0xffff) / 100.0f;
          tempReadings[i] = {v, now, bool(a.value & 4) && v >= -55 && v <= 125};
        }
    } else if (zone >= 0) {
      outputs.known[zone] = false;
      if (controller.run(zone).active)
        controller.stop(zone, now, "remote_timeout");
    }
  }
  phys = (phys + 1) % 4;
}

void nodeLoop(uint32_t now) {
#if IRRIGATION_REMOTE_NODE
  readSensors(now);
  wire::Frame q;
  while (remoteSerial.available())
    if (remoteParser.push(remoteSerial.read(), q) &&
        q.node == IOT_NODE_ADDRESS) {
      bool accepted = false;
      if (q.type == wire::Start &&
          (!IOT_PHYSICAL_OUTPUTS || !(IOT_RELAY_MASK & 1) || !storageReady))
        accepted = false;
      else if (q.type == wire::Start && !outputs.intent(0, true))
        accepted = false;
      else
        accepted = nodeLease.accept(q, now, IOT_NODE_MAX_RUN_MS);
      bool active = nodeLease.tick(now, linkLeaseMs);
      if (IOT_PHYSICAL_OUTPUTS && (IOT_RELAY_MASK & 1))
        digitalWrite(hardware::relayPins[0],
                     active ? (IOT_RELAY_ACTIVE_LOW ? LOW : HIGH)
                            : (IOT_RELAY_ACTIVE_LOW ? HIGH : LOW));
      wire::Frame a = q;
      a.type = wire::Reply;
      a.token = nodeBootToken;
      a.value = (active ? 1 : 0) | (moisture[0].valid ? 2 : 0) |
                (tempReadings[0].valid ? 4 : 0) | (accepted ? 8 : 0) |
                ((IOT_PHYSICAL_OUTPUTS && (IOT_RELAY_MASK & 1)) ? 16 : 0);
      a.durationMs = (uint32_t(uint16_t(moisture[0].value * 100)) << 16) |
                     uint16_t(int16_t(tempReadings[0].value * 100));
      txFrame(a);
    }
  bool active = nodeLease.tick(now, linkLeaseMs);
  if (IOT_PHYSICAL_OUTPUTS && (IOT_RELAY_MASK & 1))
    digitalWrite(hardware::relayPins[0],
                 active ? (IOT_RELAY_ACTIVE_LOW ? LOW : HIGH)
                        : (IOT_RELAY_ACTIVE_LOW ? HIGH : LOW));
#else
  (void)now;
#endif
}
void setup() {
  Serial.begin(115200);
  storeMutex = xSemaphoreCreateMutex();
  incoming = xQueueCreate(4, sizeof(Input *));
#if !IRRIGATION_ROLE_MATRIX
  if (IOT_PHYSICAL_OUTPUTS)
    for (unsigned i = 0; i < 4; ++i)
      if (IOT_RELAY_MASK & (1u << i)) {
        digitalWrite(hardware::relayPins[i], IOT_RELAY_ACTIVE_LOW ? HIGH : LOW);
        pinMode(hardware::relayPins[i], OUTPUT);
      }
#endif
  storageReady = storeMutex && incoming && store.begin("irrigation", false);
  if (!storageReady) {
    Serial.println("storage_fault: outputs blocked");
    return;
  }
  guid = store.getString("guid", "");
  if (!guid.length()) {
    guid = strlen(IOT_DEVICE_GUID) ? String(IOT_DEVICE_GUID) : uuid();
    storageReady = store.putString("guid", guid) == guid.length();
  }
  if (strlen(IOT_DEVICE_GUID) && guid != IOT_DEVICE_GUID) {
    storageReady = false;
    Serial.println("GUID mismatch: preserve identity, fix provisioning");
  }
  bootId = uuid();
  csrf = uuid();
  esp_fill_random(&nodeBootToken, sizeof(nodeBootToken));
  if (!nodeBootToken)
    nodeBootToken = 1;
  nodeLease.bootToken(nodeBootToken);
  session = esp_random();
  if (!session)
    session = 1;
  if (IRRIGATION_REMOTE_NODE) {
    config.count = 1;
    config.zoneIds[0] = "node_local";
    config.actuatorIds[0] = "valve_local";
    config.moistureCount = (hardware::moistureMask & 1) ? 1 : 0;
    config.moistureEnabled[0] = config.moistureCount;
    config.moistureSlot[0] = 0;
    config.actuatorSlot[0] = 0;
    config.temperatureCount = hardware::temperatureEnabled ? 1 : 0;
    config.temperatureEnabled[0] = config.temperatureCount;
    config.temperatureSlot[0] = 0;
  }
  String stored =
      IRRIGATION_REMOTE_NODE ? String() : store.getString("config", "");
  if (stored.length()) {
    String error;
    if (!applyEnvelope(stored, error))
      Serial.printf("configuration rejected: %s\n", error.c_str());
  }
#if !IRRIGATION_ROLE_MATRIX
  for (unsigned i = 0; i < 4; ++i) {
    pinMode(hardware::moisturePins[i], INPUT);
    temperatureRom[i] = store.getString(("rom" + String(i)).c_str(), "");
  }
  temperatures.begin();
  temperatures.setResolution(11);
  temperatures.setWaitForConversion(false);
#endif
  if (IOT_RS485_ENABLED) {
    digitalWrite(IOT_RS485_DE, LOW);
    pinMode(IOT_RS485_DE, OUTPUT);
    remoteSerial.begin(9600, SERIAL_8N1, IOT_RS485_RX, IOT_RS485_TX);
  }
  // Boot never resumes outputs; retain hold if recovery/fault or remote state
  // unknown.
  bootHold = !storageReady;
  for (unsigned i = 0; i < config.count; ++i) {
    String key = "run" + String(i);
    if (store.getBool(key.c_str(), false))
      journal.event(i, "interrupted", "restart_interrupted", Source::Manual);
    outputs.off(i);
  }
  if (strlen(IOT_WIFI_SSID)) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(IOT_WIFI_SSID, IOT_WIFI_PASSWORD);
    sntp_set_time_sync_notification_cb(
        [](struct timeval *) { clockSynchronized.store(true); });
    configTime(0, 0, "pool.ntp.org", "time.cloudflare.com");
    if (strlen(IOT_ADMIN_PASSWORD) >= 12)
      localPortal();
    if (!IRRIGATION_REMOTE_NODE && strlen(IOT_PORTAL_URL) &&
        strlen(IOT_DEVICE_TOKEN))
      xTaskCreatePinnedToCore(networkTask, "portal-network", 12288, nullptr, 1,
                              nullptr, 0);
  }
  Serial.printf("firmware=0.2.2 guid=%s physical=%u\n", guid.c_str(),
                IOT_PHYSICAL_OUTPUTS);
  previousEpoch = time(nullptr);
}
void loop() {
  if (!storageReady) {
    delay(10);
    return;
  }
  uint32_t now = millis();
  if (IRRIGATION_REMOTE_NODE) {
    web.handleClient();
    nodeLoop(millis());
    delay(1);
    return;
  }
  web.handleClient();
  now = millis();
  Input *input = nullptr;
  if (xQueueReceive(incoming, &input, 0) == pdTRUE) {
    if (input->kind == "configuration") {
      if (!controller.activeCount()) {
        String error;
        bool ok = applyEnvelope(input->body, error);
        DynamicJsonDocument env(24576);
        deserializeJson(env, input->body);
        DynamicJsonDocument ack(512);
        ack["revision"] = env["revision"];
        ack["configuration_hash"] = env["configuration_hash"];
        ack["status"] = ok ? "applied" : "rejected";
        if (ok)
          ack["idle"] = true;
        else
          ack["reason"] = error;
        String raw;
        serializeJson(ack, raw);
        queueJob("/iot/v1/configuration/ack", raw);
      }
    } else {
      DynamicJsonDocument d(2048);
      if (!deserializeJson(d, input->body))
        handleCommand(d.as<JsonObjectConst>(), false);
    }
    delete input;
  }
#if IRRIGATION_ROLE_MATRIX
  remotePoll(now);
#else
  readSensors(now);
#endif
  if (bootHold)
    for (unsigned i = 0; i < config.count; ++i)
      if (controller.run(i).active)
        controller.stop(i, millis(), "storage_hold");
  now = millis();
  controller.clockValid(clockReady() && !bootHold && outputs.allKnown());
  for (unsigned i = 0; i < 4; ++i) {
    Reading r = moisture[i];
    r.valid = r.valid && config.moistureEnabled[i];
    controller.reading(i, r);
  }
  if (uint32_t(now - lastTick) >= 100) {
    lastTick = now;
    time_t epoch = time(nullptr);
    const bool observed = recoverSchedules(epoch, now);
    controller.clockValid(observed && !bootHold && outputs.allKnown());
    Window windows[4];
    for (size_t i = 0; i < config.slotCount; ++i) {
      const auto &s = config.slots[i];
      auto w = observed ? evaluate(s, epoch, previousEpoch, revision) : Window{};
      if (w.open && (!windows[s.zone].open || w.startNow))
        windows[s.zone] = w;
    }
    controller.tick(now, windows);
    if (observed) previousEpoch = epoch;
  }
  if (uint32_t(now - lastSample) >= 900000) {
    lastSample = now;
    reportSample();
  }
  static uint32_t runtimeAt = 0;
  if (uint32_t(now - runtimeAt) >= 15000) {
    runtimeAt = now;
    reportRuntime();
  }
  delay(1);
}

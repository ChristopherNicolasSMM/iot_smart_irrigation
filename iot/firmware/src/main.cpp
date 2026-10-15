#include "../portal/LocalPortalPage.h"
#include "AuditReplay.h"
#include "Auth.h"
#include "Controller.h"
#include "FirmwareConfig.h"
#include "HardwareConfig.h"
#include "JsonSpan.h"
#include "LocalPortalPolicy.h"
#include "OccurrenceLedger.h"
#include "Provision.h"
#include "ProvisionCodec.h"
#include "QueueCodec.h"
#include "Recovery.h"
#include "RecoveryPlan.h"
#include "RemoteProtocol.h"
#include "ScheduleDiagnostics.h"
#include "SecurityPolicy.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <atomic>
#include <ctype.h>
#include <errno.h>
#include <esp_flash_encrypt.h>
#include <esp_idf_version.h>
#include <esp_partition.h>
#include <esp_secure_boot.h>
#include <esp_sntp.h>
#include <mbedtls/x509_crt.h>
#include <nvs_flash.h>
static_assert(ESP_IDF_VERSION_MAJOR >= 5,
              "ESP-IDF 5+ required for system time64");
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
#ifndef IOT_REQUIRE_HARDENED_BOOT
#define IOT_REQUIRE_HARDENED_BOOT 0
#endif
constexpr size_t bodyLimit = 16384;
constexpr size_t recoveryLimit = 32768;
SecurityState security;
bool recoveryHold = false, recoveryFault = false;
SemaphoreHandle_t networkMutex;
struct ScopedMutex {
  SemaphoreHandle_t handle;
  bool held;
  explicit ScopedMutex(SemaphoreHandle_t value)
      : handle(value), held(value && xSemaphoreTake(value, 0) == pdTRUE) {}
  ~ScopedMutex() {
    if (held)
      xSemaphoreGive(handle);
  }
};
constexpr uint32_t linkLeaseMs = 3000;
Preferences store;
ProvisionSettings provision;
uint32_t provisionRevision = 0, restartAt = 0;
bool provisionFault = false, restartPending = false, usbProvisionWindow = true;
String serialProvisionLine;
bool serialProvisionOverflow = false;
uint32_t serialProvisionLast = 0;

SemaphoreHandle_t storeMutex, outboxMutex;
uint64_t auditOriginFloor = 0;
String lastAuditEventId;
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
OccurrenceLedger occurrences;
bool occurrencesReady = false;
uint64_t lastScheduleObservation[3][maxSlots] = {};
int runScheduleSlot[4] = {-1, -1, -1, -1};
Window runScheduleWindow[4];
int plannedScheduleSlot[4] = {-1, -1, -1, -1};
Window plannedScheduleWindow[4];
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
  return clockSynchronized.load() && validClockEpoch(int64_t(n));
}
String digest(const String &raw) {
  uint8_t hash[32];
  if (mbedtls_sha256(reinterpret_cast<const unsigned char *>(raw.c_str()),
                     raw.length(), hash, 0) != 0)
    return "";
  char out[65];
  for (int i = 0; i < 32; ++i)
    snprintf(out + i * 2, 3, "%02x", hash[i]);
  return String(out);
}
bool lockStore() {
  return xSemaphoreTake(storeMutex, pdMS_TO_TICKS(50)) == pdTRUE;
}
void unlockStore() { xSemaphoreGive(storeMutex); }
bool readQueueMeta(QueueMeta &meta) { // Caller holds storeMutex.
  if (store.isKey("queue_v2")) {
    DynamicJsonDocument doc(6144);
    return !deserializeJson(doc, store.getString("queue_v2", "")) &&
           decodeQueueMeta(doc.as<JsonObjectConst>(), meta);
  }
  unsigned head = 0, count = 0;
  const String old = store.getString("qmeta", "0,0");
  if (sscanf(old.c_str(), "%u,%u", &head, &count) != 2 || head >= 16 ||
      count > 16)
    return false;
  meta.head = head;
  meta.count = count;
  return true;
}
bool writeQueueMeta(const QueueMeta &meta) {
  if (!meta.valid())
    return false;
  DynamicJsonDocument doc(6144);
  queueMetaJson(doc.to<JsonObject>(), meta);
  if (doc.overflowed())
    return false;
  String raw;
  serializeJson(doc, raw);
  return store.putString("queue_v2", raw) == raw.length();
}
bool queueJob(const String &path, const String &body) {
  if (body.length() > 4096 || !storageReady || !lockStore()) {
    ++lostEvents;
    return false;
  }
  QueueMeta meta;
  bool ok = false;
  if (readQueueMeta(meta) && meta.count < 16) {
    DynamicJsonDocument j(body.length() + 512);
    j["path"] = path;
    j["body"] = body;
    String raw;
    serializeJson(j, raw);
    String key = "q" + String((meta.head + meta.count) % 16);
    ok = store.putString(key.c_str(), raw) == raw.length() && meta.enqueue() &&
         writeQueueMeta(meta);
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
String hexKey(uint64_t key) {
  char text[17];
  snprintf(text, sizeof(text), "%016llx", static_cast<unsigned long long>(key));
  return String(text);
}
void scheduleContext(JsonObject row, int index, const Window &window) {
  if (index < 0 || size_t(index) >= config.slotCount || !window.occurrence)
    return;
  row["schedule_id"] = config.slotScheduleIds[index];
  if (config.slotTimeIds[index].length())
    row["time_slot_id"] = config.slotTimeIds[index];
  row["occurrence_id"] = hexKey(window.occurrence);
  row["window_start"] = iso(time_t(window.begins));
  const int64_t end = window.begins + config.slots[index].durationSecond;
  if (validClockEpoch(end))
    row["window_end"] = iso(time_t(end));
  else
    row["window_end"] = nullptr;
}
bool persistAudit(size_t slot, const char *kind, const char *reason,
                  Source source, int scheduleIndex = -1,
                  const Window *window = nullptr, int competing = -1,
                  const String &eventId = String(),
                  const String &commandId = String()) {
  if (!storageReady || !lockStore()) {
    bootHold = true;
    return false;
  }
  uint64_t seq = store.getULong64("aseq", 0) + 1;
  DynamicJsonDocument d(2048);
  const String id =
      eventId.length()
          ? eventId
          : ((strcmp(kind, "requested") && strcmp(kind, "retention"))
                 ? uuid()
                 : String());
  if (id.length()) {
    d["event_id"] = id;
    d["device_guid"] = guid;
    d["actuator_id"] = config.actuatorIds[slot];
  }
  if (commandId.length())
    d["command_id"] = commandId;
  d["sequence"] = seq;
  d["zone_id"] = config.zoneIds[slot];
  d["kind"] = kind;
  d["reason"] = reason;
  d["source"] = sourceName(source);
  d["uptime_ms"] = millis();
  d["revision"] = revision;
  if (window)
    scheduleContext(d.as<JsonObject>(), scheduleIndex, *window);
  if (competing >= 0 && size_t(competing) < config.slotCount) {
    d["competing_schedule_id"] = config.slotScheduleIds[competing];
    if (config.slotTimeIds[competing].length())
      d["competing_time_slot_id"] = config.slotTimeIds[competing];
  }
  if (clockReady())
    d["recorded_at"] = iso(time(nullptr));
  else
    d["recorded_at"] = nullptr;
  if (d.overflowed()) {
    unlockStore();
    bootHold = true;
    return false;
  }
  String raw;
  const size_t expected = measureJson(d);
  if (serializeJson(d, raw) != expected || raw.length() != expected) {
    unlockStore();
    bootHold = true;
    return false;
  }
  String key = "a" + String(uint32_t(seq % 64));
  bool ok = store.putString(key.c_str(), raw) == raw.length() &&
            store.putULong64("aseq", seq) == 8;
  unlockStore();
  if (!ok)
    bootHold = true;
  else
    lastAuditEventId = id;
  return ok;
}
// One NVS blob contains both tombstone and records. A successful write must
// precede both RAM mutation and GPIO/RS485 activation.
bool saveOccurrences(const OccurrenceLedger &next) {
  const bool saved = commitOccurrenceLedger(
      occurrences, next, [](const uint8_t *bytes, size_t length) {
        if (!lockStore())
          return false;
        const bool ok = store.putBytes("occ_v2", bytes, length) == length;
        unlockStore();
        return ok;
      });
  if (!saved)
    bootHold = true;
  return saved;
}
struct EventJournal : Journal {
  bool reserve(size_t slot, Source source) override {
    const bool scheduled = source == Source::Static || source == Source::Hybrid;
    if (scheduled) {
      runScheduleSlot[slot] = plannedScheduleSlot[slot];
      runScheduleWindow[slot] = plannedScheduleWindow[slot];
    }
    return persistAudit(slot, "requested", "output_attempt", source,
                        scheduled ? runScheduleSlot[slot] : -1,
                        scheduled ? &runScheduleWindow[slot] : nullptr);
  }
  bool contains(size_t slot, uint64_t key) const override {
    return occurrencesReady && occurrences.contains(slot, key);
  }
  // All production calls must carry a civil window start; no untimed append.
  bool consume(size_t, uint64_t) override { return false; }
  bool consumeWindow(size_t slot, uint64_t key, int64_t begins) override {
    if (!occurrencesReady)
      return false;
    auto next = std::unique_ptr<OccurrenceLedger>(
        new (std::nothrow) OccurrenceLedger(occurrences));
    if (!next) {
      bootHold = true;
      return false;
    }
    auto result = next->add(slot, key, begins);
    if (result == OccurrenceLedger::Result::Duplicate)
      return true;
    if (result != OccurrenceLedger::Result::Added) {
      bootHold = true;
      return false;
    }
    if (!saveOccurrences(*next))
      return false;
    return true;
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
    const bool scheduled = source == Source::Static || source == Source::Hybrid;
    if (scheduled && strcmp(kind, "completed") && strcmp(kind, "interrupted")) {
      runScheduleSlot[slot] = plannedScheduleSlot[slot];
      runScheduleWindow[slot] = plannedScheduleWindow[slot];
    }
    if (!persistAudit(slot, actual, why, source,
                      scheduled ? runScheduleSlot[slot] : -1,
                      scheduled ? &runScheduleWindow[slot] : nullptr, -1,
                      String(), eventCommand))
      return;
    const String auditEventId = lastAuditEventId;
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
    e["event_id"] = auditEventId;
    e["device_guid"] = guid;
    e["zone_id"] = config.zoneIds[slot];
    e["actuator_id"] = config.actuatorIds[slot];
    e["kind"] = actual;
    e["reason"] = why;
    e["source"] = sourceName(source);
    e["recorded_at"] = iso(time(nullptr));
    e["revision"] = revision;
    if (scheduled)
      scheduleContext(e, runScheduleSlot[slot], runScheduleWindow[slot]);
    if (eventCommand.length())
      e["command_id"] = eventCommand;
    if (doc.overflowed()) {
      ++lostEvents;
      return;
    }
    String raw;
    const size_t expected = measureJson(doc);
    if (serializeJson(doc, raw) != expected || raw.length() != expected) {
      ++lostEvents;
      return;
    }
    queueJob("/iot/v1/telemetry", raw);
  }
} journal;
Controller controller(outputs, journal);
// The 24-hour horizon matches the maximum schedule duration. Compact at
// hourly increments, not every loop. Migration fences all pre-upgrade windows
// because legacy hashes do not carry timestamps; legacy keys remain untouched.
bool maintainOccurrences(time_t epoch) {
  if (!clockReady())
    return false;
  if (occurrencesReady && int64_t(epoch) < occurrences.floor)
    return false;
  if (occurrencesReady && !occurrences.compactionDue(int64_t(epoch)))
    return true;
  auto next = std::unique_ptr<OccurrenceLedger>(
      new (std::nothrow) OccurrenceLedger(occurrences));
  if (!next) {
    bootHold = true;
    return false;
  }
  const bool migrating = !occurrencesReady;
  if (!(migrating ? next->advance(int64_t(epoch))
                  : next->compact(int64_t(epoch))) ||
      !saveOccurrences(*next))
    return false;
  occurrencesReady = true;
  controller.clearOccurrenceCache();
  for (size_t i = 0; i < config.count; ++i)
    if (!persistAudit(i, "retention",
                      migrating ? "legacy_fenced" : "temporal_compaction",
                      Source::Static))
      return false;
  return true;
}
// Diagnostic telemetry is best effort under the existing bounded outbox policy.
// Save locally first; stable event IDs let central ingestion deduplicate reboot
// retransmissions even when the RAM suppression cache has been lost.
bool scheduleObservation(size_t index, const Window &window, const char *kind,
                         const char *reason, unsigned category,
                         int competing = -1, bool historyTruncated = false) {
  const auto &s = config.slots[index];
  const auto key = diagnosticKey(s, window, revision, reason);
  if (lastScheduleObservation[category][index] == key)
    return true;
  const auto source = config.zones[s.zone].mode == Mode::Hybrid
                          ? Source::Hybrid
                          : Source::Static;
  const String stableHash =
      digest(guid + ":" + String(revision) + ":" + hexKey(key));
  if (stableHash.length() != 64) {
    bootHold = true;
    return false;
  }
  const String eventId = String("schedule-") + stableHash;
  if (!persistAudit(s.zone, kind, reason, source, int(index), &window,
                    competing, eventId))
    return false;
  DynamicJsonDocument doc(3072);
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
  JsonObject event = doc.createNestedArray("events").createNestedObject();
  event["event_id"] = eventId;
  event["device_guid"] = guid;
  event["zone_id"] = config.zoneIds[s.zone];
  event["actuator_id"] = config.actuatorIds[s.zone];
  event["revision"] = revision;
  event["kind"] = kind;
  event["reason"] = reason;
  event["source"] = sourceName(source);
  event["recorded_at"] = iso(time(nullptr));
  scheduleContext(event, int(index), window);
  event["history_truncated"] = historyTruncated;
  if (!s.date)
    event["weekly_lookback_days"] = 7;
  if (competing >= 0) {
    event["competing_schedule_id"] = config.slotScheduleIds[competing];
    if (config.slotTimeIds[competing].length())
      event["competing_time_slot_id"] = config.slotTimeIds[competing];
  }
  if (doc.overflowed()) {
    bootHold = true;
    return false;
  }
  String raw;
  const size_t expected = measureJson(doc);
  if (serializeJson(doc, raw) != expected || raw.length() != expected) {
    bootHold = true;
    return false;
  }
  queueJob("/iot/v1/telemetry", raw); // failure counted by existing lost_events
  lastScheduleObservation[category][index] = key;
  return true;
}
// A checkpoint describes observation, not proof of valve execution. The last
// minute before a power loss may be reported again as unobserved after boot.
bool recoverSchedules(time_t epoch, uint32_t now) {
  if (!clockReady())
    return false;
  int64_t from = previousEpoch;
  if (recoveryPending) {
    if (!lockStore()) {
      bootHold = true;
      return false;
    }
    String raw = store.getString("observation", "");
    unlockStore();
    DynamicJsonDocument checkpoint(256);
    from = epoch;
    if (raw.length()) {
      if (deserializeJson(checkpoint, raw) ||
          !checkpoint["epoch"].is<int64_t>() ||
          !checkpoint["revision"].is<uint32_t>() ||
          !validClockEpoch(checkpoint["epoch"].as<int64_t>())) {
        bootHold = true;
        return false;
      }
      if (checkpoint["revision"].as<uint32_t>() == revision)
        from = checkpoint["epoch"].as<int64_t>();
    }
  }
  // Never advance a checkpoint backwards or allow old windows after rollback.
  if (int64_t(epoch) < from)
    return false;
  if (recoveryPending || int64_t(epoch) - from > 3) {
    unsigned counts[4] = {};
    bool truncated[4] = {};
    for (size_t i = 0; i < config.slotCount; ++i) {
      const auto &s = config.slots[i];
      if (!config.zones[s.zone].enabled ||
          config.zones[s.zone].mode == Mode::Dynamic)
        continue;
      auto result = unobserved(s, from, epoch);
      counts[s.zone] += result.windows;
      truncated[s.zone] = truncated[s.zone] || result.truncated;
      if (!visitUnobserved(s, from, epoch, revision, [&](const Window &window) {
            return scheduleObservation(i, window, "recovery_observation",
                                       "unobserved_window", 0, -1,
                                       result.truncated);
          }))
        return false;
    }
    for (size_t i = 0; i < config.count; ++i) {
      const Source source = config.zones[i].mode == Mode::Hybrid
                                ? Source::Hybrid
                                : Source::Static;
      if (counts[i]) {
        String reason = "unobserved_closed_windows_" + String(counts[i]);
        if (!persistAudit(i, "recovery_summary", reason.c_str(), source))
          return false;
      }
      if (truncated[i] && !persistAudit(i, "recovery_summary",
                                        "weekly_history_limited_7d", source))
        return false;
    }
  }
  if (recoveryPending || int64_t(epoch) - from > 3) {
    for (size_t i = 0; i < config.slotCount; ++i) {
      const auto &s = config.slots[i];
      if (!config.zones[s.zone].enabled ||
          config.zones[s.zone].mode == Mode::Dynamic)
        continue;
      const auto window = evaluate(s, epoch, epoch, revision);
      if (!window.open)
        continue;
      // Also consume an interrupted open window: hybrid must not start late
      // on the next tick simply because moisture is below its threshold.
      if (!occurrences.retired(window.begins) &&
          !journal.consumeWindow(s.zone, window.occurrence, window.begins)) {
        bootHold = true;
        return false;
      }
      controller.restoreOccurrence(s.zone, window.occurrence);
      if (!scheduleObservation(i, window, "omitted", "open_window_not_resumed",
                               1, -1,
                               !s.date && from < int64_t(epoch) - 7 * 86400))
        return false;
    }
  }
  if (recoveryPending || uint32_t(now - checkpointAt) >= 60000) {
    DynamicJsonDocument checkpoint(256);
    checkpoint["revision"] = revision;
    checkpoint["epoch"] = int64_t(epoch);
    String raw;
    serializeJson(checkpoint, raw);
    if (!lockStore()) {
      bootHold = true;
      return false;
    }
    bool ok = store.putString("observation", raw) == raw.length();
    unlockStore();
    if (!ok) {
      bootHold = true;
      return false;
    }
    checkpointAt = now;
  }
  if (recoveryPending)
    previousEpoch = epoch;
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
  if (!selectTimezone(config.timezone.c_str())) {
    error = "timezone_unavailable";
    bootHold = true;
    return false;
  }
  previousEpoch = time(nullptr);
  recoveryPending = true;
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

ProvisionSettings compiledProvision() {
  ProvisionSettings p;
  p.ssid = IOT_WIFI_SSID;
  p.wifiPassword = IOT_WIFI_PASSWORD;
  p.adminPassword = IOT_ADMIN_PASSWORD;
  p.operatorPassword = IOT_OPERATOR_PASSWORD;
  p.portalUrl = IOT_PORTAL_URL;
  p.deviceToken = IOT_DEVICE_TOKEN;
  p.portalCa = IOT_PORTAL_CA;
  for (unsigned i = 0; i < 4; ++i) {
    p.actuators[i] = actuatorBinding[i];
    p.moisture[i] = moistureBinding[i];
    p.temperature[i] = temperatureBinding[i];
  }
  return p;
}
bool provisionCertificate(const ProvisionSettings &p) {
  if (p.portalUrl.empty())
    return true;
  mbedtls_x509_crt ca;
  mbedtls_x509_crt_init(&ca);
  const int code = mbedtls_x509_crt_parse(
      &ca, reinterpret_cast<const unsigned char *>(p.portalCa.c_str()),
      p.portalCa.size() + 1);
  mbedtls_x509_crt_free(&ca);
  return code == 0;
}
void loadProvision() {
  provision = compiledProvision();
  if (store.isKey("prov_v1")) {
    const String record = store.getString("prov_v1", "");
    DynamicJsonDocument doc(24576);
    size_t begin = 0, end = 0;
    String error;
    ProvisionSettings saved;
    bool valid =
        record.length() <= bodyLimit && !deserializeJson(doc, record) &&
        doc["schema_version"] == 1 && doc["profile_revision"].is<uint32_t>() &&
        doc["profile_revision"].as<uint32_t>() > 0 &&
        doc["settings_hash"].as<String>().length() == 64 &&
        doc["settings"].is<JsonObject>() &&
        objectMember(std::string(record.c_str(), record.length()), "settings",
                     begin, end) &&
        digest(record.substring(begin, end)) ==
            doc["settings_hash"].as<String>() &&
        provisionPatch(doc["settings"].as<JsonObjectConst>(), saved, error) &&
        provisionCertificate(saved);
    if (valid) {
      provision = std::move(saved);
      provisionRevision = doc["profile_revision"];
      auditOriginFloor = doc["audit_origin_floor"] | uint64_t(0);
    } else {
      provisionFault = true;
      Serial.println(
          "provision_fault: network blocked; recover via USB, preserve NVS");
    }
  } else if (!provision.ssid.empty() && (validateProvision(provision) ||
                                         !provisionCertificate(provision))) {
    provisionFault = true;
    Serial.println("compiled_provision_invalid: recover via USB");
  }
  for (unsigned i = 0; i < 4; ++i) {
    actuatorBinding[i] = provision.actuators[i].c_str();
    moistureBinding[i] = provision.moisture[i].c_str();
    temperatureBinding[i] = provision.temperature[i].c_str();
  }
}
String provisionStatus() {
  DynamicJsonDocument doc(3072);
  JsonObject result = doc.to<JsonObject>();
  provisionJson(result, provision, false);
  result["profile_revision"] = provisionRevision;
  result["configuration_revision"] = revision;
  result["recovery_hold"] = recoveryHold;
  result["recovery_fault"] = recoveryFault;
  result["usb_recovery_allowed"] = security.usbRecoveryAllowed();
  result["guid"] = guid;
  result["provision_fault"] = provisionFault;
  result["restart_pending"] = restartPending;
  result["wifi_connected"] = WiFi.status() == WL_CONNECTED;
  result["ip"] =
      WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : String();
  result["usb_window_open"] = usbProvisionWindow && millis() < 180000;
  String raw;
  serializeJson(doc, raw);
  return raw;
}
bool saveProvision(const String &body, bool authorized, String &error) {
  if (recoveryFault) {
    error = "recovery_stage_fault";
    return false;
  }
  if (body.length() > bodyLimit - 512) {
    error = "provision_too_large";
    return false;
  }
  ProvisionSettings candidate = provision;
  uint32_t expected = 0;
  { // The request document must be released before serializing the new profile.
    DynamicJsonDocument doc(24576);
    if (deserializeJson(doc, body, DeserializationOption::NestingLimit(8)) ||
        !doc["expected_revision"].is<uint32_t>() ||
        !doc["settings"].is<JsonObject>()) {
      error = "provision_request_invalid";
      return false;
    }
    for (JsonPairConst pair : doc.as<JsonObjectConst>())
      if (strcmp(pair.key().c_str(), "expected_revision") &&
          strcmp(pair.key().c_str(), "settings")) {
        error = "provision_field_unknown";
        return false;
      }
    expected = doc["expected_revision"];
    if (!provisionPatch(doc["settings"].as<JsonObjectConst>(), candidate,
                        error))
      return false;
  }
  const bool idle = !controller.activeCount() && !nodeLease.active() &&
                    (!IOT_PHYSICAL_OUTPUTS || outputs.allKnown());
  // Queue records have no destination: never redirect retained data to a new
  // origin.
  if (!lockStore()) {
    error = "storage_busy";
    return false;
  }
  QueueMeta queueMeta;
  const bool queueEmpty = readQueueMeta(queueMeta) && queueMeta.count == 0;
  const uint64_t originFloor = candidate.portalUrl != provision.portalUrl
                                   ? store.getULong64("aseq", 0)
                                   : auditOriginFloor;
  unlockStore();
  const char *reason = provisionWriteRejection(
      authorized, idle, restartPending, expected, provisionRevision,
      candidate.portalUrl != provision.portalUrl, queueEmpty);
  if (reason) {
    error = reason;
    return false;
  }
  if (!provisionCertificate(candidate)) {
    error = "portal_ca_invalid";
    return false;
  }
  DynamicJsonDocument settings(24576);
  provisionJson(settings.to<JsonObject>(), candidate, true);
  if (settings.overflowed()) {
    error = "provision_too_large";
    return false;
  }
  String raw;
  serializeJson(settings, raw);
  const String hash = digest(raw);
  char originText[21];
  snprintf(originText, sizeof(originText), "%llu",
           static_cast<unsigned long long>(originFloor));
  const String record = String("{\"schema_version\":1,\"profile_revision\":") +
                        String(provisionRevision + 1) +
                        ",\"audit_origin_floor\":" + String(originText) +
                        ",\"settings_hash\":\"" + hash +
                        "\",\"settings\":" + raw + "}";
  if (hash.length() != 64 || record.length() > bodyLimit) {
    error = "provision_too_large";
    return false;
  }
  if (!lockStore()) {
    error = "storage_busy";
    return false;
  }
  const bool ok = store.putString("prov_v1", record) == record.length();
  unlockStore();
  if (!ok) {
    error = "provision_persistence_failed";
    return false;
  }
  // Apply only after reboot. Network task keeps using the old immutable
  // snapshot.
  bootHold = true;
  restartPending = true;
  restartAt = millis();
  return true;
}
struct RecoveryStore {
  std::string guid() { return store.getString("guid", "").c_str(); }
  bool hold() { return store.putBool("recover_hold", true) == 1; }
  bool text(const std::string &key, const std::string &value) {
    if (store.isKey(key.c_str()) &&
        store.getString(key.c_str(), "") == value.c_str())
      return true;
    const size_t written = store.putString(key.c_str(), value.c_str());
    return written == value.size() && store.isKey(key.c_str()) &&
           store.getString(key.c_str(), "") == value.c_str();
  }
  bool removeStage() {
    return !store.isKey("restore_v1") || store.remove("restore_v1");
  }
};
bool recoveryIdle() {
  return !controller.activeCount() && !nodeLease.active() && !restartPending &&
         (!IOT_PHYSICAL_OUTPUTS || outputs.allKnown());
}
String recoveryBackup(String &error) {
  if (!security.usbRecoveryAllowed() || !usbProvisionWindow ||
      millis() >= 180000 || !recoveryIdle() || recoveryFault ||
      validateProvision(provision)) {
    error = "recovery_not_available";
    return String();
  }
  ScopedMutex network(networkMutex);
  if (!network.held) {
    error = "network_busy";
    return String();
  }
  DynamicJsonDocument doc(65536);
  doc["schema_version"] = 1;
  doc["device_guid"] = guid;
  doc["firmware"] = "0.6.0-experimental";
  doc["recorded_at"] = clockReady() ? iso(time(nullptr)) : String();
  provisionJson(doc.createNestedObject("settings"), provision, true);
  if (!lockStore()) {
    error = "storage_busy";
    return String();
  }
  doc["configuration_envelope"] = store.getString("config", "");
  JsonArray sensors = doc.createNestedArray("calibration");
  for (unsigned i = 0; i < 4; ++i) {
    JsonObject row = sensors.createNestedObject();
    row["slot"] = i;
    row["calibration"] = store.getString(("cal" + String(i)).c_str(), "");
    row["rom"] = store.getString(("rom" + String(i)).c_str(), "");
  }
  unlockStore();
  if (doc.overflowed()) {
    error = "recovery_too_large";
    return String();
  }
  String raw;
  serializeJson(doc, raw);
  if (raw.length() > recoveryLimit - 512) {
    error = "recovery_too_large";
    return String();
  }
  return raw;
}
bool prepareRecovery(JsonObjectConst backup, RecoveryPlan &plan,
                     String &error) {
  if (backup["schema_version"] != 1 ||
      !backup["device_guid"].is<const char *>() ||
      !backup["settings"].is<JsonObjectConst>() ||
      !backup["configuration_envelope"].is<const char *>() ||
      !backup["calibration"].is<JsonArrayConst>() ||
      backup["calibration"].size() != 4) {
    error = "recovery_snapshot_invalid";
    return false;
  }
  ProvisionSettings candidate;
  if (!provisionPatch(backup["settings"].as<JsonObjectConst>(), candidate,
                      error) ||
      !provisionCertificate(candidate))
    return false;
  if (provisionRevision == UINT32_MAX) {
    error = "provision_revision_exhausted";
    return false;
  }
  const String configEnvelope = backup["configuration_envelope"].as<String>();
  uint32_t snapshotRevision = 0;
  String snapshotHash;
  if (configEnvelope.length()) {
    DynamicJsonDocument envelope(24576);
    size_t begin = 0, end = 0;
    if (configEnvelope.length() > bodyLimit ||
        deserializeJson(envelope, configEnvelope) ||
        envelope["schema_version"] != 1 ||
        envelope["device_guid"].as<String>() != guid ||
        !envelope["revision"].is<uint32_t>() ||
        envelope["revision"].as<uint32_t>() == 0 ||
        !objectMember(
            std::string(configEnvelope.c_str(), configEnvelope.length()),
            "configuration", begin, end)) {
      error = "recovery_configuration_invalid";
      return false;
    }
    snapshotRevision = envelope["revision"];
    snapshotHash = envelope["configuration_hash"].as<String>();
    auto parsed =
        std::unique_ptr<FirmwareConfig>(new (std::nothrow) FirmwareConfig());
    if (snapshotHash.length() != 64 ||
        digest(configEnvelope.substring(begin, end)) != snapshotHash ||
        !parsed ||
        !parseConfiguration(envelope["configuration"].as<JsonObjectConst>(),
                            *parsed, error)) {
      error = "recovery_configuration_invalid";
      return false;
    }
  }
  if (!lockStore()) {
    error = "storage_busy";
    return false;
  }
  QueueMeta queue;
  const bool empty = readQueueMeta(queue) && queue.count == 0;
  const uint64_t originFloor = candidate.portalUrl != provision.portalUrl
                                   ? store.getULong64("aseq", 0)
                                   : auditOriginFloor;
  unlockStore();
  const char *reason =
      recoveryRejection(guid.c_str(), backup["device_guid"].as<std::string>(),
                        recoveryIdle(), revision, snapshotRevision,
                        snapshotRevision && snapshotRevision == revision &&
                            configHash.length() && snapshotHash != configHash,
                        candidate.portalUrl != provision.portalUrl, empty);
  if (reason) {
    error = reason;
    return false;
  }
  std::array<bool, 4> seen{};
  for (JsonObjectConst row : backup["calibration"].as<JsonArrayConst>()) {
    if (!row["slot"].is<unsigned>() || row["slot"].as<unsigned>() >= 4 ||
        !row["calibration"].is<const char *>() ||
        !row["rom"].is<const char *>()) {
      error = "recovery_calibration_invalid";
      return false;
    }
    const unsigned slot = row["slot"];
    if (seen[slot]) {
      error = "recovery_calibration_invalid";
      return false;
    }
    seen[slot] = true;
    String cal = row["calibration"].as<String>(), rom = row["rom"].as<String>();
    int dry = 0, wet = 0;
    char tail = 0;
    if (cal.length() &&
        (cal.length() > 24 ||
         sscanf(cal.c_str(), "%d,%d%c", &dry, &wet, &tail) != 2 || dry < 0 ||
         dry > 4095 || wet < 0 || wet > 4095 || abs(dry - wet) < 200)) {
      error = "recovery_calibration_invalid";
      return false;
    }
    if (rom.length() != 0 && rom.length() != 16) {
      error = "recovery_rom_invalid";
      return false;
    }
    for (size_t j = 0; j < rom.length(); ++j)
      if (!isxdigit(rom[j])) {
        error = "recovery_rom_invalid";
        return false;
      }
    plan.calibration[slot] = cal.c_str();
    plan.roms[slot] = rom.c_str();
  }
  DynamicJsonDocument settings(24576);
  provisionJson(settings.to<JsonObject>(), candidate, true);
  if (settings.overflowed()) {
    error = "recovery_too_large";
    return false;
  }
  String raw;
  serializeJson(settings, raw);
  const String hash = digest(raw);
  char floor[21];
  snprintf(floor, sizeof(floor), "%llu",
           static_cast<unsigned long long>(originFloor));
  plan.guid = guid.c_str();
  plan.configuration = configEnvelope.c_str();
  plan.tokenHash = digest(String(candidate.deviceToken.c_str())).c_str();
  const String profile =
      String("{\"schema_version\":1,\"profile_revision\":") +
      String(provisionRevision + 1) + ",\"audit_origin_floor\":" + floor +
      ",\"settings_hash\":\"" + hash + "\",\"settings\":" + raw + "}";
  if (hash.length() != 64 || profile.length() > bodyLimit) {
    error = "recovery_too_large";
    return false;
  }
  plan.profile = profile.c_str();
  return true;
}
bool stageRecovery(const String &raw, String &error) {
  if (!security.usbRecoveryAllowed() || !usbProvisionWindow ||
      millis() >= 180000 || !recoveryIdle() || recoveryFault) {
    error = "recovery_not_available";
    return false;
  }
  ScopedMutex network(networkMutex);
  if (!network.held) {
    error = "network_busy";
    return false;
  }
  if (raw.length() > recoveryLimit - 512) {
    error = "recovery_too_large";
    return false;
  }
  RecoveryPlan plan;
  {
    DynamicJsonDocument doc(65536);
    if (deserializeJson(doc, raw) ||
        !doc["expected_profile_revision"].is<uint32_t>() ||
        !doc["expected_configuration_revision"].is<uint32_t>() ||
        doc["expected_profile_revision"].as<uint32_t>() != provisionRevision ||
        doc["expected_configuration_revision"].as<uint32_t>() != revision ||
        !doc["backup"].is<JsonObjectConst>()) {
      error = "recovery_revision_or_request_invalid";
      return false;
    }
    if (!prepareRecovery(doc["backup"].as<JsonObjectConst>(), plan, error))
      return false;
  }
  DynamicJsonDocument manifest(65536);
  JsonObject body = manifest.to<JsonObject>();
  body["schema_version"] = 1;
  body["device_guid"] = plan.guid.c_str();
  body["profile"] = plan.profile.c_str();
  body["configuration"] = plan.configuration.c_str();
  body["token_hash"] = plan.tokenHash.c_str();
  JsonArray sensors = body.createNestedArray("calibration");
  for (unsigned i = 0; i < 4; ++i) {
    JsonObject row = sensors.createNestedObject();
    row["calibration"] = plan.calibration[i].c_str();
    row["rom"] = plan.roms[i].c_str();
  }
  if (manifest.overflowed()) {
    error = "recovery_too_large";
    return false;
  }
  String serialized;
  serializeJson(manifest, serialized);
  const String hash = digest(serialized);
  const String record =
      String("{\"hash\":\"") + hash + "\",\"body\":" + serialized + "}";
  if (hash.length() != 64 || record.length() > recoveryLimit) {
    error = "recovery_too_large";
    return false;
  }
  if (!lockStore()) {
    error = "storage_busy";
    return false;
  }
  bool ok = !store.isKey("restore_v1") &&
            store.putString("restore_v1", record) == record.length();
  unlockStore();
  if (!ok) {
    error = "recovery_stage_failed";
    return false;
  }
  bootHold = true;
  restartPending = true;
  restartAt = millis();
  return true;
}
bool applyStagedRecovery() { // setup only; no background task has started.
  recoveryHold = store.getBool("recover_hold", false);
  if (!store.isKey("restore_v1"))
    return true;
  // Corrupted staging also holds automation until explicit physical abort.
  if (store.putBool("recover_hold", true) != 1)
    return false;
  recoveryHold = true;
  const String raw = store.getString("restore_v1", "");
  size_t begin = 0, end = 0;
  DynamicJsonDocument doc(65536);
  if (raw.length() > recoveryLimit || deserializeJson(doc, raw) ||
      !objectMember(std::string(raw.c_str(), raw.length()), "body", begin,
                    end) ||
      doc["hash"].as<String>().length() != 64 ||
      digest(raw.substring(begin, end)) != doc["hash"].as<String>())
    return false;
  JsonObjectConst body = doc["body"];
  if (body["schema_version"] != 1 || body["device_guid"].as<String>() != guid ||
      !body["profile"].is<const char *>() ||
      !body["configuration"].is<const char *>() ||
      !body["token_hash"].is<const char *>() || body["calibration"].size() != 4)
    return false;
  RecoveryPlan plan;
  plan.guid = guid.c_str();
  plan.profile = body["profile"].as<std::string>();
  plan.configuration = body["configuration"].as<std::string>();
  plan.tokenHash = body["token_hash"].as<std::string>();
  for (unsigned i = 0; i < 4; ++i) {
    plan.calibration[i] =
        body["calibration"][i]["calibration"].as<std::string>();
    plan.roms[i] = body["calibration"][i]["rom"].as<std::string>();
  }
  RecoveryStore adapter;
  return applyRecoveryPlan(plan, adapter);
}
bool activateRecovery(String &error) {
  if (!recoveryHold || recoveryFault || !recoveryIdle()) {
    error = "recovery_not_available";
    return false;
  }
  if (!lockStore()) {
    error = "storage_busy";
    return false;
  }
  String oldHash = store.getString("recover_token", "");
  if (!provision.portalUrl.empty() &&
      (provision.deviceToken.empty() ||
       digest(String(provision.deviceToken.c_str())) == oldHash)) {
    unlockStore();
    error = "rotate_device_token_after_restore";
    return false;
  }
  const bool ok = store.putBool("recover_hold", false) == 1;
  unlockStore();
  if (!ok) {
    error = "recovery_activation_failed";
    return false;
  }
  bootHold = true;
  restartPending = true;
  restartAt = millis();
  return true;
}

void serialProvision() {
  // Physical recovery is available only during the first three minutes after
  // reset.
  const uint32_t now = millis();
  if (now >= 180000)
    usbProvisionWindow = false;
  if ((serialProvisionLine.length() &&
       uint32_t(now - serialProvisionLast) > 15000)) {
    serialProvisionLine = "";
    serialProvisionOverflow = false;
  }
  for (unsigned budget = 0; budget < 256 && Serial.available(); ++budget) {
    const char c = Serial.read();
    if (c == '\r')
      continue;
    if (c == '\n') {
      if (serialProvisionOverflow)
        Serial.println("PROVISION_RESULT "
                       "{\"saved\":false,\"error\":\"line_too_large_or_usb_"
                       "window_closed\"}");
      else if (serialProvisionLine == "PROVISION?")
        Serial.println(String("PROVISION_STATUS ") + provisionStatus());
      else if (serialProvisionLine == "RECOVERY_BACKUP" ||
               serialProvisionLine.startsWith("RECOVERY_RESTORE ") ||
               serialProvisionLine == "RECOVERY_ACCEPT" ||
               serialProvisionLine == "RECOVERY_ABORT") {
        String error;
        bool ok = false;
        if (serialProvisionLine == "RECOVERY_BACKUP") {
          String raw = recoveryBackup(error);
          if (raw.length()) {
            Serial.println(String("RECOVERY_BACKUP ") + raw);
            serialProvisionLine = "";
            continue;
          }
        } else if (serialProvisionLine.startsWith("RECOVERY_RESTORE "))
          ok = stageRecovery(serialProvisionLine.substring(17), error);
        else if (!usbProvisionWindow || millis() >= 180000 ||
                 !security.usbRecoveryAllowed())
          error = "usb_window_closed_or_production";
        else if (serialProvisionLine == "RECOVERY_ACCEPT")
          ok = activateRecovery(error);
        else if (recoveryIdle() && store.isKey("restore_v1") &&
                 store.remove("restore_v1")) {
          ok = store.putBool("recover_hold", true) == 1;
          bootHold = true;
          restartPending = ok;
          restartAt = millis();
        } else
          error = "recovery_abort_unavailable";
        DynamicJsonDocument result(512);
        result["saved"] = ok;
        result["restart_pending"] = ok;
        if (!ok)
          result["error"] = error;
        String raw;
        serializeJson(result, raw);
        Serial.println(String("RECOVERY_RESULT ") + raw);
      } else if (serialProvisionLine.startsWith("PROVISION ")) {
        if (!usbProvisionWindow || !security.usbRecoveryAllowed()) {
          Serial.println("PROVISION_RESULT "
                         "{\"saved\":false,\"error\":\"usb_window_closed\"}");
          serialProvisionLine = "";
          continue;
        }
        String error;
        const bool ok =
            saveProvision(serialProvisionLine.substring(10), true, error);
        DynamicJsonDocument result(384);
        result["saved"] = ok;
        result["restart_pending"] = ok;
        if (!ok)
          result["error"] = error;
        String raw;
        serializeJson(result, raw);
        Serial.println(String("PROVISION_RESULT ") + raw);
      }
      serialProvisionLine = "";
      serialProvisionOverflow = false;
    } else if (!serialProvisionOverflow) {
      if (serialProvisionLine.length() >=
          (usbProvisionWindow ? recoveryLimit : 64)) {
        serialProvisionLine = "";
        serialProvisionOverflow = true;
      } else
        serialProvisionLine += c;
    }
    serialProvisionLast = now;
  }
}

String statusJson(bool csrfToken, int role = 0) {
  DynamicJsonDocument d(4096);
  d["firmware"] = "0.6.0-experimental";
  d["recovery_hold"] = recoveryHold;
  d["tzdb_version"] = tzdbVersion;
  JsonObject protections = d.createNestedObject("security");
  protections["required"] = security.required;
  protections["flash_encrypted"] = security.flashEncrypted;
  protections["secure_boot"] = security.secureBoot;
  protections["nvs_encrypted"] = security.nvsEncrypted;
  protections["ready"] = security.ready();
  d["wifi_connected"] = WiFi.status() == WL_CONNECTED;
  d["ip"] =
      WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : String();
  d["restart_pending"] = restartPending;
  d["guid"] = guid;
  if (role) {
    d["local_role"] = role == 2 ? "admin" : "operator";
    d["configuration_authority"] =
        strlen(provision.portalUrl.c_str()) || IRRIGATION_REMOTE_NODE
            ? "central"
            : "local";
    JsonObject capabilities = d.createNestedObject("capabilities");
    capabilities["configure"] = role == 2 &&
                                !strlen(provision.portalUrl.c_str()) &&
                                !IRRIGATION_REMOTE_NODE;
    capabilities["calibrate"] = role == 2 && !IRRIGATION_ROLE_MATRIX;
    capabilities["outbox"] = role == 2;
    capabilities["command"] = !IRRIGATION_REMOTE_NODE;
    d["local_role_type"] =
        IRRIGATION_REMOTE_NODE
            ? "remote_node"
            : (IRRIGATION_ROLE_MATRIX ? "matrix" : "sensor_bench");
    d["configuration_body_limit"] = bodyLimit - 512;
  }
  d["revision"] = revision;
  d["clock_valid"] = clockReady();
  d["time_t_bits"] = sizeof(time_t) * 8;
  d["calendar_max_year"] = maxCalendarYear;
  d["occurrence_journal_ready"] = occurrencesReady;
  d["occurrence_floor"] = occurrencesReady ? occurrences.floor : int64_t(0);
  d["retained_occurrences"] = occurrences.count;
  d["occurrence_recent_capacity"] = OccurrenceLedger::capacity;
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
  if (strlen(provision.adminPassword.c_str()) >= 12 &&
      matches("admin", provision.adminPassword.c_str()))
    return 2;
  if (strlen(provision.operatorPassword.c_str()) >= 12 &&
      strcmp(provision.operatorPassword.c_str(),
             provision.adminPassword.c_str()) &&
      matches("operador", provision.operatorPassword.c_str()))
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
bool enqueueAuditSequence(uint64_t seq, const String &expectedId,
                          String &error);
bool readReplayState(AuditReplay &state);
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
    const int role = authenticate();
    if (!role)
      return;
    web.sendHeader("Cache-Control", "no-store");
    web.send(200, "application/json", statusJson(true, role));
  });
  web.on("/api/recovery/activate", HTTP_POST, [] {
    if (!administrator())
      return;
    if (web.header("X-CSRF-Token") != csrf) {
      web.send(403, "application/json", "{\"error\":\"csrf_invalid\"}");
      return;
    }
    String error;
    const bool ok = activateRecovery(error);
    DynamicJsonDocument doc(384);
    doc["activated"] = ok;
    doc["restart_pending"] = ok;
    if (!ok)
      doc["error"] = error;
    String raw;
    serializeJson(doc, raw);
    web.send(ok ? 200 : 409, "application/json", raw);
  });
  web.on("/api/timezones", HTTP_GET, [] {
    if (!authenticate())
      return;
    DynamicJsonDocument doc(16384);
    doc["version"] = tzdbVersion;
    JsonArray names = doc.createNestedArray("names");
    for (const auto &entry : timezoneNames)
      names.add(entry.name);
    if (doc.overflowed()) {
      web.send(503, "application/json", "{}");
      return;
    }
    String raw;
    serializeJson(doc, raw);
    web.send(200, "application/json", raw);
  });
  web.on("/api/provisioning", HTTP_GET, [] {
    if (!administrator())
      return;
    web.sendHeader("Cache-Control", "no-store");
    web.send(200, "application/json", provisionStatus());
  });
  web.on("/api/provisioning", HTTP_POST, [] {
    if (!administrator())
      return;
    if (web.header("X-CSRF-Token") != csrf) {
      web.send(403, "application/json", "{\"error\":\"csrf_invalid\"}");
      return;
    }
    String error;
    const bool ok = saveProvision(web.arg("plain"), true, error);
    DynamicJsonDocument result(384);
    result["saved"] = ok;
    result["restart_pending"] = ok;
    if (!ok)
      result["error"] = error;
    String raw;
    serializeJson(result, raw);
    web.sendHeader("Cache-Control", "no-store");
    web.send(ok ? 200 : 409, "application/json", raw);
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
    QueueMeta meta;
    if (!readQueueMeta(meta)) {
      unlockStore();
      web.send(503, "application/json", "{}");
      return;
    }
    const unsigned head = meta.head, count = meta.count;
    DynamicJsonDocument d(6144), row(6144);
    queueMetaJson(d.to<JsonObject>(), meta);
    d["count"] = count;
    d["capacity"] = 16;
    if (count && head < 16) {
      String key = "q" + String(head);
      deserializeJson(row, store.getString(key.c_str(), ""));
      d["head_digest"] = digest(store.getString(key.c_str(), ""));
      d["head_path"] = row["path"];
      DynamicJsonDocument payload(6144);
      deserializeJson(payload, row["body"].as<String>());
      d["head_message_id"] = payload["message_id"];
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
    QueueMeta meta;
    if (!readQueueMeta(meta)) {
      unlockStore();
      web.send(503, "application/json", "{}");
      return;
    }
    const unsigned head = meta.head, count = meta.count;
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
  web.on("/api/outbox/cancel", HTTP_POST, [] {
    if (!administrator())
      return;
    if (web.header("X-CSRF-Token") != csrf) {
      web.send(403, "application/json", "{\"error\":\"csrf_invalid\"}");
      return;
    }
    if (controller.activeCount() || nodeLease.active() || restartPending ||
        xSemaphoreTake(outboxMutex, 0) != pdTRUE) {
      web.send(409, "application/json", "{\"error\":\"outbox_busy\"}");
      return;
    }
    DynamicJsonDocument request(1024);
    String body = web.arg("plain");
    bool valid = body.length() <= 768 && !deserializeJson(request, body) &&
                 request["expected_head"].is<const char *>() &&
                 request["reason"].is<const char *>();
    bool ok = false;
    String error = "cancel_request_invalid";
    if (valid && lockStore()) {
      QueueMeta meta;
      if (readQueueMeta(meta) && meta.count) {
        String key = "q" + String(meta.head),
               raw = store.getString(key.c_str(), "");
        if (digest(raw) == request["expected_head"].as<String>()) {
          DynamicJsonDocument row(6144), payload(6144);
          deserializeJson(row, raw);
          deserializeJson(payload, row["body"].as<String>());
          QueueCancellation entry;
          entry.digest = digest(raw).c_str();
          entry.reason = request["reason"].as<std::string>();
          entry.path = row["path"].as<std::string>();
          entry.messageId = payload["message_id"].as<std::string>();
          entry.recordedAt = clockReady() ? iso(time(nullptr)).c_str() : "";
          ok = meta.cancel(entry) && writeQueueMeta(meta);
          if (ok)
            store.remove(key.c_str());
          else
            error = "cancel_persistence_failed";
        } else
          error = "outbox_head_changed";
      } else
        error = "outbox_empty_or_invalid";
      unlockStore();
    }
    xSemaphoreGive(outboxMutex);
    DynamicJsonDocument response(384);
    response["cancelled"] = ok;
    if (!ok)
      response["error"] = error;
    String raw;
    serializeJson(response, raw);
    web.send(ok ? 200 : 409, "application/json", raw);
  });
  web.on("/api/events/replay", HTTP_POST, [] {
    if (!administrator())
      return;
    if (web.header("X-CSRF-Token") != csrf) {
      web.send(403, "application/json", "{\"error\":\"csrf_invalid\"}");
      return;
    }
    if (controller.activeCount() || nodeLease.active() ||
        provision.portalUrl.empty() || bootHold) {
      web.send(409, "application/json", "{\"error\":\"replay_unavailable\"}");
      return;
    }
    DynamicJsonDocument request(512);
    String raw = web.arg("plain"), error;
    bool ok = false;
    if (raw.length() <= 384 && !deserializeJson(request, raw) &&
        request["sequence"].is<const char *>() &&
        request["event_id"].is<const char *>()) {
      String text = request["sequence"];
      char *end = nullptr;
      errno = 0;
      uint64_t seq = strtoull(text.c_str(), &end, 10);
      bool valid = text.length() > 0 && text.length() <= 20;
      for (size_t i = 0; i < text.length(); ++i)
        valid = valid && isdigit(text[i]);
      if (valid && errno != ERANGE && end && !*end)
        ok = enqueueAuditSequence(seq, request["event_id"].as<String>(), error);
    }
    DynamicJsonDocument response(384);
    response["queued"] = ok;
    if (!ok)
      response["error"] = error.length() ? error : "replay_invalid";
    raw = "";
    serializeJson(response, raw);
    web.send(ok ? 200 : 409, "application/json", raw);
  });
  web.on("/api/events", HTTP_GET, [] {
    if (!authenticate())
      return;
    if (!lockStore()) {
      web.send(503, "application/json", "{}");
      return;
    }
    uint64_t before = 0;
    if (web.hasArg("before")) {
      String cursor = web.arg("before");
      bool valid = cursor.length() > 0 && cursor.length() <= 20;
      for (size_t i = 0; i < cursor.length(); ++i)
        valid = valid && isdigit(cursor[i]);
      errno = 0;
      char *end = nullptr;
      before = strtoull(cursor.c_str(), &end, 10);
      if (!valid || errno == ERANGE || !end || *end || !before) {
        unlockStore();
        web.send(400, "application/json", "{\"error\":\"invalid_cursor\"}");
        return;
      }
    }
    DynamicJsonDocument d(24576);
    JsonArray events = d.createNestedArray("events");
    const uint64_t last = store.getULong64("aseq", 0),
                   first = last > 63 ? last - 63 : 1;
    uint64_t seq = before && before <= last ? before - 1 : last;
    d["total_since_provisioning"] = last;
    d["retained_limit"] = 64;
    d["page_limit"] = 10;
    AuditReplay progress;
    const bool replayValid = readReplayState(progress);
    char cursor[21];
    snprintf(cursor, sizeof(cursor), "%llu",
             static_cast<unsigned long long>(progress.cursor));
    d["replay_cursor"] = cursor;
    d["replay_overwritten"] = progress.overwritten;
    d["replay_state_valid"] = replayValid;
    unsigned visited = 0;
    while (seq >= first && seq > 0 && visited < 10) {
      String key = "a" + String(uint32_t(seq % 64));
      DynamicJsonDocument row(2048);
      if (!deserializeJson(row, store.getString(key.c_str(), "")) &&
          row["sequence"].as<uint64_t>() == seq) {
        char text[21];
        snprintf(text, sizeof(text), "%llu",
                 static_cast<unsigned long long>(seq));
        row["sequence_cursor"] = text;
        events.add(row.as<JsonObjectConst>());
      }
      --seq;
      ++visited;
    }
    if (seq >= first && seq > 0) {
      char cursor[21];
      snprintf(cursor, sizeof(cursor), "%llu",
               static_cast<unsigned long long>(seq + 1));
      d["next_before"] = cursor;
    } else
      d["next_before"] = nullptr;
    unlockStore();
    if (d.overflowed()) {
      web.send(503, "application/json", "{\"error\":\"event_page_too_large\"}");
      return;
    }
    String raw;
    serializeJson(d, raw);
    web.send(200, "application/json", raw);
  });
  web.on("/api/sensors", HTTP_GET, [] {
    if (!administrator())
      return;
    DynamicJsonDocument d(4096);
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
    d["local_available"] = !IRRIGATION_ROLE_MATRIX;
    JsonArray calibration = d.createNestedArray("calibration");
    if (!lockStore()) {
      web.send(503, "application/json", "{}");
      return;
    }
    for (unsigned i = 0; i < 4; ++i) {
      JsonObject row = calibration.createNestedObject();
      row["slot"] = i;
      row["moisture_enabled"] =
          !IRRIGATION_ROLE_MATRIX && (hardware::moistureMask & (1u << i));
      row["temperature_enabled"] =
          !IRRIGATION_ROLE_MATRIX && hardware::temperatureEnabled;
      const String value = store.getString(("cal" + String(i)).c_str(), "");
      int dry = 0, wet = 0;
      const bool saved = sscanf(value.c_str(), "%d,%d", &dry, &wet) == 2;
      if (saved) {
        row["dry"] = dry;
        row["wet"] = wet;
      }
      row["rom"] = temperatureRom[i];
    }
    unlockStore();
    String s;
    serializeJson(d, s);
    web.send(200, "application/json", s);
  });
  web.on("/api/calibration", HTTP_POST, [] {
    if (!administrator())
      return;
    if (IRRIGATION_ROLE_MATRIX) {
      web.send(405, "application/json",
               "{\"error\":\"calibrate_at_sensor_node\"}");
      return;
    }
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
    if (IRRIGATION_REMOTE_NODE && i != 0) {
      web.send(400, "application/json", "{}");
      return;
    }
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
    if (IRRIGATION_REMOTE_NODE || strlen(provision.portalUrl.c_str())) {
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
  web.on("/api/configuration/draft", HTTP_POST, [] {
    if (!administrator())
      return;
    if (web.header("X-CSRF-Token") != csrf) {
      web.send(403, "application/json", "{\"error\":\"csrf_invalid\"}");
      return;
    }
    const String body = web.arg("plain");
    if (body.length() > bodyLimit - 256) {
      web.send(413, "application/json",
               "{\"error\":\"configuration_too_large\"}");
      return;
    }
    const char *reason = nullptr;
    { // Release the parsed draft before applyEnvelope allocates its document.
      DynamicJsonDocument draft(24576);
      if (deserializeJson(draft, body) ||
          !draft["expected_revision"].is<uint32_t>() ||
          !draft["configuration"].is<JsonObject>()) {
        web.send(400, "application/json", "{\"error\":\"draft_invalid\"}");
        return;
      }
      reason =
          draftRejection(2, true, strlen(provision.portalUrl.c_str()),
                         IRRIGATION_REMOTE_NODE, controller.activeCount(),
                         draft["expected_revision"].as<uint32_t>(), revision);
    }
    if (reason) {
      web.send(409, "application/json",
               String("{\"error\":\"") + reason + "\"}");
      return;
    }
    size_t begin = 0, end = 0;
    if (!objectMember(std::string(body.c_str(), body.length()), "configuration",
                      begin, end)) {
      web.send(400, "application/json", "{\"error\":\"draft_invalid\"}");
      return;
    }
    const String configuration = body.substring(begin, end),
                 hash = digest(configuration);
    if (configuration.length() > bodyLimit - 512 || hash.length() != 64) {
      web.send(400, "application/json",
               "{\"error\":\"configuration_too_large_or_hash_failed\"}");
      return;
    }
    const String envelope = String("{\"schema_version\":1,\"device_guid\":\"") +
                            guid + "\",\"revision\":" + String(revision + 1) +
                            ",\"configuration_hash\":\"" + hash +
                            "\",\"configuration\":" + configuration + "}";
    String error;
    const bool applied = applyEnvelope(envelope, error);
    DynamicJsonDocument result(512);
    result["applied"] = applied;
    result["revision"] = revision;
    if (!applied)
      result["error"] = error;
    String raw;
    serializeJson(result, raw);
    web.send(applied ? 200 : 400, "application/json", raw);
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
  ScopedMutex transaction(networkMutex);
  if (!transaction.held || recoveryHold)
    return -4;
  if (authBlocked.load())
    return -3;
  if (WiFi.status() != WL_CONNECTED ||
      !String(provision.portalUrl.c_str()).startsWith("https://") ||
      !strlen(provision.portalCa.c_str()) ||
      !strlen(provision.deviceToken.c_str()) || !clockReady())
    return -1;
  WiFiClientSecure tls;
  tls.setCACert(provision.portalCa.c_str());
  tls.setTimeout(3);
  HTTPClient http;
  http.setConnectTimeout(2000);
  http.setTimeout(3000);
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  if (!http.begin(tls, String(provision.portalUrl.c_str()) + path))
    return -1;
  http.addHeader("Authorization",
                 String("Bearer ") + provision.deviceToken.c_str());
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
bool enqueueAuditSequence(uint64_t seq, const String &expectedId,
                          String &error) {
  if (!lockStore()) {
    error = "storage_busy";
    return false;
  }
  const uint64_t last = store.getULong64("aseq", 0),
                 first = last > 63 ? last - 63 : 1;
  if (seq < first || seq > last || seq <= auditOriginFloor) {
    unlockStore();
    error = "audit_not_retained_or_prior_destination";
    return false;
  }
  DynamicJsonDocument event(2048);
  const String key = "a" + String(uint32_t(seq % 64));
  const bool valid =
      !deserializeJson(event, store.getString(key.c_str(), "")) &&
      event["sequence"].as<uint64_t>() == seq;
  QueueMeta queue;
  const bool room = readQueueMeta(queue) && queue.count < 15;
  unlockStore();
  if (!valid || !event["event_id"].is<const char *>() ||
      event["event_id"].as<String>() != expectedId ||
      !event["recorded_at"].is<const char *>() ||
      event["revision"].as<uint32_t>() == 0) {
    error = "audit_not_replayable";
    return false;
  }
  if (!room) {
    error = "outbox_full";
    return false;
  }
  DynamicJsonDocument doc(4096);
  doc["schema_version"] = 1;
  doc["message_id"] = uuid();
  doc["device_guid"] = guid;
  doc["boot_id"] = bootId;
  doc["sequence"] = ++sequence;
  doc["recorded_at"] = event["recorded_at"];
  doc["time_quality"] = "synchronized";
  doc["configuration_revision"] = event["revision"];
  doc["simulated"] = false;
  doc.createNestedArray("readings");
  JsonObject copy = doc.createNestedArray("events").createNestedObject();
  copy.set(event.as<JsonObjectConst>());
  if (doc.overflowed()) {
    error = "audit_too_large";
    return false;
  }
  String raw;
  serializeJson(doc, raw);
  if (!queueJob("/iot/v1/telemetry", raw)) {
    error = "audit_enqueue_failed";
    return false;
  }
  return true;
}
bool readReplayState(AuditReplay &state) {
  if (!store.isKey("replay_v1")) {
    state.cursor = store.getULong64("areplay", 0);
    state.overwritten = store.getULong64("audit_lost", 0);
    return true;
  }
  DynamicJsonDocument doc(384);
  if (deserializeJson(doc, store.getString("replay_v1", "")) ||
      doc["schema_version"] != 1 || !doc["cursor"].is<uint64_t>() ||
      !doc["overwritten"].is<uint64_t>())
    return false;
  state.cursor = doc["cursor"];
  state.overwritten = doc["overwritten"];
  return true;
}
bool writeReplayState(const AuditReplay &state) {
  DynamicJsonDocument doc(384);
  doc["schema_version"] = 1;
  doc["cursor"] = state.cursor;
  doc["overwritten"] = state.overwritten;
  String raw;
  serializeJson(doc, raw);
  return store.putString("replay_v1", raw) == raw.length();
}
void replayAudit() {
  static uint32_t at = 0;
  if (uint32_t(millis() - at) < 2000 || controller.activeCount() ||
      nodeLease.active() || provision.portalUrl.empty() || !revision ||
      !clockReady() || bootHold)
    return;
  at = millis();
  if (!lockStore())
    return;
  const uint64_t last = store.getULong64("aseq", 0);
  AuditReplay state;
  if (!readReplayState(state)) {
    unlockStore();
    return;
  }
  const uint64_t before = state.cursor;
  if (!state.fence(last, auditOriginFloor) ||
      (state.cursor != before && !writeReplayState(state))) {
    unlockStore();
    return;
  }
  if (state.cursor >= last) {
    unlockStore();
    return;
  }
  const uint64_t next = state.cursor + 1;
  DynamicJsonDocument row(2048);
  String key = "a" + String(uint32_t(next % 64));
  const bool eligible =
      !deserializeJson(row, store.getString(key.c_str(), "")) &&
      row["sequence"].as<uint64_t>() == next &&
      row["event_id"].is<const char *>() &&
      row["recorded_at"].is<const char *>() &&
      row["revision"].as<uint32_t>() > 0;
  const String id = row["event_id"].as<String>();
  unlockStore();
  String error;
  if (eligible && !enqueueAuditSequence(next, id, error))
    return;
  state.cursor = next;
  if (lockStore()) {
    writeReplayState(state);
    unlockStore();
  }
}

void networkTask(void *) {
  uint32_t configAt = millis() - 10000, commandsAt = millis() - 1000;
  uint32_t retryAt = 0;
  for (;;) {
    String response;
    if (xSemaphoreTake(outboxMutex, 0) == pdTRUE) {
      if (lockStore()) {
        QueueMeta meta;
        const bool valid = readQueueMeta(meta);
        const unsigned count = valid ? meta.count : 0,
                       head = valid ? meta.head : 0;
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
                saved =
                    store.putString(key.c_str(), updated) == updated.length();
              unlockStore();
            }
            if (!saved) {
              xSemaphoreGive(outboxMutex);
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
                  QueueMeta current;
                  if (readQueueMeta(current) && current.head == head &&
                      current.count && current.pop() && writeQueueMeta(current))
                    store.remove(key.c_str());
                }
                unlockStore();
              }
            }
          }
        }
      }
      xSemaphoreGive(outboxMutex);
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
          (!IOT_PHYSICAL_OUTPUTS || !(IOT_RELAY_MASK & 1) || !storageReady ||
           recoveryHold))
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
  Serial.setRxBufferSize(4096);
  Serial.begin(115200);
  security.required = IOT_REQUIRE_HARDENED_BOOT;
  security.flashEncrypted = esp_flash_encryption_enabled();
  security.secureBoot = esp_secure_boot_enabled();
#if defined(CONFIG_NVS_ENCRYPTION) && CONFIG_NVS_ENCRYPTION
  const esp_partition_t *keys = esp_partition_find_first(
      ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS_KEYS, nullptr);
  nvs_sec_cfg_t cfg{};
  security.nvsEncrypted = keys && keys->encrypted &&
                          nvs_flash_read_security_cfg(keys, &cfg) == ESP_OK;
  memset(&cfg, 0, sizeof(cfg));
#endif
  storeMutex = xSemaphoreCreateMutex();
  outboxMutex = xSemaphoreCreateMutex();
  networkMutex = xSemaphoreCreateMutex();
  incoming = xQueueCreate(4, sizeof(Input *));
#if !IRRIGATION_ROLE_MATRIX
  if (IOT_PHYSICAL_OUTPUTS)
    for (unsigned i = 0; i < 4; ++i)
      if (IOT_RELAY_MASK & (1u << i)) {
        digitalWrite(hardware::relayPins[i], IOT_RELAY_ACTIVE_LOW ? HIGH : LOW);
        pinMode(hardware::relayPins[i], OUTPUT);
      }
#endif
  storageReady = storeMutex && outboxMutex && networkMutex && incoming &&
                 store.begin("irrigation", false);
  if (!storageReady) {
    Serial.println("storage_fault: outputs blocked");
    return;
  }
  if (!security.allowed()) {
    storageReady = false;
    Serial.println("security_requirements_unmet: production boot blocked; no "
                   "eFuse changed");
    return;
  }
  if (store.isKey("occ_v2")) {
    const size_t length = store.getBytesLength("occ_v2");
    auto bytes = std::unique_ptr<uint8_t[]>(
        new (std::nothrow) uint8_t[OccurrenceLedger::maxBytes]);
    if (!bytes || length > OccurrenceLedger::maxBytes || !length ||
        store.getBytes("occ_v2", bytes.get(), length) != length ||
        !occurrences.decode(bytes.get(), length)) {
      storageReady = false;
      Serial.println("occurrence_journal_fault: preserve NVS; outputs blocked");
      return;
    }
    occurrencesReady = true;
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
  if (!applyStagedRecovery())
    recoveryFault = true;
  loadProvision();
  if (recoveryFault) {
    provisionFault = true;
    Serial.println("recovery_stage_fault: held, USB abort available");
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
  bootHold = !storageReady || provisionFault || recoveryHold;
  for (unsigned i = 0; i < config.count; ++i) {
    String key = "run" + String(i);
    if (store.getBool(key.c_str(), false))
      journal.event(i, "interrupted", "restart_interrupted", Source::Manual);
    outputs.off(i);
  }
  if (!provisionFault && strlen(provision.ssid.c_str())) {
    WiFi.persistent(false);
    WiFi.setAutoReconnect(true);
    WiFi.mode(WIFI_STA);
    WiFi.begin(provision.ssid.c_str(), provision.wifiPassword.c_str());
    esp_sntp_set_time_sync_notification_cb(
        [](struct timeval *) { clockSynchronized.store(true); });
    configTime(0, 0, "pool.ntp.org", "time.cloudflare.com");
    if (config.timezone.length())
      selectTimezone(config.timezone.c_str());
    if (strlen(provision.adminPassword.c_str()) >= 12)
      localPortal();
    if (!IRRIGATION_REMOTE_NODE && strlen(provision.portalUrl.c_str()) &&
        strlen(provision.deviceToken.c_str()))
      xTaskCreatePinnedToCore(networkTask, "portal-network", 12288, nullptr, 1,
                              nullptr, 0);
  }
  Serial.printf("firmware=0.6.0 guid=%s physical=%u\n", guid.c_str(),
                IOT_PHYSICAL_OUTPUTS);
  Serial.println(
      "USB provisioning: PROVISION? or PROVISION {expected_revision,settings}; "
      "window 180 seconds; no secret echo");
  previousEpoch = time(nullptr);
}
void loop() {
  if (!storageReady) {
    delay(10);
    return;
  }
  serialProvision();
  replayAudit();
  if (restartPending) {
    if (uint32_t(millis() - restartAt) >= 1500) {
      Serial.flush();
      ESP.restart();
    }
    delay(1);
    return;
  }
  if (provisionFault) {
    delay(1);
    return;
  }
  static bool wifiWasConnected = false;
  const bool wifiConnected = WiFi.status() == WL_CONNECTED;
  if (wifiConnected != wifiWasConnected) {
    wifiWasConnected = wifiConnected;
    if (wifiConnected)
      Serial.println(String("WiFi connected; portal http://") +
                     WiFi.localIP().toString() + "/");
    else
      Serial.println("WiFi disconnected; waiting for automatic reconnect");
  }
  uint32_t now = millis();
  if (IRRIGATION_REMOTE_NODE) {
    web.handleClient();
    if (restartPending)
      return;
    nodeLoop(millis());
    delay(1);
    return;
  }
  web.handleClient();
  if (restartPending)
    return;
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
    const bool observed =
        maintainOccurrences(epoch) && recoverSchedules(epoch, now);
    controller.clockValid(observed && !bootHold && outputs.allKnown());
    Window windows[4];
    static Window candidates[maxSlots];
    int selected[4] = {-1, -1, -1, -1};
    for (size_t i = 0; i < config.slotCount; ++i) {
      const auto &s = config.slots[i];
      auto w =
          observed ? evaluate(s, epoch, previousEpoch, revision) : Window{};
      w.retired = w.open && occurrences.retired(w.begins);
      candidates[i] = w;
    }
    selectScheduleWindows(config.slots, config.slotCount, candidates,
                          controller, config.count, runScheduleSlot, now,
                          windows, selected);
    for (size_t zone = 0; zone < config.count; ++zone) {
      plannedScheduleSlot[zone] = selected[zone];
      plannedScheduleWindow[zone] = windows[zone];
    }
    for (size_t i = 0; i < config.slotCount; ++i) {
      const auto &s = config.slots[i];
      const auto &w = candidates[i];
      if (!config.zones[s.zone].enabled ||
          config.zones[s.zone].mode == Mode::Dynamic ||
          occurrences.contains(s.zone, w.occurrence) ||
          (controller.run(s.zone).active &&
           !runBlocksSchedule(controller.run(s.zone), now)))
        continue;
      if (scheduleConflict(w, windows[s.zone], controller.run(s.zone).active)) {
        if (!journal.consumeWindow(s.zone, w.occurrence, w.begins) ||
            !scheduleObservation(i, w, "omitted",
                                 controller.run(s.zone).active
                                     ? "active_run_conflict"
                                     : "schedule_overlap",
                                 2, selected[s.zone]))
          controller.clockValid(false);
      }
    }
    if (bootHold)
      controller.clockValid(false);
    controller.tick(now, windows);
    if (observed)
      previousEpoch = epoch;
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

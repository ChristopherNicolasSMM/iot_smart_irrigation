#pragma once
#include "Controller.h"
#include "Schedule.h"
#include <Arduino.h>
#include <ArduinoJson.h>
namespace irrigation {
constexpr size_t maxSlots = 64;
struct FirmwareConfig {
  Zone zones[4];
  String zoneIds[4], actuatorIds[4], moistureIds[4], temperatureIds[4];
  int8_t actuatorSlot[4] = {-1, -1, -1, -1}, moistureSlot[4] = {-1, -1, -1, -1},
         temperatureSlot[4] = {-1, -1, -1, -1};
  bool moistureEnabled[4] = {}, temperatureEnabled[4] = {};
  uint8_t count = 0, moistureCount = 0, temperatureCount = 0, capacity = 1;
  Schedule slots[maxSlots];
  size_t slotCount = 0;
  String timezone;
};
inline uint64_t hashKey(const String &s) {
  uint64_t n = 1469598103934665603ULL;
  for (size_t i = 0; i < s.length(); ++i) {
    n ^= uint8_t(s[i]);
    n *= 1099511628211ULL;
  }
  return n;
}
inline bool validId(const String &id) {
  if (!id.length() || id.length() > 64 || id == "__proto__" ||
      id == "constructor" || id == "prototype")
    return false;
  for (size_t i = 0; i < id.length(); ++i) {
    char c = id[i];
    if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
          (c >= '0' && c <= '9') || c == '_' || c == '-'))
      return false;
  }
  return true;
}
inline bool parseConfiguration(JsonObjectConst c, FirmwareConfig &out,
                               String &error) {
  auto fail = [&](const char *s) {
    error = s;
    return false;
  };
  if (!c["components"].is<JsonArrayConst>() ||
      !c["zones"].is<JsonArrayConst>() || !c["schedules"].is<JsonArrayConst>())
    return fail("configuration_incomplete");
  if (!c["name"].is<const char *>() || !c["name"].as<String>().length() ||
      c["name"].as<String>().length() > 100 || !c["area"].is<const char *>() ||
      c["area"].as<String>().length() > 100)
    return fail("name_area_invalid");
  out = FirmwareConfig{};
  out.timezone = c["timezone"].as<String>();
  // No IANA database in this first embedded profile: explicitly reject
  // unsupported TZ.
  if (out.timezone != "America/Sao_Paulo" && out.timezone != "UTC" &&
      out.timezone != "Etc/UTC")
    return fail("timezone_unsupported");
  if (!c["max_simultaneous"].is<unsigned>() ||
      c["max_simultaneous"].as<unsigned>() < 1 ||
      c["max_simultaneous"].as<unsigned>() > 4)
    return fail("capacity_invalid");
  out.capacity = c["max_simultaneous"].as<unsigned>();
  String allIds[12];
  uint8_t n = 0, actuatorCount = 0;
  for (JsonObjectConst comp : c["components"].as<JsonArrayConst>()) {
    String id = comp["id"].as<String>(), kind = comp["kind"].as<String>();
    if (n == 12 || !validId(id) || !comp["enabled"].is<bool>())
      return fail("component_invalid");
    for (unsigned i = 0; i < n; ++i)
      if (allIds[i] == id)
        return fail("component_duplicate");
    allIds[n++] = id;
    if (kind == "soil_moisture") {
      if (out.moistureCount == 4)
        return fail("moisture_limit");
      auto i = out.moistureCount++;
      out.moistureIds[i] = id;
      out.moistureEnabled[i] = comp["enabled"];
    } else if (kind == "soil_temperature") {
      if (out.temperatureCount == 4)
        return fail("temperature_limit");
      auto i = out.temperatureCount++;
      out.temperatureIds[i] = id;
      out.temperatureEnabled[i] = comp["enabled"];
    } else if (kind == "irrigation_actuator") {
      if (++actuatorCount > 4)
        return fail("actuator_limit");
    } else
      return fail("component_kind_invalid");
  }
  for (JsonObjectConst zone : c["zones"].as<JsonArrayConst>()) {
    if (out.count == 4)
      return fail("zone_limit");
    unsigned i = out.count++;
    String id = zone["id"].as<String>(), act = zone["actuator_id"].as<String>();
    if (!validId(id))
      return fail("zone_id_invalid");
    for (unsigned j = 0; j < i; ++j)
      if (out.zoneIds[j] == id || out.actuatorIds[j] == act)
        return fail("zone_duplicate");
    bool found = false;
    for (JsonObjectConst comp : c["components"].as<JsonArrayConst>())
      if (comp["id"].as<String>() == act &&
          comp["kind"] == "irrigation_actuator") {
        found = true;
        out.zones[i].enabled = comp["enabled"];
      }
    if (!found)
      return fail("actuator_missing");
    out.zoneIds[i] = id;
    out.actuatorIds[i] = act;
    String mode = zone["mode"].as<String>();
    if (mode == "static")
      out.zones[i].mode = Mode::Static;
    else if (mode == "dynamic")
      out.zones[i].mode = Mode::Dynamic;
    else if (mode == "hybrid")
      out.zones[i].mode = Mode::Hybrid;
    else
      return fail("mode_invalid");
    if (!zone["max_run_s"].is<unsigned>() ||
        zone["max_run_s"].as<unsigned>() < 1 ||
        zone["max_run_s"].as<unsigned>() > 86400 ||
        !zone["min_pause_s"].is<unsigned>() ||
        zone["min_pause_s"].as<unsigned>() > 86400)
      return fail("duration_invalid");
    out.zones[i].maxMs = zone["max_run_s"].as<unsigned>() * 1000;
    out.zones[i].pauseMs = zone["min_pause_s"].as<unsigned>() * 1000;
    String sensor = zone["sensor_id"].as<String>();
    for (unsigned j = 0; j < out.moistureCount; ++j)
      if (out.moistureIds[j] == sensor)
        out.zones[i].sensor = j;
    if (sensor.length() && out.zones[i].sensor < 0)
      return fail("sensor_missing");
    if (mode != "static") {
      if (out.zones[i].sensor < 0 || !zone["start_below"].is<float>() ||
          !zone["target"].is<float>())
        return fail("moisture_required");
      out.zones[i].below = zone["start_below"];
      out.zones[i].target = zone["target"];
      if (!isfinite(out.zones[i].below) || !isfinite(out.zones[i].target) ||
          out.zones[i].below < 0 || out.zones[i].target > 100 ||
          out.zones[i].below >= out.zones[i].target)
        return fail("threshold_invalid");
    }
    String temp = zone["temperature_sensor_id"].as<String>();
    if (temp.length()) {
      bool exists = false;
      for (unsigned j = 0; j < out.temperatureCount; ++j)
        exists |= out.temperatureIds[j] == temp;
      if (!exists)
        return fail("temperature_missing");
    }
  }
  if (c["schedules"].size() > 32)
    return fail("schedule_limit");
  String scheduleIds[32];
  size_t ns = 0;
  for (JsonObjectConst agenda : c["schedules"].as<JsonArrayConst>()) {
    String id = agenda["id"].as<String>();
    if (!validId(id) || !agenda["enabled"].is<bool>())
      return fail("schedule_invalid");
    for (size_t j = 0; j < ns; ++j)
      if (scheduleIds[j] == id)
        return fail("schedule_duplicate");
    scheduleIds[ns++] = id;
    int zi = -1;
    for (unsigned j = 0; j < out.count; ++j)
      if (out.zoneIds[j] == agenda["zone_id"].as<String>())
        zi = j;
    if (zi < 0)
      return fail("schedule_zone_missing");
    String calendar = agenda["calendar"].as<String>();
    if (calendar != "weekly" && calendar != "date")
      return fail("calendar_invalid");
    int32_t date = 0;
    if (calendar == "date") {
      String ds = agenda["date"].as<String>();
      int y, m, d;
      char tail;
      if (ds.length() != 10 ||
          sscanf(ds.c_str(), "%4d-%2d-%2d%c", &y, &m, &d, &tail) != 3 ||
          y < 2020 || y > 2037 || m < 1 || m > 12 || d < 1 || d > 31)
        return fail("date_invalid");
      tm dt{};
      dt.tm_year = y - 1900;
      dt.tm_mon = m - 1;
      dt.tm_mday = d;
      dt.tm_hour = 12;
      dt.tm_isdst = -1;
      mktime(&dt);
      if (dt.tm_year != y - 1900 || dt.tm_mon != m - 1 || dt.tm_mday != d)
        return fail("date_invalid");
      date = y * 10000 + m * 100 + d;
    }
    auto slot = [&](JsonObjectConst t, int weekday, const String &key) -> bool {
      if (out.slotCount == maxSlots)
        return fail("expanded_schedule_limit_64");
      Schedule s;
      s.enabled = agenda["enabled"];
      s.zone = zi;
      s.weekday = weekday;
      s.date = date;
      s.id = hashKey(key);
      if (!validTime(t["start_time"] | "", s.startSecond))
        return fail("time_invalid");
      String termination = t["termination"].as<String>();
      if (termination == "duration") {
        if (!t["duration_s"].is<unsigned>() ||
            t["duration_s"].as<unsigned>() < 1 ||
            t["duration_s"].as<unsigned>() > 86400 || !t["end_time"].isNull())
          return fail("schedule_duration_invalid");
        s.durationSecond = t["duration_s"];
      } else if (termination == "end_time") {
        uint32_t end;
        if (!validTime(t["end_time"] | "", end) || !t["duration_s"].isNull() ||
            end == s.startSecond)
          return fail("end_time_invalid");
        bool cross = t["crosses_midnight"] | false;
        if ((!cross && end < s.startSecond) || (cross && end > s.startSecond))
          return fail("midnight_invalid");
        s.durationSecond = end + (cross ? 86400 : 0) - s.startSecond;
      } else
        return fail("termination_invalid");
      out.slots[out.slotCount++] = s;
      return true;
    };
    if (calendar == "date") {
      if (!slot(agenda, 0, id))
        return false;
    } else if (!agenda["day_times"].isNull()) {
      bool days[8] = {};
      JsonArrayConst dayTimes = agenda["day_times"];
      if (!dayTimes || !dayTimes.size() || dayTimes.size() > 7)
        return fail("days_invalid");
      for (JsonObjectConst day : dayTimes) {
        int wd = day["weekday"] | 0;
        if (wd < 1 || wd > 7 || days[wd])
          return fail("weekday_invalid");
        days[wd] = true;
        JsonArrayConst times = day["times"];
        if (!times || !times.size() || times.size() > 8)
          return fail("times_invalid");
        String tids[8];
        size_t nt = 0;
        for (JsonObjectConst t : times) {
          String tid = t["id"].as<String>();
          if (!validId(tid))
            return fail("slot_id_invalid");
          for (size_t j = 0; j < nt; ++j)
            if (tids[j] == tid)
              return fail("slot_duplicate");
          tids[nt++] = tid;
          if (!slot(t, wd, id + ":" + String(wd) + ":" + tid))
            return false;
        }
      }
    } else {
      JsonArrayConst weekdays = agenda["weekdays"];
      if (!weekdays || !weekdays.size() || weekdays.size() > 7)
        return fail("days_missing");
      bool days[8] = {};
      for (JsonVariantConst day : weekdays) {
        int wd = day.as<int>();
        if (wd < 1 || wd > 7 || days[wd])
          return fail("weekday_invalid");
        days[wd] = true;
        if (!slot(agenda, wd, id + ":" + String(wd)))
          return false;
      }
    }
  }
  return true;
}
} // namespace irrigation

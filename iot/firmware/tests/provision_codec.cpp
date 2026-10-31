#include <string>
using String = std::string;
#include "ProvisionCodec.h"
#include <cassert>
using namespace irrigation;
int main() {
  ProvisionSettings p;
  p.ssid = "Horta";
  p.adminPassword = "synthetic-admin";
  p.wifiPassword = "synthetic-wifi";
  p.apSsid = "Horta-config";
  p.apPassword = "synthetic-ap-key";
  p.actuators[0] = "valve_1";
  DynamicJsonDocument doc(24576);
  provisionJson(doc.to<JsonObject>(), p, true);
  std::string raw;
  serializeJson(doc, raw);
  assert(raw.find("synthetic-admin") != std::string::npos);
  ProvisionSettings saved;
  String error;
  assert(provisionPatch(doc.as<JsonObjectConst>(), saved, error));
  assert(saved.adminPassword == p.adminPassword);
  assert(saved.actuators == p.actuators);
  assert(saved.apSsid == p.apSsid && saved.apPassword == p.apPassword);
  doc.clear();
  provisionJson(doc.to<JsonObject>(), p, false);
  raw.clear();
  serializeJson(doc, raw);
  assert(raw.find("synthetic-admin") == std::string::npos);
  assert(raw.find("synthetic-wifi") == std::string::npos);
  assert(raw.find("synthetic-ap-key") == std::string::npos);
  assert(!doc.containsKey("ap_password"));
  assert(!doc.containsKey("wifi_password"));
  assert(doc["has_wifi_password"] == true);
  doc.clear();
  deserializeJson(doc, "{\"ssid\":\"Nova rede\"}");
  assert(provisionPatch(doc.as<JsonObjectConst>(), p, error));
  assert(p.adminPassword == "synthetic-admin");
  assert(p.ssid == "Nova rede");
  for (const char *bad :
       {"{\"guid\":\"overwrite\"}", "{\"physical_outputs\":true}",
        "{\"admin_password\":null}", "{\"ssid\":\"a\\u0000b\"}",
        "{\"bindings\":{\"actuators\":[\"valve_1\"]}}",
        "{\"bindings\":{\"other\":[]}}"}) {
    assert(!deserializeJson(doc, bad));
    auto candidate = p;
    assert(!provisionPatch(doc.as<JsonObjectConst>(), candidate, error));
  }
  doc.clear();
  deserializeJson(doc, "{\"operator_password\":\"\"}");
  assert(provisionPatch(doc.as<JsonObjectConst>(), p, error));
  assert(p.operatorPassword.empty());
  doc.clear();
  deserializeJson(
      doc,
      "{\"ssid\":\"\",\"wifi_password\":\"\",\"ap_ssid\":\"Nova-rede-ESP32\"}");
  assert(provisionPatch(doc.as<JsonObjectConst>(), p, error));
  assert(p.ssid.empty() && p.apSsid == "Nova-rede-ESP32" &&
         p.apPassword == "synthetic-ap-key");
  doc.clear();
  deserializeJson(
      doc, "{\"ssid\":\"Legacy\",\"admin_password\":\"synthetic-admin\"}");
  ProvisionSettings legacy;
  assert(provisionPatch(doc.as<JsonObjectConst>(), legacy, error));
  assert(legacy.apSsid.empty() && legacy.apPassword.empty());
}

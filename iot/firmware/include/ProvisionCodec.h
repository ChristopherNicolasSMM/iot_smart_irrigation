#pragma once
#include "ProvisionSettings.h"
#include <ArduinoJson.h>
#include <utility>
namespace irrigation {
inline void provisionJson(JsonObject obj, const ProvisionSettings &p,
                          bool secrets) {
  obj["ssid"] = p.ssid.c_str();
  obj["portal_url"] = p.portalUrl.c_str();
  if (secrets) {
    obj["wifi_password"] = p.wifiPassword.c_str();
    obj["admin_password"] = p.adminPassword.c_str();
    obj["operator_password"] = p.operatorPassword.c_str();
    obj["device_token"] = p.deviceToken.c_str();
    obj["portal_ca"] = p.portalCa.c_str();
  } else {
    obj["has_wifi_password"] = !p.wifiPassword.empty();
    obj["has_operator_password"] = !p.operatorPassword.empty();
    obj["has_device_token"] = !p.deviceToken.empty();
    obj["has_portal_ca"] = !p.portalCa.empty();
  }
  JsonObject bindings = obj.createNestedObject("bindings");
  for (auto pair : {std::make_pair("actuators", &p.actuators),
                    std::make_pair("moisture", &p.moisture),
                    std::make_pair("temperature", &p.temperature)}) {
    JsonArray array = bindings.createNestedArray(pair.first);
    for (const auto &id : *pair.second)
      array.add(id.c_str());
  }
}
inline bool provisionPatch(JsonObjectConst obj, ProvisionSettings &p,
                           String &error) {
  for (JsonPairConst pair : obj) {
    const std::string key = pair.key().c_str();
    std::string *field = nullptr;
    if (key == "ssid")
      field = &p.ssid;
    else if (key == "wifi_password")
      field = &p.wifiPassword;
    else if (key == "admin_password")
      field = &p.adminPassword;
    else if (key == "operator_password")
      field = &p.operatorPassword;
    else if (key == "portal_url")
      field = &p.portalUrl;
    else if (key == "device_token")
      field = &p.deviceToken;
    else if (key == "portal_ca")
      field = &p.portalCa;
    else if (key == "bindings") {
      if (!pair.value().is<JsonObjectConst>()) {
        error = "bindings_invalid";
        return false;
      }
      for (JsonPairConst binding : pair.value().as<JsonObjectConst>()) {
        const std::string kind = binding.key().c_str();
        auto *target = kind == "actuators"     ? &p.actuators
                       : kind == "moisture"    ? &p.moisture
                       : kind == "temperature" ? &p.temperature
                                               : nullptr;
        if (!target || !binding.value().is<JsonArrayConst>() ||
            binding.value().size() != 4) {
          error = "bindings_invalid";
          return false;
        }
        for (size_t i = 0; i < 4; ++i) {
          auto value = binding.value()[i];
          if (!value.is<const char *>()) {
            error = "binding_invalid";
            return false;
          }
          JsonString str = value.as<JsonString>();
          (*target)[i].assign(str.c_str(), str.size());
        }
      }
      continue;
    } else {
      error = "provision_field_unknown";
      return false;
    }
    if (!pair.value().is<const char *>()) {
      error = "provision_field_invalid";
      return false;
    }
    JsonString str = pair.value().as<JsonString>();
    field->assign(str.c_str(), str.size());
  }
  const char *reason = validateProvision(p);
  if (reason) {
    error = reason;
    return false;
  }
  return true;
}
} // namespace irrigation

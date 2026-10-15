#pragma once
#include <array>
#include <cstdint>
#include <string>
namespace irrigation {
struct ProvisionSettings {
  std::string ssid, wifiPassword, adminPassword, operatorPassword;
  std::string portalUrl, deviceToken, portalCa;
  std::array<std::string, 4> actuators{}, moisture{}, temperature{};
};
inline bool provisionText(const std::string &s, size_t maximum,
                          bool multiline = false) {
  if (s.size() > maximum)
    return false;
  for (unsigned char c : s)
    if (c == 0 || c == 127 ||
        (c < 32 && !(multiline && (c == '\n' || c == '\r'))))
      return false;
  return true;
}
inline bool provisionId(const std::string &s) {
  if (s.empty())
    return true;
  if (s.size() > 64 || s == "__proto__" || s == "constructor" ||
      s == "prototype")
    return false;
  for (char c : s)
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '_' || c == '-'))
      return false;
  return true;
}
inline bool provisionOrigin(const std::string &url) {
  if (url.compare(0, 8, "https://") || url.size() > 240)
    return false;
  const auto authority = url.substr(8);
  if (authority.empty())
    return false;
  auto colon = authority.find(':');
  const auto host = authority.substr(0, colon);
  if (host.empty() || host.front() == '.' || host.back() == '.' ||
      host.front() == '-' || host.back() == '-')
    return false;
  for (char c : host)
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '.' || c == '-'))
      return false;
  if (colon != std::string::npos) {
    auto port = authority.substr(colon + 1);
    if (port.empty() || port.size() > 5)
      return false;
    unsigned value = 0;
    for (char c : port) {
      if (c < '0' || c > '9')
        return false;
      value = value * 10 + unsigned(c - '0');
    }
    if (!value || value > 65535)
      return false;
  }
  return true;
}
inline const char *validateProvision(const ProvisionSettings &p) {
  if (p.ssid.empty() || !provisionText(p.ssid, 32))
    return "wifi_ssid_invalid";
  if (!provisionText(p.wifiPassword, 64))
    return "wifi_password_invalid";
  const auto n = p.wifiPassword.size();
  if (n && (n < 8 || n > 63)) {
    if (n != 64)
      return "wifi_password_invalid";
    for (char c : p.wifiPassword)
      if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
            (c >= 'A' && c <= 'F')))
        return "wifi_password_invalid";
  }
  if (p.adminPassword.size() < 12 || !provisionText(p.adminPassword, 128))
    return "admin_password_invalid";
  if (!p.operatorPassword.empty() && (p.operatorPassword.size() < 12 ||
                                      !provisionText(p.operatorPassword, 128) ||
                                      p.operatorPassword == p.adminPassword))
    return "operator_password_invalid";
  if (!provisionText(p.deviceToken, 512) ||
      !provisionText(p.portalCa, 8192, true))
    return "integration_secret_invalid";
  if (p.portalUrl.empty()) {
    if (!p.deviceToken.empty() || !p.portalCa.empty())
      return "disabled_integration_has_secrets";
  } else if (!provisionOrigin(p.portalUrl) || p.deviceToken.empty() ||
             p.portalCa.find("-----BEGIN CERTIFICATE-----") ==
                 std::string::npos ||
             p.portalCa.find("-----END CERTIFICATE-----") == std::string::npos)
    return "https_integration_invalid";
  for (const auto *bindings : {&p.actuators, &p.moisture, &p.temperature})
    for (size_t i = 0; i < 4; ++i) {
      if (!provisionId((*bindings)[i]))
        return "binding_invalid";
      for (size_t j = 0; j < i; ++j)
        if (!(*bindings)[i].empty() && (*bindings)[i] == (*bindings)[j])
          return "binding_duplicate";
    }
  return nullptr;
}
inline const char *provisionWriteRejection(bool authorized, bool idle,
                                           bool pending, uint32_t expected,
                                           uint32_t current,
                                           bool destinationChanged,
                                           bool queueEmpty) {
  if (!authorized)
    return "provision_authorization_required";
  if (!idle)
    return "controller_not_idle";
  if (pending)
    return "restart_pending";
  if (expected != current)
    return "provision_revision_conflict";
  if (current == UINT32_MAX)
    return "provision_revision_exhausted";
  if (destinationChanged && !queueEmpty)
    return "outbox_destination_locked";
  return nullptr;
}
} // namespace irrigation

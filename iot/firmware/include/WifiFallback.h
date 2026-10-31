#pragma once
#include <stdint.h>
#include <string>
namespace irrigation {
// Evaluated in the Wi-Fi task; no wall clock or blocking driver calls here.
struct WifiFallback {
  static constexpr uint32_t offlineDelay = 30000, stableDelay = 30000;
  uint32_t changedAt = 0;
  bool initialized = false, connectedBefore = false, keepAP = false;
  bool update(uint32_t now, bool configured, bool connected, unsigned clients) {
    if (!configured) {
      initialized = false;
      keepAP = true;
      return true;
    }
    if (!initialized || connected != connectedBefore) {
      initialized = true;
      connectedBefore = connected;
      changedAt = now;
    }
    if (!connected && uint32_t(now - changedAt) >= offlineDelay)
      keepAP = true;
    if (connected && uint32_t(now - changedAt) >= stableDelay && !clients)
      keepAP = false;
    return keepAP;
  }
};
inline bool bootstrapRequestAllowed(bool bootstrap, bool apInterface,
                                    const std::string &uri) {
  return !bootstrap || (apInterface && (uri == "/" || uri == "/api/state" ||
                                        uri == "/api/provisioning"));
}
inline bool needsInitialPortal(bool hardened, bool storedProfile,
                               bool recoveryFault, bool missingAdmin) {
  return !hardened && !storedProfile && !recoveryFault && missingAdmin;
}
} // namespace irrigation

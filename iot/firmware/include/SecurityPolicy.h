#pragma once
namespace irrigation {
struct SecurityState {
  bool required = false, flashEncrypted = false, secureBoot = false,
       nvsEncrypted = false;
  bool ready() const { return flashEncrypted && secureBoot && nvsEncrypted; }
  bool allowed() const { return !required || ready(); }
  bool usbRecoveryAllowed() const { return !required; }
};
} // namespace irrigation

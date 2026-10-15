#include "ProvisionSettings.h"
#include <cassert>
#include <cstring>
using namespace irrigation;
int main() {
  ProvisionSettings p;
  p.ssid = "Horta";
  p.adminPassword = "synthetic-admin";
  assert(!validateProvision(p)); // Open Wi-Fi supported, operator disabled.
  p.wifiPassword = "short";
  assert(validateProvision(p));
  p.wifiPassword = "12345678";
  p.operatorPassword = p.adminPassword;
  assert(validateProvision(p));
  p.operatorPassword = "synthetic-operator";
  assert(!validateProvision(p));
  p.actuators[0] = "valve_1";
  p.actuators[1] = "valve_1";
  assert(validateProvision(p));
  p.actuators[1] = "valve_2";
  assert(!validateProvision(p));
  p.moisture[0] = "__proto__";
  assert(validateProvision(p));
  p.moisture[0] = "soil_1";
  p.portalUrl = "https://portal.invalid:8443";
  assert(validateProvision(p));
  p.deviceToken = "synthetic-token";
  p.portalCa =
      "-----BEGIN CERTIFICATE-----\nsynthetic\n-----END CERTIFICATE-----";
  assert(!validateProvision(p));
  for (const char *url :
       {"http://portal.invalid", "https://user@portal.invalid",
        "https://portal.invalid/path", "https://portal.invalid?x",
        "https://portal.invalid:0", "https://portal.invalid:65536",
        "https://portal.invalid\r\nHost:bad", "https://"})
    assert(!provisionOrigin(url));
  assert(provisionOrigin("https://192.168.1.2:443"));
  p.ssid = std::string("ok\0bad", 6);
  assert(validateProvision(p));
  p.ssid = "Horta";
  p.deviceToken = "bad\nheader";
  assert(validateProvision(p));
  assert(!provisionWriteRejection(true, true, false, 0, 0, false, false));
  assert(!strcmp(provisionWriteRejection(false, true, false, 0, 0, false, true),
                 "provision_authorization_required"));
  assert(!strcmp(provisionWriteRejection(true, false, false, 0, 0, false, true),
                 "controller_not_idle"));
  assert(!strcmp(provisionWriteRejection(true, true, true, 0, 0, false, true),
                 "restart_pending"));
  assert(!strcmp(provisionWriteRejection(true, true, false, 1, 2, false, true),
                 "provision_revision_conflict"));
  assert(!strcmp(provisionWriteRejection(true, true, false, UINT32_MAX,
                                         UINT32_MAX, false, true),
                 "provision_revision_exhausted"));
  assert(!strcmp(provisionWriteRejection(true, true, false, 1, 1, true, false),
                 "outbox_destination_locked"));
  assert(!provisionWriteRejection(true, true, false, 1, 1, true, true));
}

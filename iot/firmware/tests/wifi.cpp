#include "WifiFallback.h"
#include <cassert>
using namespace irrigation;
int main() {
  WifiFallback p;
  assert(p.update(0, false, false, 0));
  assert(
      p.update(100, true, false, 0)); // previous AP retained through reconnect
  assert(p.update(200, true, true, 1));
  assert(p.update(30200, true, true, 1)); // don't evict a portal user
  assert(!p.update(30201, true, true, 0));
  assert(!p.update(40000, true, false, 0));
  assert(!p.update(69999, true, false, 0));
  assert(p.update(70000, true, false, 0));
  assert(p.update(71000, true, true, 0));
  assert(p.update(72000, true, false, 0)); // flap doesn't shut down AP
  assert(p.update(73000, true, true, 0));
  assert(!p.update(103000, true, true, 0));
  WifiFallback fresh;
  assert(!fresh.update(0, true, false, 0));
  assert(!fresh.update(29999, true, false, 0));
  assert(fresh.update(30000, true, false, 0));
  WifiFallback wrap;
  assert(!wrap.update(UINT32_MAX - 10000, true, false, 0));
  assert(!wrap.update(10000, true, false, 0));
  assert(wrap.update(20000, true, false, 0));
  assert(bootstrapRequestAllowed(true, true, "/api/provisioning"));
  assert(!bootstrapRequestAllowed(true, false, "/api/provisioning"));
  assert(!bootstrapRequestAllowed(true, true, "/api/command"));
  assert(!bootstrapRequestAllowed(true, true, "/api/configuration/draft"));
  assert(bootstrapRequestAllowed(false, false, "/api/command"));
  for (const char *route : {"/", "/api/state", "/api/provisioning", "/api/configuration",
       "/api/timezones", "/api/sensors", "/api/events", "/api/outbox"}) {
    assert(bootstrapRequestAllowed(true, true, route, true, false));
    assert(!bootstrapRequestAllowed(true, false, route, true, false));
    if (std::string(route) != "/api/provisioning")
      assert(!bootstrapRequestAllowed(true, true, route, false, true));
  }
  assert(bootstrapRequestAllowed(true, true, "/api/provisioning", false, true));
  assert(!bootstrapRequestAllowed(true, true, "/api/clock", false, true));
  assert(!bootstrapRequestAllowed(true, true, "/api/calibration", false, true));
  assert(!bootstrapRequestAllowed(true, true, "/api/outbox/retry", false, true));
  assert(!bootstrapRequestAllowed(true, true, "/api/events/replay", false, true));
  assert(needsInitialPortal(false, false, false, true));
  assert(!needsInitialPortal(false, true, false, true));
  assert(!needsInitialPortal(true, false, false, true));
  assert(!needsInitialPortal(false, false, true, true));
  assert(!needsInitialPortal(false, false, false, false));
}

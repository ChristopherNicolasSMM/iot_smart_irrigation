#include "RecoveryPlan.h"
#include "SecurityPolicy.h"
#include <cassert>
#include <map>
using namespace irrigation;
struct Store {
  std::map<std::string, std::string> data{{"guid", "same-guid"},
                                          {"queue_v2", "unchanged"},
                                          {"occ_v2", "unchanged"},
                                          {"restore_v1", "pending"}};
  unsigned fail = 999, calls = 0;
  bool allow() { return calls++ != fail; }
  std::string guid() { return data["guid"]; }
  bool hold() {
    if (!allow())
      return false;
    data["recover_hold"] = "true";
    return true;
  }
  bool text(const std::string &k, const std::string &v) {
    if (!allow())
      return false;
    data[k] = v;
    return true;
  }
  bool removeStage() {
    if (!allow())
      return false;
    data.erase("restore_v1");
    return true;
  }
};
int main() {
  SecurityState state;
  assert(state.allowed() && state.usbRecoveryAllowed());
  state.required = true;
  assert(!state.allowed() && !state.usbRecoveryAllowed());
  state.flashEncrypted = true;
  state.secureBoot = true;
  assert(!state.allowed());
  state.nvsEncrypted = true;
  assert(state.allowed());
  RecoveryPlan p;
  p.guid = "same-guid";
  p.profile = "new-profile";
  p.configuration = "new-config";
  p.tokenHash = "old-token-hash";
  for (unsigned failure = 0; failure < 13; ++failure) {
    Store s;
    s.fail = failure;
    assert(!applyRecoveryPlan(p, s));
    assert(s.data.count("restore_v1"));
    assert(s.data["queue_v2"] == "unchanged" &&
           s.data["occ_v2"] == "unchanged");
    s.fail = 999;
    s.calls = 0;
    assert(applyRecoveryPlan(p, s));
    assert(s.data["guid"] == p.guid && s.data["recover_hold"] == "true" &&
           !s.data.count("restore_v1"));
  }
  Store foreign;
  p.guid = "other";
  assert(!applyRecoveryPlan(p, foreign));
  assert(foreign.calls == 0);
  assert(recoveryRejection("a", "b", true, 1, 1, false, false, true));
  assert(recoveryRejection("a", "a", false, 1, 1, false, false, true));
  assert(recoveryRejection("a", "a", true, 2, 1, false, false, true));
  assert(recoveryRejection("a", "a", true, 1, 1, true, false, true));
  assert(recoveryRejection("a", "a", true, 1, 2, false, true, false));
  assert(!recoveryRejection("a", "a", true, 1, 2, false, true, true));
}

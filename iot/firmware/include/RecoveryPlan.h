#pragma once
#include <array>
#include <stdint.h>
#include <string>
namespace irrigation {
struct RecoveryPlan {
  std::string guid, profile, configuration, tokenHash;
  std::array<std::string, 4> calibration{}, roms{};
};
inline const char *recoveryRejection(const std::string &currentGuid,
                                     const std::string &snapshotGuid, bool idle,
                                     uint32_t currentRevision,
                                     uint32_t snapshotRevision,
                                     bool sameRevisionChanged,
                                     bool destinationChanged, bool queueEmpty) {
  if (currentGuid.empty() || currentGuid != snapshotGuid)
    return "recovery_guid_mismatch";
  if (!idle)
    return "controller_not_idle";
  if (snapshotRevision && snapshotRevision < currentRevision)
    return "recovery_configuration_older";
  if (sameRevisionChanged)
    return "recovery_revision_conflict";
  if (destinationChanged && !queueEmpty)
    return "outbox_destination_locked";
  return nullptr;
}
// Stage remains until every write succeeds. Retrying after reset is idempotent.
// Identity, queue, occurrence ledger, audit and command history are never
// written.
template <class Store>
bool applyRecoveryPlan(const RecoveryPlan &plan, Store &store) {
  if (store.guid() != plan.guid)
    return false;
  if (!store.hold() || !store.text("recover_token", plan.tokenHash) ||
      !store.text("prov_v1", plan.profile))
    return false;
  if (!plan.configuration.empty() && !store.text("config", plan.configuration))
    return false;
  for (unsigned i = 0; i < 4; ++i) {
    if (!store.text("cal" + std::to_string(i), plan.calibration[i]) ||
        !store.text("rom" + std::to_string(i), plan.roms[i]))
      return false;
  }
  return store.removeStage();
}
} // namespace irrigation

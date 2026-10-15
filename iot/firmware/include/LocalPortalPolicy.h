#pragma once
#include <stdint.h>
namespace irrigation {
inline const char *draftRejection(int role,bool csrf,bool central,bool remote,
    bool active,uint32_t expected,uint32_t current) {
  if (role!=2) return "administrator_required";
  if (!csrf) return "csrf_invalid";
  if (central || remote) return "portal_is_configuration_authority";
  if (active) return "controller_not_idle";
  if (expected!=current) return "revision_conflict";
  if (current==UINT32_MAX) return "revision_exhausted";
  return nullptr;
}
} // namespace irrigation

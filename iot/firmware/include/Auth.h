#pragma once
#include <string>
namespace irrigation {
inline bool credentialMatches(const std::string &supplied,
                              const std::string &expected) {
  if (supplied.rfind("Basic ", 0) != 0 || expected.rfind("Basic ", 0) != 0 ||
      expected.size() <= 6 || supplied.size() != expected.size())
    return false;
  unsigned diff = 0;
  for (size_t i = 0; i < expected.size(); ++i)
    diff |= static_cast<unsigned char>(supplied[i]) ^
            static_cast<unsigned char>(expected[i]);
  return diff == 0;
}
} // namespace irrigation

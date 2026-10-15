#include "Auth.h"
#include <cassert>
int main() {
  const std::string expected = "Basic YWRtaW46c2VjcmV0";
  assert(irrigation::credentialMatches(expected, expected));
  assert(!irrigation::credentialMatches("admin", expected));
  assert(!irrigation::credentialMatches("Bearer YWRtaW46c2VjcmV0", expected));
  assert(!irrigation::credentialMatches("Basic YWRtaW46c2VjcmV1", expected));
  assert(!irrigation::credentialMatches("Basic ", expected));
  assert(!irrigation::credentialMatches(expected, "Basic "));
}

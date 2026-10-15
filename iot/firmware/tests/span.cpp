#include "JsonSpan.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
int main(int argc, char **argv) {
  size_t a = 0, b = 0;
  assert(irrigation::objectMember(
      "{\"x\":3,\"configuration\":{\"v\":\"}\\\"[\",\"n\":[1,2]}}",
      "configuration", a, b));
  assert(!irrigation::objectMember("{}", "configuration", a, b));
  assert(
      !irrigation::objectMember("{\"configuration\":{", "configuration", a, b));
  if (argc > 1) {
    std::ifstream in(argv[1]);
    std::string s((std::istreambuf_iterator<char>(in)), {});
    assert(irrigation::objectMember(s, "configuration", a, b));
    std::cout << s.substr(a, b - a);
  }
}

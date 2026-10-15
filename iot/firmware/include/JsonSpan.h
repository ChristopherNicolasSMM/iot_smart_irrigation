#pragma once
#include <cstddef>
#include <string>
namespace irrigation {
// Find exact bytes of a top-level object value; retain server JSON order for
// hash.
inline bool objectMember(const std::string &s, const std::string &name,
                         size_t &begin, size_t &end) {
  bool found = false;
  size_t i = 0;
  auto space = [&]() {
    while (i < s.size() &&
           (s[i] == ' ' || s[i] == '\n' || s[i] == '\r' || s[i] == '\t'))
      ++i;
  };
  auto stringEnd = [&](size_t start) -> size_t {
    bool escaped = false;
    for (size_t j = start + 1; j < s.size(); ++j) {
      if (escaped) {
        escaped = false;
        continue;
      }
      if (s[j] == '\\') {
        escaped = true;
        continue;
      }
      if (s[j] == '"')
        return j + 1;
    }
    return s.size() + 1;
  };
  space();
  if (i == s.size() || s[i++] != '{')
    return false;
  while (i < s.size()) {
    space();
    if (i == s.size())
      return false;
    if (s[i] == '}') {
      ++i;
      space();
      return found && i == s.size();
    }
    if (s[i] != '"')
      return false;
    size_t keyStart = i, keyEnd = stringEnd(i);
    if (keyEnd > s.size())
      return false;
    i = keyEnd;
    space();
    if (i == s.size() || s[i++] != ':')
      return false;
    space();
    size_t start = i;
    int depth = 0;
    bool inString = false, escaped = false;
    while (i < s.size()) {
      char c = s[i];
      if (inString) {
        if (escaped)
          escaped = false;
        else if (c == '\\')
          escaped = true;
        else if (c == '"')
          inString = false;
      } else if (c == '"')
        inString = true;
      else if (c == '{' || c == '[')
        ++depth;
      else if (c == '}' || c == ']') {
        if (!depth)
          break;
        --depth;
      } else if (c == ',' && !depth)
        break;
      ++i;
    }
    if (inString || depth || i == start)
      return false;
    size_t stop = i;
    while (stop > start && (s[stop - 1] == ' ' || s[stop - 1] == '\n' ||
                            s[stop - 1] == '\r' || s[stop - 1] == '\t'))
      --stop;
    if (s.substr(keyStart + 1, keyEnd - keyStart - 2) == name) {
      if (found)
        return false;
      begin = start;
      end = stop;
      found = true;
    }
    if (i < s.size() && s[i] == '}') {
      ++i;
      space();
      return found && i == s.size();
    }
    if (i == s.size() || s[i] != ',')
      return false;
    ++i;
  }
  return false;
}
} // namespace irrigation

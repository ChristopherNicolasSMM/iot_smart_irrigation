#pragma once
#include <stddef.h>
#include <stdint.h>
namespace irrigation {
namespace wire {
// Fixed 28-byte frames, explicitly little endian; not Modbus and not PoE.
constexpr size_t frameSize = 28;
enum Type : uint8_t { Poll = 1, Start = 2, Stop = 3, Reply = 4 };
struct Frame {
  uint8_t node = 0, type = 0;
  uint32_t session = 0, sequence = 0, durationMs = 0;
  uint16_t value = 0;
  uint64_t token = 0;
};
inline uint16_t crc(const uint8_t *p, size_t n) {
  uint16_t c = 0xffff;
  while (n--) {
    c ^= *p++;
    for (int i = 0; i < 8; ++i)
      c = (c & 1) ? uint16_t((c >> 1) ^ 0xa001) : uint16_t(c >> 1);
  }
  return c;
}
inline void put32(uint8_t *p, uint32_t n) {
  for (int i = 0; i < 4; ++i)
    p[i] = uint8_t(n >> (8 * i));
}
inline uint32_t get32(const uint8_t *p) {
  uint32_t n = 0;
  for (int i = 0; i < 4; ++i)
    n |= uint32_t(p[i]) << (8 * i);
  return n;
}
inline void encode(const Frame &f, uint8_t out[frameSize]) {
  out[0] = 0xa5;
  out[1] = 1;
  out[2] = f.node;
  out[3] = f.type;
  put32(out + 4, f.session);
  put32(out + 8, f.sequence);
  put32(out + 12, f.durationMs);
  out[16] = uint8_t(f.value);
  out[17] = uint8_t(f.value >> 8);
  put32(out + 18, uint32_t(f.token));
  put32(out + 22, uint32_t(f.token >> 32));
  auto c = crc(out, 26);
  out[26] = uint8_t(c);
  out[27] = uint8_t(c >> 8);
}
inline bool decode(const uint8_t *p, Frame &f) {
  if (p[0] != 0xa5 || p[1] != 1 || p[2] < 1 || p[2] > 4 || p[3] < Poll ||
      p[3] > Reply || crc(p, 26) != (uint16_t(p[26]) | (uint16_t(p[27]) << 8)))
    return false;
  f.node = p[2];
  f.type = p[3];
  f.session = get32(p + 4);
  f.sequence = get32(p + 8);
  f.durationMs = get32(p + 12);
  f.value = uint16_t(p[16]) | (uint16_t(p[17]) << 8);
  f.token = uint64_t(get32(p + 18)) | (uint64_t(get32(p + 22)) << 32);
  return true;
}
class Parser {
  uint8_t bytes_[frameSize] = {};
  size_t n_ = 0;

public:
  bool push(uint8_t b, Frame &f) {
    if (!n_ && b != 0xa5)
      return false;
    bytes_[n_++] = b;
    if (n_ < frameSize)
      return false;
    if (decode(bytes_, f)) {
      n_ = 0;
      return true;
    }
    // Sliding resynchronization, including a marker inside corrupted frames.
    size_t next = 1;
    while (next < frameSize && bytes_[next] != 0xa5)
      ++next;
    n_ = frameSize - next;
    for (size_t i = 0; i < n_; ++i)
      bytes_[i] = bytes_[next + i];
    return false;
  }
  void reset() { n_ = 0; }
};
// Node lease: never extends an existing run on duplicate, session changes fail
// off.
class Lease {
  uint64_t token_ = 0;
  uint32_t session_ = 0, seq_ = 0, began_ = 0, duration_ = 0, last_ = 0;
  bool active_ = false, seen_ = false;

public:
  void bootToken(uint64_t token) {
    token_ = token;
    active_ = false;
  }
  bool accept(const Frame &f, uint32_t now, uint32_t maxMs) {
    if (f.type == Stop) {
      active_ = false;
      return true;
    }
    if (f.type != Start && f.type != Poll)
      return false;
    if (f.type == Start &&
        (!token_ || f.token != token_ || session_ != f.session))
      return false;
    if (session_ != f.session) {
      active_ = false;
      session_ = f.session;
      seq_ = 0;
      seen_ = false;
    }
    if (f.type == Poll) {
      last_ = now;
      return true;
    }
    if (seen_ && f.sequence <= seq_)
      return false;
    if (active_ || !f.durationMs || f.durationMs > maxMs ||
        f.durationMs >= 0x80000000u)
      return false;
    seq_ = f.sequence;
    seen_ = true;
    began_ = now;
    duration_ = f.durationMs;
    last_ = now;
    active_ = true;
    return true;
  }
  bool tick(uint32_t now, uint32_t timeout) {
    if (active_ && (uint32_t(now - began_) >= duration_ ||
                    uint32_t(now - last_) >= timeout))
      active_ = false;
    return active_;
  }
  bool active() const { return active_; }
};
} // namespace wire
} // namespace irrigation

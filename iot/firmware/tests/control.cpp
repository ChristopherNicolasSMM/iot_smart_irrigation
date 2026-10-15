#include "Controller.h"
#include "RemoteProtocol.h"
#include "Schedule.h"
#include <cassert>
#include <cstdlib>
#include <string>
#include <vector>
using namespace irrigation;
struct IO : Backend {
  bool available = true;
  bool outputs[4] = {};
  bool on(size_t i, uint32_t) override {
    if (!available)
      return false;
    outputs[i] = true;
    return true;
  }
  void off(size_t i) override { outputs[i] = false; }
};
struct Log : Journal {
  bool persist = true, ready = true;
  std::vector<std::string> rows;
  bool consume(size_t, uint64_t) override { return persist; }
  bool reserve(size_t, Source) override { return ready; }
  void event(size_t, const char *k, const char *r, Source) override {
    rows.push_back(std::string(k) + ":" + r);
  }
};
int main() {
  IO io;
  Log log;
  Controller c(io, log);
  Zone z[4];
  z[0].enabled = true;
  assert(c.configure(z, 1, 1));
  Window w[4];
  assert(!c.manual(0, 0, 1000, true));
  c.clockValid(true);
  assert(c.manual(0, 0xfffffff0u, 1000, true));
  assert(!c.manual(0, 0, 1000, true));
  c.tick(984, w);
  assert(!c.run(0).active && !io.outputs[0]);
  assert(!c.manual(0, 0, 31000, true));
  w[0].open = true;
  w[0].startNow = true;
  w[0].remainingMs = 1000;
  w[0].occurrence = 1;
  log.persist = false;
  c.tick(1000, w);
  assert(!c.run(0).active);
  log.persist = true;
  c.tick(1000, w);
  assert(c.run(0).active);
  c.stop(0, 1001);
  c.tick(1002, w);
  assert(!c.run(0).active);
  z[0].mode = Mode::Dynamic;
  z[0].sensor = 0;
  z[0].pauseMs = 0;
  assert(c.configure(z, 1, 1));
  c.reading(0, {20, 1002, true});
  c.tick(1002, w);
  assert(c.run(0).active);
  c.reading(0, {45, 1003, true});
  c.tick(1003, w);
  assert(!c.run(0).active);
  c.reading(0, {20, 1004, true});
  c.tick(1004, w);
  assert(c.run(0).active);
  c.reading(0, {0, 1004, false});
  c.tick(1005, w);
  assert(!io.outputs[0]);
  z[0].mode = Mode::Hybrid;
  assert(c.configure(z, 1, 1));
  w[0].open = false;
  c.reading(0, {20, 1010, true});
  c.tick(1010, w);
  assert(!c.run(0).active);
  w[0].open = true;
  w[0].occurrence = 2;
  c.tick(1011, w);
  assert(c.run(0).active);
  w[0].open = false;
  c.tick(1012, w);
  assert(!c.run(0).active);
  w[0].open = true;
  c.tick(1013, w);
  assert(!c.run(0).active);
  z[0].mode = Mode::Static;
  z[1] = z[0];
  assert(c.configure(z, 2, 1));
  assert(c.manual(0, 2000, 1000, false));
  assert(!c.manual(1, 2000, 1000, false));
  c.stop(0, 2001);
  log.ready = false;
  assert(!c.manual(0, 2002, 1000, false));
  assert(!io.outputs[0]);
  log.ready = true;
  io.available = false;
  assert(!c.manual(0, 2002, 1000, false));
  assert(!c.run(0).active);
  wire::Frame frame;
  frame.node = 1;
  frame.type = wire::Start;
  frame.session = 8;
  frame.sequence = 1;
  frame.durationMs = 1000;
  uint8_t bytes[wire::frameSize];
  wire::encode(frame, bytes);
  wire::Frame decoded;
  assert(wire::decode(bytes, decoded));
  bytes[8] ^= 1;
  assert(!wire::decode(bytes, decoded));
  wire::Parser parser;
  assert(!parser.push(0, decoded));
  wire::encode(frame, bytes);
  bool found = false;
  for (auto b : bytes)
    found = parser.push(b, decoded) || found;
  assert(found);
  wire::Lease lease;
  lease.bootToken(99);
  frame.token = 99;
  auto poll = frame;
  poll.type = wire::Poll;
  assert(lease.accept(poll, 0, 30000));
  assert(lease.accept(frame, 0, 30000));
  assert(!lease.accept(frame, 900, 30000));
  assert(!lease.tick(1000, 3000));
  frame.sequence = 2;
  assert(lease.accept(frame, 1001, 30000));
  assert(!lease.tick(4001, 3000));
  frame.sequence = 3;
  assert(lease.accept(frame, 5000, 30000));
  frame.type = wire::Poll;
  frame.session = 9;
  assert(lease.accept(frame, 5001, 30000));
  assert(!lease.active());
  frame.type = wire::Start;
  frame.token = 98;
  assert(!lease.accept(frame, 5002, 30000));
  setenv("TZ", "UTC0", 1);
  tzset();
  uint32_t sec;
  assert(validTime("06:00", sec));
  assert(!validTime("24:00", sec));
  Schedule s;
  s.enabled = true;
  s.weekday = 1;
  s.startSecond = 23 * 3600 + 50 * 60;
  s.durationSecond = 1200;
  s.id = 1;
  tm t{};
  t.tm_year = 126;
  t.tm_mon = 9;
  t.tm_mday = 13;
  t.tm_hour = 0;
  t.tm_min = 5;
  time_t now = mktime(&t);
  auto window = evaluate(s, now, now - 1, 1);
  assert(window.open && !window.startNow && window.remainingMs == 300000);
  t.tm_min = 10;
  now = mktime(&t);
  assert(!evaluate(s, now, now - 1, 1).open);
}

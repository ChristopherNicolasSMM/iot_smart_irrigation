#include "Controller.h"
#include "OccurrenceLedger.h"
#include <cassert>
#include <vector>
#include <cstring>
using namespace irrigation;
struct Disk {
  std::vector<uint8_t> bytes;
  enum Fault { None, Before, After, Truncated } fault=None;
  bool operator()(const uint8_t *data,size_t n) {
    if (fault==Before) return false;
    bytes.assign(data,data+n-(fault==Truncated ? 1 : 0));
    return fault==None;
  }
};
struct Output : Backend {
  size_t starts=0;
  bool on(size_t,uint32_t) override { ++starts;return true; }
  void off(size_t) override {}
};
struct Log : Journal {
  OccurrenceLedger ledger;
  Disk disk;
  bool contains(size_t s,uint64_t k) const override { return ledger.contains(s,k); }
  bool consume(size_t,uint64_t) override { return false; }
  bool consumeWindow(size_t s,uint64_t k,int64_t begins) override {
    auto next=ledger;
    auto result=next.add(s,k,begins);
    if (result==OccurrenceLedger::Result::Duplicate) return true;
    return result==OccurrenceLedger::Result::Added &&
      commitOccurrenceLedger(ledger,next,[&](const uint8_t *p,size_t n){return disk(p,n);});
  }
  void event(size_t,const char *,const char *,Source) override {}
  bool save(const OccurrenceLedger &next) {
    return commitOccurrenceLedger(ledger,next,[&](const uint8_t *p,size_t n){return disk(p,n);});
  }
};
int main() {
  constexpr int64_t base=2208988800LL; // 2040; no 32-bit fallback
  Log log;auto initial=log.ledger;assert(initial.advance(base));assert(log.save(initial));
  Output output;Controller controller(output,log);Zone z[1];z[0].enabled=true;
  assert(controller.configure(z,1,1));controller.clockValid(true);
  uint64_t key=0;
  for (int day=1;day<=250;++day) {
    for (int item=0;item<64;++item) {
      const int64_t now=base+day*86400+item*60+1;
      if (log.ledger.compactionDue(now)) {
        auto next=log.ledger;assert(next.compact(now));assert(log.save(next));
        controller.clearOccurrenceCache();
      }
      Window w[4];w[0].open=true;w[0].startNow=true;w[0].remainingMs=1000;
      w[0].begins=now;w[0].occurrence=++key;w[0].retired=log.ledger.retired(now);
      const auto tick=uint32_t(key*3000);
      controller.tick(tick,w);assert(controller.run(0).active);
      controller.tick(tick+1000,w);assert(!controller.run(0).active);
      controller.tick(tick+1001,w);assert(!controller.run(0).active);
      assert(log.ledger.count<=128);
      if (key%97==0) { // reboot from only the persisted blob, no RAM cache
        OccurrenceLedger loaded;assert(loaded.decode(log.disk.bytes.data(),log.disk.bytes.size()));
        log.ledger=loaded;assert(controller.configure(z,1,1));
        controller.tick(tick+1002,w);assert(!controller.run(0).active);
      }
    }
  }
  assert(output.starts==16000);
  const auto compactedFloor=log.ledger.floor;
  Window old[4];old[0].open=true;old[0].startNow=true;old[0].remainingMs=1000;
  old[0].begins=base+1;old[0].occurrence=999999; // a different revision/hash
  old[0].retired=log.ledger.retired(old[0].begins);
  assert(controller.configure(z,1,1));controller.tick(999,old);
  assert(output.starts==16000);
  z[0].mode=Mode::Hybrid;z[0].sensor=0;
  assert(controller.configure(z,1,1));controller.reading(0,Reading{10,999,true});
  controller.tick(999,old);assert(output.starts==16000);
  assert(!log.ledger.advance(compactedFloor-1));
  assert(!log.ledger.compact(compactedFloor-1));
  // Retiring the key of an active hybrid run must not close its window.
  Window current[4];current[0].open=true;current[0].remainingMs=1000;
  current[0].begins=compactedFloor+1;current[0].occurrence=888888;
  controller.tick(1000,current);assert(controller.run(0).active);
  current[0].retired=true;controller.tick(1100,current);
  assert(controller.run(0).active);controller.stop(0,1200);
  controller.tick(1201,current);assert(!controller.run(0).active);
  // Every byte is integrity protected, including floor/version/count.
  auto valid=log.disk.bytes;
  for (size_t i=0;i<valid.size();++i) {
    auto corrupt=valid;corrupt[i]^=1;OccurrenceLedger decoded;
    assert(!decoded.decode(corrupt.data(),corrupt.size()));
  }
  OccurrenceLedger decoded;
  assert(!decoded.decode(valid.data(),valid.size()-1));
  // A valid CRC cannot make an invalid version acceptable.
  auto wrongVersion=valid;wrongVersion[4]=2;
  OccurrenceLedger::put(wrongVersion.data()+wrongVersion.size()-4,
      OccurrenceLedger::crc(wrongVersion.data(),wrongVersion.size()-4),4);
  assert(!decoded.decode(wrongVersion.data(),wrongVersion.size()));
  // Commit failure before write never advances the in-memory tombstone.
  auto before=log.ledger;auto next=before;assert(next.advance(before.floor+86400));
  log.disk.fault=Disk::Before;assert(!log.save(next));assert(log.ledger.floor==before.floor);
  assert(decoded.decode(log.disk.bytes.data(),log.disk.bytes.size()));
  assert(decoded.floor==before.floor);
  // Power loss after durable commit: reboot sees the NEW floor, no old replay.
  log.disk.fault=Disk::After;assert(!log.save(next));assert(log.ledger.floor==before.floor);
  assert(decoded.decode(log.disk.bytes.data(),log.disk.bytes.size()));
  assert(decoded.floor==next.floor && decoded.retired(before.floor+1));
  log.disk.fault=Disk::Truncated;assert(!log.save(next));
  assert(!decoded.decode(log.disk.bytes.data(),log.disk.bytes.size()));
  // A genuine burst beyond the recent bound fails closed, without eviction.
  OccurrenceLedger full;assert(full.advance(base));
  for (uint64_t n=1;n<=128;++n)
    assert(full.add(0,n,base+1)==OccurrenceLedger::Result::Added);
  assert(full.add(0,129,base+1)==OccurrenceLedger::Result::Full);
  assert(full.add(0,1,base+1)==OccurrenceLedger::Result::Duplicate);
  assert(full.count==128);
  assert(full.advance(base+1));assert(full.count==0);
  assert(full.add(0,129,base+1)==OccurrenceLedger::Result::Retired);
  assert(full.add(0,130,base+2)==OccurrenceLedger::Result::Added);
  // Interrupted migration: no old keys are deleted before the new fence exists.
  OccurrenceLedger legacy;Disk migratingDisk;migratingDisk.fault=Disk::Before;
  auto fenced=legacy;assert(fenced.advance(base));
  assert(!commitOccurrenceLedger(legacy,fenced,[&](const uint8_t *p,size_t n){return migratingDisk(p,n);}));
  assert(legacy.floor==0 && migratingDisk.bytes.empty());
  // A failed durable consume must never reach the electrical backend.
  for (auto fault : {Disk::Before, Disk::After, Disk::Truncated}) {
    Log failed;auto fence=failed.ledger;assert(fence.advance(base));assert(failed.save(fence));
    failed.disk.fault=fault;
    Output safe;Controller guarded(safe,failed);Zone stat[1];stat[0].enabled=true;
    assert(guarded.configure(stat,1,1));guarded.clockValid(true);
    Window request[4];request[0].open=true;request[0].startNow=true;
    request[0].remainingMs=1000;request[0].begins=base+1;request[0].occurrence=44;
    guarded.tick(1,request);assert(safe.starts==0);
    if (fault==Disk::After) {
      OccurrenceLedger rebooted;
      assert(rebooted.decode(failed.disk.bytes.data(),failed.disk.bytes.size()));
      failed.ledger=rebooted;failed.disk.fault=Disk::None;
      assert(guarded.configure(stat,1,1));guarded.tick(2,request);
      assert(safe.starts==0); // conservative omission after ambiguous commit
    }
  }
}

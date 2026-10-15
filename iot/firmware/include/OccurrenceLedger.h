#pragma once
#include "TimePolicy.h"
#include <stddef.h>
#include <stdint.h>
#include <memory>
#include <new>
namespace irrigation {
// A temporal tombstone covers every removed key, including keys from previous
// revisions. Keep records for the last 24 hours (maximum schedule duration).
struct OccurrenceLedger {
  static constexpr size_t capacity = 128;
  static constexpr size_t maxBytes = 22 + capacity * 17;
  struct Record { uint64_t key = 0; int64_t begins = 0; uint8_t slot = 0; };
  enum class Result { Added, Duplicate, Retired, Full, Invalid };
  int64_t floor = 0;
  size_t count = 0;
  Record records[capacity];
  bool contains(size_t slot, uint64_t key) const {
    for (size_t i = 0; i < count; ++i)
      if (records[i].slot == slot && records[i].key == key) return true;
    return false;
  }
  bool retired(int64_t begins) const { return begins <= floor; }
  bool compactionDue(int64_t epoch) const {
    return validClockEpoch(epoch) && epoch >= floor && epoch-floor >= 90000;
  }
  bool compact(int64_t epoch) {
    return validClockEpoch(epoch) && epoch >= floor && advance(epoch-86400);
  }
  Result add(size_t slot, uint64_t key, int64_t begins) {
    if (slot >= 4 || !key || !validClockEpoch(begins)) return Result::Invalid;
    if (retired(begins)) return Result::Retired;
    if (contains(slot,key)) return Result::Duplicate;
    if (count == capacity) return Result::Full;
    records[count++] = Record{key,begins,uint8_t(slot)};
    return Result::Added;
  }
  bool advance(int64_t cutoff) {
    if (!validClockEpoch(cutoff) || cutoff < floor) return false;
    floor = cutoff;
    size_t kept = 0;
    for (size_t i = 0; i < count; ++i)
      if (!retired(records[i].begins)) records[kept++] = records[i];
    count = kept;
    return true;
  }
  static uint32_t crc(const uint8_t *data,size_t n) {
    uint32_t value=0xffffffffu;
    for (size_t i=0;i<n;++i) {
      value^=data[i];
      for (unsigned j=0;j<8;++j)
        value=(value>>1)^(0xedb88320u & (0u-(value&1u)));
    }
    return ~value;
  }
  static void put(uint8_t *out,uint64_t value,unsigned n) {
    for (unsigned i=0;i<n;++i) out[i]=uint8_t(value>>(i*8));
  }
  static uint64_t get(const uint8_t *in,unsigned n) {
    uint64_t value=0;
    for (unsigned i=0;i<n;++i) value|=uint64_t(in[i])<<(i*8);
    return value;
  }
  size_t encode(uint8_t *out,size_t available) const {
    const size_t size=22+count*17;
    if (!validClockEpoch(floor) || count>capacity || available<size) return 0;
    out[0]='I';out[1]='O';out[2]='T';out[3]='L';
    out[4]=1;out[5]=out[6]=out[7]=0;
    put(out+8,uint64_t(floor),8);put(out+16,count,2);
    for (size_t i=0;i<count;++i) {
      const size_t p=18+i*17;
      out[p]=records[i].slot;put(out+p+1,records[i].key,8);
      put(out+p+9,uint64_t(records[i].begins),8);
    }
    put(out+size-4,crc(out,size-4),4);
    return size;
  }
  bool decode(const uint8_t *in,size_t size) {
    if (size<22 || size>maxBytes || in[0]!='I' || in[1]!='O' ||
        in[2]!='T' || in[3]!='L' || in[4]!=1 || in[5] || in[6] || in[7] ||
        get(in+size-4,4)!=crc(in,size-4)) return false;
    const auto n=get(in+16,2), epoch=get(in+8,8);
    if (n>capacity || size!=22+n*17 || epoch>uint64_t(maxClockEpoch) ||
        !validClockEpoch(int64_t(epoch))) return false;
    OccurrenceLedger next;
    next.floor=int64_t(epoch);
    for (size_t i=0;i<n;++i) {
      const size_t p=18+i*17;
      const auto begins=get(in+p+9,8);
      if (begins>uint64_t(maxClockEpoch) ||
          next.add(in[p],get(in+p+1,8),int64_t(begins))!=Result::Added)
        return false;
    }
    *this=next;
    return true;
  }
};
// The same commit path is exercised with NVS and with fault-injected storage.
// Never mutate RAM until the complete blob (floor + keys) is durable.
template<class Write>
bool commitOccurrenceLedger(OccurrenceLedger &current,
                            const OccurrenceLedger &next, Write write) {
  auto bytes=std::unique_ptr<uint8_t[]>(new (std::nothrow) uint8_t[OccurrenceLedger::maxBytes]);
  if (!bytes) return false;
  const size_t length=next.encode(bytes.get(),OccurrenceLedger::maxBytes);
  if (!length || !write(bytes.get(),length)) return false;
  current=next;
  return true;
}
} // namespace irrigation

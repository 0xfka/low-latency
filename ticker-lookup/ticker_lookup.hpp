#pragma once

#include <immintrin.h>  // bzhi

#include <algorithm>  // for sorting at cold path
#include <cstdint>    // uint32 / 64 definitions etc.
#include <cstring>    // memcpy
#include <vector>

#include "../common/hugepage.hpp"
namespace ticker {

struct TickerEntry {
  const char* symbol;  // null-terminated, max 6 chars
  size_t symbol_len;
  uint32_t value;
};
struct InternalEntry {
  uint64_t key;    // Packed symbol
  uint32_t value;  // internal ID for state machine
};
class TickerLookup {
 public:
  static constexpr unsigned int used_cnt = 1UL << 15;
  static constexpr unsigned int bucket_cnt = 2048U;
  static_assert(bucket_cnt % 2 == 0);
  static_assert(used_cnt % 2 == 0);
  uint32_t* perfect_ = nullptr;
  uint64_t* key_ = nullptr;
  uint32_t* value_ = nullptr;
  TickerLookup() {
    // Valid + invalid tickers
    constexpr int perfect_footprint = bucket_cnt * sizeof(uint32_t);
    constexpr int key_footprint = used_cnt * sizeof(uint32_t);
    constexpr int value_footprint = used_cnt * sizeof(uint32_t);
    constexpr unsigned long mem_footprint =
        perfect_footprint + key_footprint + value_footprint;
    int required_hugepages = mem_footprint / (1UL << 20);
    if ((required_hugepages & 1) != 0 || required_hugepages == 0) {
      ++required_hugepages;
    }
    void* hugepage = common::alloc_hugepage(required_hugepages * 1024 * 1024);
    if (hugepage == nullptr) [[unlikely]]
      return;
    perfect_ = static_cast<uint32_t*>(hugepage);
    key_ = static_cast<uint64_t*>(hugepage) + perfect_footprint;
    // Key per valid/invalid element, 2000 is for invalid tickers
    value_ =
        static_cast<uint32_t*>(hugepage) + perfect_footprint + key_footprint;
  }

  /* Hash used to derive table positions. */
  /* Using a higher quality hash function will
     make it easier for build to do its job but its called at hot path,
     and faster hot path is selected over slower build() runtime.
   */
  // temporarily changed to the given paper
  constexpr inline uint32_t deterministic_hash(uint64_t x) noexcept {
    // https://arxiv.org/pdf/1504.06804
    constexpr uint64_t a1 = 0x4fad5e1e7bfd0b06;
    constexpr uint64_t a2 = 0x94a078ea5917aa97;
    constexpr uint64_t b = 0x7fe150b2a4ff0486;
    return ((a1 + x) * (a2 + (x >> 32)) + b) >> (32);
  }
  /* Receive all entries at once. Called once at init,  NOT measured.
     At an end to end state machine, it is positioned as running at the
     initialization at the day, when feed handler processed the tickers and
     sends them. Since all the tickers which will be processed throughout the
     day is known at start, we can do perfect hashing.
    */
  void build(const TickerEntry workload[], const size_t sz) {
    std::vector<int> buckets[bucket_cnt];
    /* This array will store is the position is already used,
     * and if matches, logic should break and try another candidate for perfect.
     */
    int used[used_cnt];  // 0 or 1
    /* Calculate which bucket the values will be put into */
    for (unsigned int i = 0; i < sz; i++) {
      uint64_t input = pack(workload[i].symbol, workload[i].symbol_len);
      /* getting first 32 bits of hash  */
      unsigned int bucket_index = deterministic_hash(input) & (bucket_cnt - 1);
      buckets[bucket_index].push_back(i);
    }
    /* Sort buckets from highest to lowest */
    std::vector<unsigned int> bucket_order(bucket_cnt);
    for (size_t i = 0; i < bucket_cnt; i++) {
      bucket_order[i] = i;
    }
    std::sort(bucket_order.begin(), bucket_order.end(),
              [&](unsigned int a, unsigned int b) {
                return buckets[a].size() > buckets[b].size();
              });
    /* Now, bucket_order[0] will be the biggest */
    for (size_t i = 0; i < bucket_cnt; i++) {
      /* This variable will be tested as a perfect val, and increased if
         collision is encountered. */
      uint32_t p = bucket_order[i];
      while (true) {
        int collision = false;
        auto& current_bucket = buckets[bucket_order[i]];
        for (size_t j = 0; j < current_bucket.size(); j++) {
          uint64_t key1 = pack(workload[current_bucket[j]].symbol,
                               workload[current_bucket[j]].symbol_len);
          size_t slot1 =
              ((deterministic_hash(key1) >> 11) ^ p) & (used_cnt - 1);
          if (used[slot1]) {
            collision = true;
            break;
          }
          for (size_t k = j + 1; k < current_bucket.size(); ++k) {
            uint64_t key2 = pack(workload[current_bucket[k]].symbol,
                                 workload[current_bucket[k]].symbol_len);
            size_t slot2 =
                ((deterministic_hash(key2) >> 11) ^ p) & (used_cnt - 1);
            if (slot1 == slot2) {
              collision = true;
              break;
            }
          }

          if (collision) break;
        }
        if (collision == true) p += 297317412;  // maybe use rand ?
        if (collision == false) {
          for (size_t val : current_bucket) {
            uint64_t key = pack(workload[val].symbol, workload[val].symbol_len);
            size_t slot =
                ((deterministic_hash(key) >> 11) ^ p) & (used_cnt - 1);
            used[slot] = true;
            key_[slot] = key;
            value_[slot] = workload[val].value;
          }
          perfect_[bucket_order[i]] = p;
          break;
        }
      }
    }
  }
  /* Look up a symbol. Returns pointer to value, or nullptr if not found.
     The value it's returning is internal ID which'll be used throughout the
     state machine.
   */
  inline uint32_t* find(const char* symbol, size_t symbol_len) noexcept {
    // TODO: As I remember the ticker lookup itself is faster than a branch
    // misprediction ( 15 cycles~ )
    if ((unsigned char)symbol[0] < 'A') [[unlikely]] {
      return nullptr;
    }
    const uint64_t key = pack(symbol, symbol_len);
    const uint32_t full_hash = deterministic_hash(key);
    const uint32_t p = perfect_[full_hash & (bucket_cnt - 1)];
    const uint32_t index = ((full_hash >> 11) ^ p) & (used_cnt - 1);

    return key_[index] == key ? &value_[index] : nullptr;
  }

 private:
  inline int64_t pack(const char* str, size_t len) noexcept {
    uint64_t key = 0;
    std::memcpy(&key, str, 8);
    uint64_t keep = len << 3;
    uint64_t bzhi = _bzhi_u64(key, keep);
    /*  turn uppercase ASCII letters into numbers between
        1-26. The math is, for example 'A', 0x41, if you remove
        6th bit ( 0x40 = 2^6), it's now 0x01 = 1. */
    return bzhi & 0xBFBFBFBFBFBFBFBFULL;
  }
};

}  // namespace ticker

#pragma once

#include <immintrin.h>  // bzhi

#include <algorithm>  // for sorting at cold path
#include <cstdint>    // uint32 / 64 definitions etc.
#include <cstring>    // memcpy
#include <vector>     // only for cold path
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
  TickerLookup();
  /* Receive all entries at once. Called once at init,  NOT measured.
     At an end to end state machine, it is positioned as running at the
     initialization at the day, when feed handler processed the tickers and
     sends them. Since all the tickers which will be processed throughout the
     day is known at start, we can do perfect hashing.
    */
  void build(const TickerEntry* entries, size_t count);

  /* Look up a symbol. Returns pointer to value, or nullptr if not found.
     The value it's returning is internal ID which'll be used throughout the
     state machine.
   */
  constexpr uint32_t* find(const char* symbol, size_t symbol_len) noexcept;

 private:
  constexpr inline int64_t pack(const char* str, size_t len) noexcept {
    uint64_t key = 0;
    std::memcpy(&key, str, 8);
    uint64_t keep = len << 3;
    uint64_t bzhi = _bzhi_u64(key, keep);
    return bzhi & 0xBFBFBFBFBFBFBFBFULL;
  }
};

}  // namespace ticker

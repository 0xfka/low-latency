#include <benchmark/benchmark.h>
#include <fastrange.h>
#include <sys/random.h>
#include <sys/syscall.h>

#include <cstdint>
#include <cstdlib>
#include <print>
#include <random>
#include <string>
#include <unordered_set>
#include <vector>

#include "../common/harness.hpp"
#include "ticker_lookup.hpp"
#include "tickers.hpp"

namespace ticker {

std::vector<std::string> generate_miss_keys(
    std::random_device& rd, size_t count,
    const std::unordered_set<std::string>& set) {
  std::mt19937 gen(rd());

  std::vector<std::string> keys;
  keys.reserve(count);
  for (size_t i = 0; i < count; i++) {
    while (1) {
      size_t len = 1 + fastrange32(gen(), 6);
      std::string val;
      for (size_t i = 0; i < len; i++) {
        val += static_cast<char>('A' + fastrange32(gen(), 26));
      }
      if (!set.contains(val)) {
        keys.push_back(val);
        break;
      }
    }
  }
  return keys;
}
/* Mixed hits (80%) and misses (20%)~
   If a pattern accours, such as 4 hit 1 miss every time,
   branch predictor will learn it. To prevent it, a std::mt19937_64
   with fastrange is used.
 */
int construct_bench() {
  std::unordered_set<std::string> map(std::begin(tickers), std::end(tickers));
  std::random_device rd;
  std::mt19937 gen(rd());
  constexpr size_t valid_workload_cnt = sizeof(tickers) / sizeof(tickers[0]);
  constexpr int miss_keys_cnt = []() {
    int init = (valid_workload_cnt / 4);
    while ((valid_workload_cnt + init) % 2 != 0) {
      init++;
    }
    return init;
  }();
  std::vector<TickerEntry> workload_temp;
  workload_temp.reserve(valid_workload_cnt + miss_keys_cnt);
  std::vector<std::string> miss_keys =
      generate_miss_keys(rd, miss_keys_cnt, map);
  size_t miss_key_counter = 0;
  size_t tickers_counter = 0;
  static_assert((valid_workload_cnt + miss_keys_cnt) % 2 == 0);
  TickerEntry valid_workload[valid_workload_cnt] = {};
  for (size_t i = 0; i < valid_workload_cnt + miss_keys_cnt; i += 5) {
    size_t pos = fastrange32(gen(), 4);
    for (size_t j = 0; j < 5; j++) {
      if (j == pos) {
        workload_temp.push_back(TickerEntry{miss_keys[miss_key_counter].data(),
                                            miss_keys[miss_key_counter].size(),
                                            (uint32_t)gen()});
        ++miss_key_counter;
      } else {
        uint32_t value = gen();
        workload_temp.push_back(
            TickerEntry{tickers[tickers_counter],
                        std::strlen(tickers[tickers_counter]), value});
        valid_workload[tickers_counter] =
            (TickerEntry{tickers[tickers_counter],
                         std::strlen(tickers[tickers_counter]), value});
        ++tickers_counter;
      }
    }
  }
  alignas(4096) TickerEntry workload[miss_keys_cnt + valid_workload_cnt];
  std::ranges::copy_n(workload_temp.begin(), std::size(workload), workload);
  TickerLookup c;

  c.build(valid_workload, valid_workload_cnt);
  uint32_t* ret = nullptr;
  for (size_t i = 0; i < valid_workload_cnt; i++) {
    ret = c.find(valid_workload[i].symbol, valid_workload[i].symbol_len);
    if (*ret != valid_workload[i].value) [[unlikely]] {
      std::println("Correctness test failed at iter {}, expected : {}, got {}",
                   i, valid_workload[i].symbol, *ret);
      return -1;
    }
  }
  // TODO: maybe benchmark other sizes, and compare ?
  benchmark::RegisterBenchmark("Find", [&c, &workload](benchmark::State& st) {
    int i = 0;
    for (auto _ : st) {
      int idx = i & (miss_keys_cnt + valid_workload_cnt) - 1;
      benchmark::DoNotOptimize(
          c.find(workload[idx].symbol, workload[idx].symbol_len));
      ++i;
    }
  });

  benchmark::Initialize(0, 0);
  benchmark::RunSpecifiedBenchmarks();
  return 0;
}
}  // namespace ticker
int main(void) {
  harness::pin_to_thread(7);  // TODO: magic num here.
  if (ticker::construct_bench() != 0) {
    return 1;
  }
}
/* Common library for benchmarking code execution times among codebase
 */
#include <pthread.h>
#include <sched.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <system_error>
#include <vector>

namespace harness {
/* rdtsc + drain pipeline.
   Backward only, will make sure every instruction before is executed.
 */
static inline uint64_t rdtscp() {
  uint64_t rax, rdx;
  uint32_t aux; /* value is not required */
  asm volatile("rdtscp\n" : "=a"(rax), "=d"(rdx), "=c"(aux) : :);
  return (rdx << 32) + rax;
}
static inline uint64_t rdtsc() {
  uint32_t lo, hi;
  __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
  return ((uint64_t)hi << 32) | lo;
}

static inline void cpuid() {
  unsigned int eax = 0, ebx, ecx, edx;
  __asm__ volatile("cpuid" : "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(eax) :);
}
inline uint64_t time_start(void) {
  /* serialize  */
  cpuid();
  /* Theoratically rdtsc has no guraantee for forward execution,
     but accepting this because it has no dependency or something else,
     so there's no reason for CPU to not execute it right after cpuid.
   */
  return rdtsc();
}
/* Call with what rdtsc_start returned */
inline uint64_t time_stop(void) {
  uint64_t t = rdtscp();
  /* Rdtscp has no guraantee for forward execution, because of that we are
     running CPUID. CPUID prevents forward and backward out of order execution.
   */
  cpuid();

  return t;
}
inline void pin_to_thread(const int thread) {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(thread, &cpuset);
  pthread_t t = pthread_self();
  const int err = pthread_setaffinity_np(t, sizeof(cpuset), &cpuset);
  if (err != 0) [[unlikely]] {
    throw std::system_error(err, std::system_category(),
                            "pthread_setaffinity_np");
  }
}
/* Google benchmark's DoNotOptimize
 */
template <class T>
inline void DoNotOptimize(T& value) {
#if defined(__clang__)
  asm volatile("" : "+r,m"(value) : : "memory");
#else
  asm volatile("" : "+m,r"(value) : : "memory");
#endif
}

template <int... Percentiles, class T>
inline auto calculate_percentiles(std::vector<T>& cycles) {
  static_assert(((Percentiles >= 1 && Percentiles <= 99) && ...),
                "percentiles must be between 1 and 99");
  std::array<T, sizeof...(Percentiles)> values{};
  std::size_t i = 0;
  for (const int percentile :
       std::array<int, sizeof...(Percentiles)>{Percentiles...}) {
    const std::size_t k = (percentile * cycles.size() - 1) / 100;
    std::nth_element(cycles.begin(), cycles.begin() + k, cycles.end());
    values[i++] = cycles[k];
  }
  return values;
}

};  // Namespace harness

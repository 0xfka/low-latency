#include "harness.hpp"

#include <pthread.h>
#include <sched.h>

#include <array>

#include "utility.hpp"

int main() {
  int failures = 0;
  std::vector<int> cycles = {10, 80, 30, 40, 50, 60, 70, 70, 90, 100};
  failures += !TEST(UnsortedDuplicatesAndRepeatedRequests,
                    harness::calculate_percentiles<90, 1, 50, 99, 50>(cycles) ==
                        std::array<int, 5>{90, 10, 60, 100, 60});
  harness::pin_to_thread(0);
  pthread_t t = pthread_self();
  cpu_set_t cpuset;
  failures += !TEST(pintothread,
                    pthread_getaffinity_np(t, sizeof(cpuset), &cpuset) == 0);
  return failures != 0;
}

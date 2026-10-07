#include "harness.hpp"

#include <array>
#include <cstdint>
#include <vector>

#include "utility.hpp"

int main() {
  int failures = 0;
  std::vector<std::uint64_t> cycles = {10, 80, 30, 40, 50, 60, 70, 70, 90, 100};
  failures += !TEST(UnsortedDuplicatesAndRepeatedRequests,
                    harness::print_percentiles<90, 1, 50, 99, 50>(cycles) ==
                        std::array<std::uint64_t, 5>{90, 10, 60, 100, 60});
  return failures != 0;
}

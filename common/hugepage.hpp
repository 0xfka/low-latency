#pragma once

#include <sys/mman.h>

namespace common {

inline void* alloc_hugepage(size_t alloc_size) noexcept {
  void* mapping =
      mmap(nullptr, alloc_size, PROT_READ | PROT_WRITE,
           MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | MAP_POPULATE, -1, 0);
  if (mapping == MAP_FAILED) [[unlikely]] {
    return nullptr;
  }
  return mapping;
}

}  // namespace common

// Aligned allocation shim. The VAD and resampler only require the documented
// contract: AlignedMalloc returns alignment-byte aligned storage, and
// AlignedFree releases it. The upstream bookkeeping prefix is not used.

#include "rtc_base/memory/aligned_malloc.h"

#include <cstdint>
#include <cstdlib>

namespace webrtc {

void* GetRightAlign(const void* ptr, size_t alignment) {
  const uintptr_t value = reinterpret_cast<uintptr_t>(ptr);
  const uintptr_t aligned = (value + alignment - 1) & ~(alignment - 1);
  return reinterpret_cast<void*>(aligned);
}

void* AlignedMalloc(size_t size, size_t alignment) {
  if (alignment < sizeof(void*)) {
    alignment = sizeof(void*);
  }
  void* ptr = nullptr;
  if (posix_memalign(&ptr, alignment, size) != 0) {
    return nullptr;
  }
  return ptr;
}

void AlignedFree(void* mem_block) {
  free(mem_block);
}

}  // namespace webrtc

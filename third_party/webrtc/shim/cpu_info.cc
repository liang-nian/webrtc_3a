// CPU-feature shim. Upstream rtc_base/cpu_info.cc is not in the VAD sources.
// __builtin_cpu_supports selects the same SSE2/AVX2/FMA kernels the upstream
// sinc resampler and VectorMath already contain. It does not change those
// kernels.

#include "rtc_base/cpu_info.h"

namespace webrtc {
namespace cpu_info {

uint32_t DetectNumberOfCores() {
  return 1;
}

bool Supports(ISA instruction_set_architecture) {
  switch (instruction_set_architecture) {
    case ISA::kSSE2:
      return __builtin_cpu_supports("sse2");
    case ISA::kSSE3:
      return __builtin_cpu_supports("sse3");
    case ISA::kAVX2:
      return __builtin_cpu_supports("avx2");
    case ISA::kFMA3:
      return __builtin_cpu_supports("fma");
    case ISA::kNeon:
      return false;
  }
  return false;
}

}  // namespace cpu_info
}  // namespace webrtc

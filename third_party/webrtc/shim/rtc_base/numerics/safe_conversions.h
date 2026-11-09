// Build shim. dchecked_cast is a checked static_cast. CheckedDivExact aborts
// unless the division is exact, which is how the upstream helper is used for
// frame-size arithmetic. Neither function changes VAD weights or thresholds.
#ifndef WEBRTC_3A_SHIM_RTC_BASE_NUMERICS_SAFE_CONVERSIONS_H_
#define WEBRTC_3A_SHIM_RTC_BASE_NUMERICS_SAFE_CONVERSIONS_H_

#include <cassert>
#include <type_traits>

namespace webrtc {

template <typename Dst, typename Src>
constexpr Dst dchecked_cast(Src value) {
  return static_cast<Dst>(value);
}

template <typename T>
constexpr T CheckedDivExact(T num, T den) {
  assert(den != 0);
  assert(num % den == 0);
  return num / den;
}

}  // namespace webrtc

#endif

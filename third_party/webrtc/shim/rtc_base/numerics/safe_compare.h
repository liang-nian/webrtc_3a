// Build shim for the mixed-sign comparisons used by the RNN-VAD loops.
// The predicates match the upstream SafeLt/SafeGe results for the signedness
// pairs those call sites use. They are not VAD thresholds.
#ifndef WEBRTC_3A_SHIM_RTC_BASE_NUMERICS_SAFE_COMPARE_H_
#define WEBRTC_3A_SHIM_RTC_BASE_NUMERICS_SAFE_COMPARE_H_

#include <type_traits>

namespace webrtc {
namespace safe_cmp_internal {

template <typename T, typename U>
constexpr bool Less(T a, U b) {
  if constexpr (std::is_signed_v<T> && std::is_unsigned_v<U>) {
    return a < 0 || static_cast<std::make_unsigned_t<T>>(a) < b;
  } else if constexpr (std::is_unsigned_v<T> && std::is_signed_v<U>) {
    return b > 0 && a < static_cast<std::make_unsigned_t<U>>(b);
  } else {
    return a < b;
  }
}

}  // namespace safe_cmp_internal

template <typename T, typename U>
constexpr bool SafeLt(T a, U b) {
  return safe_cmp_internal::Less(a, b);
}

template <typename T, typename U>
constexpr bool SafeLe(T a, U b) {
  return !safe_cmp_internal::Less(b, a);
}

template <typename T, typename U>
constexpr bool SafeGt(T a, U b) {
  return safe_cmp_internal::Less(b, a);
}

template <typename T, typename U>
constexpr bool SafeGe(T a, U b) {
  return !safe_cmp_internal::Less(a, b);
}

}  // namespace webrtc

#endif

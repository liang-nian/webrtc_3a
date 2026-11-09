// Build shim. Upstream rtc_base/checks.h is not part of the VAD algorithm.
// Debug checks keep the same predicates. They do not change thresholds,
// weights, or filter coefficients. Release builds strip them, as upstream does.
#ifndef WEBRTC_3A_SHIM_RTC_BASE_CHECKS_H_
#define WEBRTC_3A_SHIM_RTC_BASE_CHECKS_H_

#ifdef __cplusplus
#include <cassert>
#include <cstdlib>
#else
#include <assert.h>
#include <stdlib.h>
#endif

#ifdef __cplusplus
namespace webrtc_shim {

// Release-style DCHECK: the predicate is not evaluated. A constexpr function
// in pitch_search_internal.cc contains RTC_DCHECK, so the replacement has to
// be a constant expression. Upstream release builds strip these the same way.
struct CheckSink {
  constexpr CheckSink() = default;
  template <typename T>
  constexpr CheckSink& operator<<(const T&) {
    return *this;
  }
};

}  // namespace webrtc_shim

#define RTC_DCHECK(cond) ::webrtc_shim::CheckSink()
#define RTC_DCHECK_EQ(a, b) RTC_DCHECK(a)
#define RTC_DCHECK_NE(a, b) RTC_DCHECK(a)
#define RTC_DCHECK_LT(a, b) RTC_DCHECK(a)
#define RTC_DCHECK_LE(a, b) RTC_DCHECK(a)
#define RTC_DCHECK_GT(a, b) RTC_DCHECK(a)
#define RTC_DCHECK_GE(a, b) RTC_DCHECK(a)
#define RTC_DCHECK_NOTREACHED() RTC_DCHECK(0)

#else

#define RTC_DCHECK(cond) assert(cond)
#define RTC_DCHECK_EQ(a, b) assert((a) == (b))
#define RTC_DCHECK_NE(a, b) assert((a) != (b))
#define RTC_DCHECK_LT(a, b) assert((a) < (b))
#define RTC_DCHECK_LE(a, b) assert((a) <= (b))
#define RTC_DCHECK_GT(a, b) assert((a) > (b))
#define RTC_DCHECK_GE(a, b) assert((a) >= (b))
#define RTC_DCHECK_NOTREACHED() assert(0)

#endif

#define RTC_CHECK(cond)                                                      \
  do {                                                                       \
    if (!(cond)) {                                                           \
      abort();                                                               \
    }                                                                        \
  } while (0)
#define RTC_CHECK_EQ(a, b) RTC_CHECK((a) == (b))
#define RTC_CHECK_NE(a, b) RTC_CHECK((a) != (b))
#define RTC_CHECK_LT(a, b) RTC_CHECK((a) < (b))
#define RTC_CHECK_LE(a, b) RTC_CHECK((a) <= (b))
#define RTC_CHECK_GT(a, b) RTC_CHECK((a) > (b))
#define RTC_CHECK_GE(a, b) RTC_CHECK((a) >= (b))

#endif

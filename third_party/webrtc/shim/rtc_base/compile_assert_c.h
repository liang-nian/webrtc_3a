// Build shim. Upstream compile-time assert macro, C11 _Static_assert.
#ifndef WEBRTC_3A_SHIM_RTC_BASE_COMPILE_ASSERT_C_H_
#define WEBRTC_3A_SHIM_RTC_BASE_COMPILE_ASSERT_C_H_

#define RTC_COMPILE_ASSERT(expression) _Static_assert(expression, #expression)

#endif

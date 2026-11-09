// Build shim. The production sources only use this macro to declare a test
// friend. The friend is not instantiated here.
#ifndef WEBRTC_3A_SHIM_RTC_BASE_GTEST_PROD_UTIL_H_
#define WEBRTC_3A_SHIM_RTC_BASE_GTEST_PROD_UTIL_H_

#define FRIEND_TEST_ALL_PREFIXES(test_case_name, test_name)

#endif

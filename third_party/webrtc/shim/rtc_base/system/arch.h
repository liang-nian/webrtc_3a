// Build shim. Selects the x86 family the way upstream arch.h does on x86_64.
// It does not change VAD coefficients. SIMD kernels are the upstream ones.
#ifndef WEBRTC_3A_SHIM_RTC_BASE_SYSTEM_ARCH_H_
#define WEBRTC_3A_SHIM_RTC_BASE_SYSTEM_ARCH_H_

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#define WEBRTC_ARCH_X86_FAMILY
#endif
#if defined(__x86_64__) || defined(_M_X64)
#define WEBRTC_ARCH_64_BITS
#endif

#endif

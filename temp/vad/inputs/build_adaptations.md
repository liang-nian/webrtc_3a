# 构建适配

算法、权重、门限、滤波器系数都来自下面两个提交，文件字节和重新下载的结果一致。适配只发生在 `shim/`，以及 C++ 编译时为了让 `audio_view.h` 里的 `memcpy` 可见而加的 `-include cstring`。

| 来源 | 提交 |
|------|------|
| WebRTC src | `eeb7e6beb2e20c5a273922fdf22b75a75cc28ad5`（2026-11-07） |
| Chromium `third_party`（PFFFT、RNNoise 权重） | `63d42f6e6a54c9463fcb599001ca81ff2fd3dc8e`（2026-11-07） |

下载脚本是 `tools/fetch_webrtc_src.py`。`sim/vad/align_vad.py` 会再下一份到 `output/webrtc-ref/src`，用 `diff -rq` 对 `third_party/webrtc/src`，不一致就退出。

## shim 清单

| 文件 | 作用 |
|------|------|
| `rtc_base/checks.h` | Release 风格的 `RTC_DCHECK`，条件不求值，能放进 `constexpr`。`RTC_CHECK` 失败则 `abort` |
| `rtc_base/sanitizer.h` | `RTC_NO_SANITIZE` 展开为空 |
| `rtc_base/compile_assert_c.h` | `RTC_COMPILE_ASSERT` 变成 C11 `_Static_assert` |
| `rtc_base/system/arch.h` | 在 x86_64 上定义 `WEBRTC_ARCH_X86_FAMILY`，从而编进上游自带的 SSE/AVX 核 |
| `rtc_base/gtest_prod_util.h` | `FRIEND_TEST_ALL_PREFIXES` 展开为空 |
| `rtc_base/numerics/safe_compare.h` | `SafeLt` 等，供 RNN 循环使用 |
| `rtc_base/numerics/safe_conversions.h` | `dchecked_cast`、`CheckedDivExact` |
| `rtc_base/strings/string_builder.h` | 只给 `AvailableCpuFeatures::ToString` |
| `rtc_base/cpu_info.h` | 上游头文件原样拷贝 |
| `cpu_info.cc` | `Supports` 用 `__builtin_cpu_supports`，用来选择上游已有的 SSE2/AVX2/FMA 核 |
| `rtc_base/memory/aligned_malloc.h` | 上游头文件原样拷贝 |
| `aligned_malloc.cc` | `posix_memalign` / `free`，满足对齐分配约定 |
| `absl/strings/string_view.h` | `absl::string_view` = `std::string_view` |
| `absl/algorithm/container.h` | `absl::c_fill` |
| `api/function_view.h` | 调用 RNNoise 的 `TansigApproximated` / `SigmoidApproximated` 函数指针 |

参考构建和学习构建用同一套 shim、同一组编译选项。两边的差异只允许来自 `src/` 里的算法文件；对齐脚本要求这些文件字节相同。

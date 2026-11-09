// Build shim for the debug string in AvailableCpuFeatures::ToString.
// Not on the VAD sample path.
#ifndef WEBRTC_3A_SHIM_RTC_BASE_STRINGS_STRING_BUILDER_H_
#define WEBRTC_3A_SHIM_RTC_BASE_STRINGS_STRING_BUILDER_H_

#include <string>

namespace webrtc {

class StringBuilder {
 public:
  StringBuilder& operator<<(const char* text) {
    buffer_ += text;
    return *this;
  }
  std::string Release() { return std::move(buffer_); }

 private:
  std::string buffer_;
};

}  // namespace webrtc

#endif

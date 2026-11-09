// Build shim for the one absl::c_fill call in audio_view.h.
#ifndef WEBRTC_3A_SHIM_ABSL_ALGORITHM_CONTAINER_H_
#define WEBRTC_3A_SHIM_ABSL_ALGORITHM_CONTAINER_H_

#include <algorithm>

namespace absl {

template <typename Container, typename T>
void c_fill(Container& container, const T& value) {
  std::fill(std::begin(container), std::end(container), value);
}

}  // namespace absl

#endif

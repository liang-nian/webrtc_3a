// Build shim. absl::string_view is std::string_view. Layer names are only
// used in debug checks.
#ifndef WEBRTC_3A_SHIM_ABSL_STRINGS_STRING_VIEW_H_
#define WEBRTC_3A_SHIM_ABSL_STRINGS_STRING_VIEW_H_

#include <string_view>

namespace absl {
using string_view = std::string_view;
}

#endif

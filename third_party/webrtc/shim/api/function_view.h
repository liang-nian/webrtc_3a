// Build shim for the activation-function callback stored by the fully
// connected layer. It calls the same rnnoise::TansigApproximated and
// SigmoidApproximated function pointers the upstream FunctionView calls.
#ifndef WEBRTC_3A_SHIM_API_FUNCTION_VIEW_H_
#define WEBRTC_3A_SHIM_API_FUNCTION_VIEW_H_

namespace webrtc {

template <typename Sig>
class FunctionView;

template <typename R, typename... Args>
class FunctionView<R(Args...)> {
 public:
  FunctionView() = default;
  FunctionView(R (*fn)(Args...))
      : fn_(fn) {}

  R operator()(Args... args) const { return fn_(args...); }

 private:
  R (*fn_)(Args...) = nullptr;
};

}  // namespace webrtc

#endif

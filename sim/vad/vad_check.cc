// SpeechProbabilityBuffer sums from the upstream unit test. This class is not
// on GainController2::Process. The GMM and RNN comparisons live in align_vad.py.

#include "speech_probability_buffer.h"

#include <cmath>
#include <iostream>

namespace {

int g_fails = 0;

void Expect(bool ok, const char* what) {
  if (!ok) {
    std::cerr << "FAIL " << what << "\n";
    ++g_fails;
  } else {
    std::cout << "ok " << what << "\n";
  }
}

}  // namespace

int main() {
  webrtc_3a::SpeechProbabilityBuffer buffer(0.2f);
  Expect(buffer.SumProbabilities() == 0.f, "buffer sum starts at 0");
  buffer.Update(0.7f);
  buffer.Update(0.6f);
  Expect(std::fabs(buffer.SumProbabilities() - 1.3f) < 0.001f,
         "buffer sum after two updates");
  buffer.Reset();
  for (int i = 0; i < 6; ++i) {
    buffer.Update(0.f);
  }
  buffer.Update(1.f);
  buffer.Update(1.f);
  buffer.Update(1.f);
  Expect(std::fabs(buffer.SumProbabilities() - 3.f) < 0.001f,
         "three speech frames stay in the buffer");
  buffer.Update(0.f);
  Expect(std::fabs(buffer.SumProbabilities()) < 0.001f,
         "a 3-frame transient is removed");
  buffer.Reset();
  for (int i = 0; i < 9; ++i) {
    buffer.Update(1.f);
  }
  buffer.Update(0.f);
  Expect(std::fabs(buffer.SumProbabilities() - 9.f) < 0.001f,
         "a 9-frame run is not a transient");
  if (g_fails != 0) {
    return 1;
  }
  std::cout << "vad_check passed\n";
  return 0;
}

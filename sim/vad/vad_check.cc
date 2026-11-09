// Checks that follow the upstream RNN-VAD tests which do not need the
// binary resource files: silence forces probability 0, reset reproduces
// the same probability, a lower tone gets a larger pitch feature than a
// higher tone, and SpeechProbabilityBuffer matches its unit-test sums.

#include "agc2_rnn_vad.h"
#include "speech_probability_buffer.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kFeatures[42] = {
    -1.00131f,   -0.627069f, -7.81097f,  7.86285f,    -2.87145f,  3.32365f,
    -0.653161f,  0.529839f,  -0.425307f, 0.25583f,    0.235094f,  0.230527f,
    -0.144687f,  0.182785f,  0.57102f,   0.125039f,   0.479482f,  -0.0255439f,
    -0.0073141f, -0.147346f, -0.217106f, -0.0846906f, -8.34943f,  3.09065f,
    1.42628f,    -0.85235f,  -0.220207f, -0.811163f,  2.09032f,   -2.01425f,
    -0.690268f,  -0.925327f, -0.541354f, 0.58455f,    -0.606726f, -0.0372358f,
    0.565991f,   0.435854f,  0.420812f,  0.162198f,   -2.13f,     10.0089f};

int g_fails = 0;

void Expect(bool ok, const char* what) {
  if (!ok) {
    std::cerr << "FAIL " << what << "\n";
    ++g_fails;
  } else {
    std::cout << "ok " << what << "\n";
  }
}

void Tone(float hz, float amp, std::vector<float>* dst) {
  dst->resize(4 * webrtc_3a::kAgc2VadFrameSamples);
  for (int i = 0; i < static_cast<int>(dst->size()); ++i) {
    (*dst)[i] =
        amp * std::sin(2.f * kPi * hz * i / webrtc_3a::kAgc2VadSampleRateHz);
  }
}

float LastPitchFeature(webrtc_3a::Agc2RnnVad* vad, const std::vector<float>& x,
                       bool* silence) {
  webrtc_3a::Agc2VadFrame frame;
  const int n = webrtc_3a::kAgc2VadFrameSamples;
  for (int i = 0; i + n <= static_cast<int>(x.size()); i += n) {
    frame = vad->ProcessFrame(x.data() + i);
  }
  *silence = frame.is_silence;
  return frame.pitch_feature;
}

}  // namespace

int main() {
  webrtc_3a::Agc2RnnVad rnn;
  float pre = 0.f;
  for (int i = 0; i < 10; ++i) {
    pre = rnn.ComputeProbability(kFeatures, false);
  }
  const float silenced = rnn.ComputeProbability(kFeatures, true);
  Expect(silenced == 0.f, "silence probability is 0");
  float again = 0.f;
  for (int i = 0; i < 10; ++i) {
    again = rnn.ComputeProbability(kFeatures, false);
  }
  const float post = rnn.ComputeProbability(kFeatures, false);
  // 10 warm-up frames, then one more, compared with the same sequence.
  float seq_a = 0.f;
  rnn.Reset();
  for (int i = 0; i < 10; ++i) {
    seq_a = rnn.ComputeProbability(kFeatures, false);
  }
  Expect(seq_a == pre, "reset reproduces the warmed-up probability");
  Expect(again == pre, "silence resets the GRU like Reset()");
  std::cout << "warmup_probability=" << pre << " after_extra=" << post << "\n";

  webrtc_3a::Agc2RnnVad vad;
  std::vector<float> zeros(webrtc_3a::kAgc2VadFrameSamples, 0.f);
  auto z = vad.ProcessFrame(zeros.data());
  Expect(z.is_silence && z.speech_probability == 0.f, "zero frame is silence");

  std::vector<float> low;
  std::vector<float> high;
  Tone(150.f, 1000.f, &low);
  Tone(250.f, 1000.f, &high);
  vad.Reset();
  bool sil_low = true;
  const float feat_low = LastPitchFeature(&vad, low, &sil_low);
  vad.Reset();
  bool sil_high = true;
  const float feat_high = LastPitchFeature(&vad, high, &sil_high);
  std::cout << "pitch_feature_150hz=" << feat_low
            << " pitch_feature_250hz=" << feat_high << "\n";
  Expect(!sil_low && !sil_high, "amplitude-1000 tones are not silence");
  Expect(feat_high < feat_low, "250 Hz pitch feature is below 150 Hz");

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
    std::cerr << g_fails << " checks failed\n";
    return 1;
  }
  std::cout << "vad_check passed\n";
  return 0;
}

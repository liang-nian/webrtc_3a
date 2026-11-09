// Runs VoiceActivityDetectorWrapper from the extracted WebRTC sources.
// 10 ms frames. Sample rates other than 24 kHz go through PushResampler,
// and the wrapper resets the GRU every 1500 ms, both inside the upstream file.
//
// Usage: rnn_run <s16le.pcm> <sample_rate>
// Prints one hex float probability per frame.

#include "modules/audio_processing/agc2/cpu_features.h"
#include "modules/audio_processing/agc2/vad_wrapper.h"

#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {

std::vector<int16_t> ReadPcm(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    std::cerr << "cannot read " << path << "\n";
    std::exit(1);
  }
  in.seekg(0, std::ios::end);
  const auto bytes = in.tellg();
  in.seekg(0);
  std::vector<int16_t> pcm(static_cast<size_t>(bytes) / sizeof(int16_t));
  in.read(reinterpret_cast<char*>(pcm.data()),
          static_cast<std::streamsize>(pcm.size() * sizeof(int16_t)));
  return pcm;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 3) {
    std::cerr << "usage: rnn_run pcm rate\n";
    return 2;
  }
  const int rate = std::stoi(argv[2]);
  if (rate % 100 != 0) {
    std::cerr << "rate must be a multiple of 100\n";
    return 2;
  }
  const size_t frame = static_cast<size_t>(rate / 100);
  const auto pcm = ReadPcm(argv[1]);
  webrtc::VoiceActivityDetectorWrapper vad(webrtc::GetAvailableCpuFeatures(),
                                           rate);
  std::vector<float> samples(frame);
  std::cout << "frame probability\n";
  for (size_t i = 0, n = 0; i + frame <= pcm.size(); i += frame, ++n) {
    for (size_t k = 0; k < frame; ++k) {
      samples[k] = static_cast<float>(pcm[i + k]);
    }
    webrtc::DeinterleavedView<const float> view(samples.data(), frame, 1);
    const float p = vad.Analyze(view);
    std::cout << n << " " << std::hexfloat << p << std::defaultfloat << "\n";
  }
  return 0;
}

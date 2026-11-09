// Runs the extracted WebRTC GMM VAD. Prints the raw CalcVad return (0..6)
// and the public WebRtcVad_Process return (0/1) for the same PCM.
// Two instances are used so the two calls do not share filter state.
//
// Usage: gmm_run <s16le.pcm> <sample_rate> <frame_ms> <mode>

extern "C" {
#include "common_audio/vad/include/webrtc_vad.h"
#include "common_audio/vad/vad_core.h"
}

#include <cstdint>
#include <fstream>
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

int RawDecision(VadInstT* self, int fs, const int16_t* frame, size_t n) {
  if (fs == 48000) {
    return WebRtcVad_CalcVad48khz(self, frame, n);
  }
  if (fs == 32000) {
    return WebRtcVad_CalcVad32khz(self, frame, n);
  }
  if (fs == 16000) {
    return WebRtcVad_CalcVad16khz(self, frame, n);
  }
  return WebRtcVad_CalcVad8khz(self, frame, n);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 5) {
    std::cerr << "usage: gmm_run pcm rate frame_ms mode\n";
    return 2;
  }
  const int rate = std::stoi(argv[2]);
  const int frame_ms = std::stoi(argv[3]);
  const int mode = std::stoi(argv[4]);
  const auto pcm = ReadPcm(argv[1]);
  const size_t frame = static_cast<size_t>(rate / 1000 * frame_ms);
  if (WebRtcVad_ValidRateAndFrameLength(rate, frame) != 0) {
    std::cerr << "invalid rate/frame\n";
    return 2;
  }

  VadInst* raw_handle = WebRtcVad_Create();
  VadInst* pub_handle = WebRtcVad_Create();
  if (WebRtcVad_Init(raw_handle) != 0 || WebRtcVad_Init(pub_handle) != 0 ||
      WebRtcVad_set_mode(raw_handle, mode) != 0 ||
      WebRtcVad_set_mode(pub_handle, mode) != 0) {
    std::cerr << "init failed\n";
    return 1;
  }
  auto* raw_core = reinterpret_cast<VadInstT*>(raw_handle);

  std::cout << "frame raw public\n";
  for (size_t i = 0; i + frame <= pcm.size(); i += frame) {
    const int raw = RawDecision(raw_core, rate, pcm.data() + i, frame);
    const int pub =
        WebRtcVad_Process(pub_handle, rate, pcm.data() + i, frame);
    std::cout << (i / frame) << " " << raw << " " << pub << "\n";
  }
  WebRtcVad_Free(raw_handle);
  WebRtcVad_Free(pub_handle);
  return 0;
}

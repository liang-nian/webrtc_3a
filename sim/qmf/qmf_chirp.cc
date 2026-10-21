// Linear chirp through WebRtcSpl_AnalysisQMF / SynthesisQMF.
// Parameters match doc/qmf.md §3.2: 32 kHz, 1 s, amplitude 0.8,
// 100 Hz -> 15.5 kHz. Subbands are passed through unchanged.
//
// Usage: qmf_chirp [out_dir]
// Writes in.f32 and out.f32 (float32, native endian) into out_dir.
// Default out_dir is output/sim/qmf.

#include "splitting_filter.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr int kFs = 32000;
constexpr double kDurationS = 1.0;
constexpr double kAmp = 0.8;
constexpr double kF0 = 100.0;
constexpr double kF1 = 15500.0;
constexpr int kFrame = 320;  // 10 ms at 32 kHz; band length 160 <= 320
constexpr double kPi = 3.14159265358979323846;

void WriteF32(const std::filesystem::path& path, const std::vector<float>& v) {
  std::ofstream out(path, std::ios::binary);
  if (!out) {
    throw std::runtime_error("cannot write " + path.string());
  }
  out.write(reinterpret_cast<const char*>(v.data()),
            static_cast<std::streamsize>(v.size() * sizeof(float)));
}

}  // namespace

int main(int argc, char** argv) {
  const int n = static_cast<int>(std::lround(kFs * kDurationS));
  if (n % kFrame != 0) {
    std::cerr << "length is not a whole number of frames\n";
    return 1;
  }

  std::vector<float> x(n);
  std::vector<float> y(n);
  const double k = (kF1 - kF0) / kDurationS;
  for (int i = 0; i < n; ++i) {
    const double t = static_cast<double>(i) / kFs;
    const double phase = 2.0 * kPi * (kF0 * t + 0.5 * k * t * t);
    x[i] = static_cast<float>(kAmp * std::sin(phase));
  }

  float ana1[6] = {};
  float ana2[6] = {};
  float syn1[6] = {};
  float syn2[6] = {};
  std::vector<float> low(kFrame / 2);
  std::vector<float> high(kFrame / 2);

  for (int i = 0; i < n; i += kFrame) {
    WebRtcSpl_AnalysisQMF(x.data() + i, kFrame, low.data(), high.data(), ana1,
                          ana2);
    WebRtcSpl_SynthesisQMF(low.data(), high.data(), kFrame / 2, y.data() + i,
                           syn1, syn2);
  }

  double e_x = 0.0;
  double e_y = 0.0;
  float peak_x = 0.0f;
  float peak_y = 0.0f;
  for (int i = 0; i < n; ++i) {
    e_x += static_cast<double>(x[i]) * x[i];
    e_y += static_cast<double>(y[i]) * y[i];
    peak_x = std::max(peak_x, std::fabs(x[i]));
    peak_y = std::max(peak_y, std::fabs(y[i]));
  }
  const double rms_x = std::sqrt(e_x / n);
  const double rms_y = std::sqrt(e_y / n);

  const std::filesystem::path out_dir =
      argc > 1 ? std::filesystem::path(argv[1])
               : std::filesystem::path("output/sim/qmf");
  std::filesystem::create_directories(out_dir);
  WriteF32(out_dir / "in.f32", x);
  WriteF32(out_dir / "out.f32", y);

  std::cout << "fs=" << kFs << " n=" << n << " frame=" << kFrame
            << " frames=" << (n / kFrame) << "\n"
            << "rms_in=" << rms_x << " rms_out=" << rms_y
            << " ratio=" << (rms_y / rms_x) << "\n"
            << "peak_in=" << peak_x << " peak_out=" << peak_y << "\n"
            << "wrote " << (out_dir / "in.f32") << " " << (out_dir / "out.f32")
            << "\n";
  return 0;
}

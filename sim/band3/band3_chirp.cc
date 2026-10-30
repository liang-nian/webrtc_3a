// Linear chirp through webrtc::ThreeBandFilterBank.
// Parameters match doc/band3.md: 48 kHz, 1 s, amplitude 0.8,
// 100 Hz -> 23 kHz. Subbands are passed through unchanged.
//
// Usage: band3_chirp [out_dir]
// Writes in.f32 and out.f32 (float32, native endian) into out_dir.
// Default out_dir is output/sim/band3.

#include "three_band_filter_bank.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr int kFs = 48000;
constexpr double kDurationS = 1.0;
constexpr double kAmp = 0.8;
constexpr double kF0 = 100.0;
constexpr double kF1 = 23000.0;
constexpr int kFrame = webrtc::ThreeBandFilterBank::kFullBandSize;
constexpr int kBand = webrtc::ThreeBandFilterBank::kSplitBandSize;
constexpr double kPi = 3.14159265358979323846;

void WriteF32(const std::filesystem::path& path, const std::vector<float>& v) {
  std::ofstream out(path, std::ios::binary);
  if (!out) {
    throw std::runtime_error("cannot write " + path.string());
  }
  out.write(reinterpret_cast<const char*>(v.data()),
            static_cast<std::streamsize>(v.size() * sizeof(float)));
}

void Process(webrtc::ThreeBandFilterBank& bank, const std::vector<float>& x,
             std::vector<float>& y) {
  for (int i = 0; i + kFrame <= static_cast<int>(x.size()); i += kFrame) {
    std::array<float, kFrame> in{};
    std::copy(x.begin() + i, x.begin() + i + kFrame, in.begin());
    std::array<std::array<float, kBand>, 3> bands{};
    std::array<std::span<float>, 3> view{bands[0], bands[1], bands[2]};
    bank.Analysis(in, view);
    std::array<float, kFrame> out{};
    bank.Synthesis(view, out);
    std::copy(out.begin(), out.end(), y.begin() + i);
  }
}

double SnrAtLag(const std::vector<float>& x, const std::vector<float>& y,
                int lag) {
  const int n = static_cast<int>(x.size()) - lag;
  double sig = 0.0;
  double err = 0.0;
  for (int i = 0; i < n; ++i) {
    const double d = static_cast<double>(y[i + lag]) - x[i];
    sig += static_cast<double>(x[i]) * x[i];
    err += d * d;
  }
  if (err <= 0.0) {
    return 1e9;
  }
  return 10.0 * std::log10(sig / err);
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

  webrtc::ThreeBandFilterBank bank;
  Process(bank, x, y);

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

  int best_lag = 0;
  double best_snr = -1e9;
  for (int lag = 0; lag <= 120; ++lag) {
    const double snr = SnrAtLag(x, y, lag);
    if (snr > best_snr) {
      best_snr = snr;
      best_lag = lag;
    }
  }

  std::vector<float> impulse(kFrame * 2, 0.0f);
  impulse[0] = 1.0f;
  std::vector<float> impulse_out(impulse.size(), 0.0f);
  webrtc::ThreeBandFilterBank impulse_bank;
  Process(impulse_bank, impulse, impulse_out);
  int impulse_lag = 0;
  float impulse_peak = 0.0f;
  for (int i = 0; i < static_cast<int>(impulse_out.size()); ++i) {
    const float a = std::fabs(impulse_out[i]);
    if (a > impulse_peak) {
      impulse_peak = a;
      impulse_lag = i;
    }
  }

  webrtc::ThreeBandFilterBank ones_bank;
  std::array<std::array<float, kBand>, 3> ones_bands{};
  std::array<std::span<float>, 3> ones_view{ones_bands[0], ones_bands[1],
                                            ones_bands[2]};
  std::array<float, kFrame> ones_in{};
  ones_in.fill(1.0f);
  ones_bank.Analysis(ones_in, ones_view);

  const std::filesystem::path out_dir =
      argc > 1 ? std::filesystem::path(argv[1])
               : std::filesystem::path("output/sim/band3");
  std::filesystem::create_directories(out_dir);
  WriteF32(out_dir / "in.f32", x);
  WriteF32(out_dir / "out.f32", y);

  std::cout << "fs=" << kFs << " n=" << n << " frame=" << kFrame
            << " frames=" << (n / kFrame) << "\n"
            << "rms_in=" << rms_x << " rms_out=" << rms_y
            << " ratio=" << (rms_y / rms_x) << "\n"
            << "peak_in=" << peak_x << " peak_out=" << peak_y << "\n"
            << "chirp_lag=" << best_lag << " chirp_snr_db=" << best_snr << "\n"
            << "impulse_lag=" << impulse_lag << " impulse_peak=" << impulse_peak
            << "\n"
            << "ones0=" << ones_bands[0][0] << " " << ones_bands[1][0] << " "
            << ones_bands[2][0] << "\n"
            << "wrote " << (out_dir / "in.f32") << " " << (out_dir / "out.f32")
            << "\n";
  return 0;
}

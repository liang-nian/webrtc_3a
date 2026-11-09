// Synthetic 24 kHz frames through Agc2RnnVad.
// Signals are generated here. None of them is recorded speech.
// Samples use the S16 float scale, matching the upstream feature extractor.
//
// Usage: vad_sim [out_dir]
// Writes one CSV per condition and summary.txt. Seed is 20261109.

#include "agc2_rnn_vad.h"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr int kFs = webrtc_3a::kAgc2VadSampleRateHz;
constexpr int kFrame = webrtc_3a::kAgc2VadFrameSamples;
constexpr double kPi = 3.14159265358979323846;
constexpr uint32_t kSeed = 20261109u;
constexpr double kDurationS = 3.2;

struct Row {
  int frame = 0;
  float p = 0.f;
  int silence = 0;
  int rnn_reset = 0;
  int period = 0;
  float pitch_hz = 0.f;
  float pitch_feature = 0.f;
  float rms_dbfs = 0.f;
  int confident = 0;
  int adjacent = 0;
  int gain_ok = 0;
  float level = 0.f;
  int level_updated = 0;
  int vol_closed = 0;
  float vol_ratio = -1.f;
  int vol_ok = 0;
};

void Harmonic(std::vector<float>* x, double f0, double amp, int n) {
  x->assign(n, 0.f);
  for (int k = 1; k <= 8; ++k) {
    const double ak = amp / k;
    for (int i = 0; i < n; ++i) {
      const double t = static_cast<double>(i) / kFs;
      (*x)[i] += static_cast<float>(ak * std::sin(2.0 * kPi * f0 * k * t));
    }
  }
}

void ApplySyllables(std::vector<float>* x, int on, int off) {
  const int period = on + off;
  for (int i = 0; i < static_cast<int>(x->size()); ++i) {
    const int m = i % period;
    float g = 0.f;
    if (m < on) {
      const int edge = std::min(on / 8, kFrame);
      if (m < edge) {
        g = 0.5f - 0.5f * std::cos(static_cast<float>(kPi * m / edge));
      } else if (m > on - edge) {
        g = 0.5f - 0.5f * std::cos(static_cast<float>(kPi * (on - m) / edge));
      } else {
        g = 1.f;
      }
    }
    (*x)[i] *= g;
  }
}

double Rms(const std::vector<float>& x) {
  double a = 0.0;
  for (float v : x) {
    a += static_cast<double>(v) * v;
  }
  return std::sqrt(a / static_cast<double>(x.size()));
}

void AddNoise(std::vector<float>* x, double sigma, std::mt19937* rng) {
  std::normal_distribution<double> dist(0.0, sigma);
  for (float& v : *x) {
    v += static_cast<float>(dist(*rng));
  }
}

std::vector<Row> Run(const std::vector<float>& x) {
  webrtc_3a::Agc2RnnVad vad;
  std::vector<Row> rows;
  const int n = static_cast<int>(x.size()) / kFrame * kFrame;
  for (int i = 0, frame = 0; i < n; i += kFrame, ++frame) {
    const auto f = vad.ProcessFrame(x.data() + i);
    Row r;
    r.frame = frame;
    r.p = f.speech_probability;
    r.silence = f.is_silence ? 1 : 0;
    r.rnn_reset = f.rnn_reset ? 1 : 0;
    r.period = f.pitch_period_48khz;
    r.pitch_hz = f.pitch_hz;
    r.pitch_feature = f.pitch_feature;
    r.rms_dbfs = f.rms_dbfs;
    r.confident = f.confident_speech ? 1 : 0;
    r.adjacent = f.adjacent_speech_frames;
    r.gain_ok = f.gain_increase_allowed ? 1 : 0;
    r.level = f.speech_level_dbfs;
    r.level_updated = f.speech_level_updated ? 1 : 0;
    r.vol_closed = f.volume_window_closed ? 1 : 0;
    r.vol_ratio = f.volume_speech_ratio;
    r.vol_ok = f.volume_update_allowed ? 1 : 0;
    rows.push_back(r);
  }
  return rows;
}

void WriteCsv(const std::filesystem::path& path, const std::vector<Row>& rows) {
  std::ofstream out(path);
  out << "frame,t,p,silence,rnn_reset,period,pitch_hz,pitch_feature,rms_dbfs,"
         "confident,adjacent,gain_ok,level_dbfs,level_updated,vol_closed,"
         "vol_ratio,vol_ok\n";
  out.setf(std::ios::fixed);
  out.precision(6);
  for (const Row& r : rows) {
    out << r.frame << "," << (r.frame * 0.01) << "," << r.p << "," << r.silence
        << "," << r.rnn_reset << "," << r.period << "," << r.pitch_hz << ","
        << r.pitch_feature << "," << r.rms_dbfs << "," << r.confident << ","
        << r.adjacent << "," << r.gain_ok << "," << r.level << ","
        << r.level_updated << "," << r.vol_closed << "," << r.vol_ratio << ","
        << r.vol_ok << "\n";
  }
}

struct Stats {
  double mean_p = 0.0;
  double mean_p_voiced = 0.0;
  int voiced = 0;
  int silence = 0;
  int confident = 0;
  int gain_ok = 0;
  int level_updated = 0;
  double mean_pitch_hz = 0.0;
  int pitch_n = 0;
  float level_end = 0.f;
  int first_gain_frame = -1;
  int reset_frames = 0;
};

Stats Summarize(const std::vector<Row>& rows) {
  Stats s;
  s.level_end = rows.empty() ? 0.f : rows.back().level;
  for (const Row& r : rows) {
    s.mean_p += r.p;
    s.silence += r.silence;
    s.confident += r.confident;
    s.gain_ok += r.gain_ok;
    s.level_updated += r.level_updated;
    s.reset_frames += r.rnn_reset;
    if (!r.silence) {
      s.mean_p_voiced += r.p;
      s.voiced += 1;
      s.mean_pitch_hz += r.pitch_hz;
      s.pitch_n += 1;
    }
    if (r.gain_ok && s.first_gain_frame < 0) {
      s.first_gain_frame = r.frame;
    }
  }
  if (!rows.empty()) {
    s.mean_p /= static_cast<double>(rows.size());
  }
  if (s.voiced) {
    s.mean_p_voiced /= s.voiced;
  }
  if (s.pitch_n) {
    s.mean_pitch_hz /= s.pitch_n;
  }
  return s;
}

void WriteSummaryLine(std::ostream& out, const std::string& name,
                      const Stats& s, double rms, const std::string& note) {
  out << name << " rms=" << rms << " mean_p=" << s.mean_p
      << " mean_p_nonsilence=" << s.mean_p_voiced << " silence=" << s.silence
      << "/" << (s.silence + s.voiced) << " confident=" << s.confident
      << " gain_frames=" << s.gain_ok << " first_gain_frame=" << s.first_gain_frame
      << " level_updates=" << s.level_updated << " level_end=" << s.level_end
      << " mean_pitch_hz_nonsilence=" << s.mean_pitch_hz
      << " rnn_reset_frames=" << s.reset_frames << " note=" << note << "\n";
}

}  // namespace

int main(int argc, char** argv) {
  const std::filesystem::path out_dir =
      argc > 1 ? std::filesystem::path(argv[1])
               : std::filesystem::path("output/sim/vad");
  std::filesystem::create_directories(out_dir);
  const int n = static_cast<int>(std::lround(kFs * kDurationS));
  std::mt19937 rng(kSeed);

  std::vector<float> silence(n, 0.f);

  std::vector<float> noise_quiet(n, 0.f);
  {
    std::mt19937 g(kSeed);
    AddNoise(&noise_quiet, 200.0, &g);
  }
  std::vector<float> noise_loud(n, 0.f);
  {
    std::mt19937 g(kSeed);
    AddNoise(&noise_loud, 4000.0, &g);
  }

  std::vector<float> harmonic;
  Harmonic(&harmonic, 150.0, 8000.0, n);
  const double harmonic_rms = Rms(harmonic);

  std::vector<float> syllables = harmonic;
  ApplySyllables(&syllables, static_cast<int>(0.30 * kFs),
                 static_cast<int>(0.20 * kFs));

  std::vector<float> mix = harmonic;
  const double sigma_10db = harmonic_rms / std::pow(10.0, 10.0 / 20.0);
  {
    std::mt19937 g(kSeed);
    AddNoise(&mix, sigma_10db, &g);
  }

  std::vector<float> carrier(n, 0.f);
  {
    std::mt19937 g(kSeed + 1);
    AddNoise(&carrier, 1500.0, &g);
  }
  std::vector<float> burst = carrier;
  std::vector<float> burst_src;
  Harmonic(&burst_src, 140.0, 8000.0, n);
  const int burst_at = static_cast<int>(1.2 * kFs);
  const int burst_n = static_cast<int>(0.08 * kFs);
  for (int i = 0; i < burst_n; ++i) {
    burst[burst_at + i] += burst_src[burst_at + i];
  }
  std::vector<float> long_burst = carrier;
  const int long_n = static_cast<int>(0.20 * kFs);
  for (int i = 0; i < long_n; ++i) {
    long_burst[burst_at + i] += burst_src[burst_at + i];
  }

  struct Item {
    const char* name;
    const std::vector<float>* x;
    const char* note;
  };
  const Item items[] = {
      {"silence", &silence, "synthetic_zeros"},
      {"noise_std200", &noise_quiet, "synthetic_gaussian_seed_20261109"},
      {"noise_std4000", &noise_loud, "synthetic_gaussian_seed_20261109"},
      {"harmonic_150", &harmonic, "synthetic_harmonics_not_speech"},
      {"syllable_150", &syllables, "synthetic_harmonics_not_speech"},
      {"harmonic_snr10", &mix, "synthetic_harmonics_plus_gaussian"},
      {"burst_80ms", &burst, "synthetic_harmonics_in_noise"},
      {"burst_200ms", &long_burst, "synthetic_harmonics_in_noise"},
  };

  std::ofstream summary(out_dir / "summary.txt");
  summary.setf(std::ios::fixed);
  summary.precision(4);
  summary << "fs=" << kFs << " duration_s=" << kDurationS << " frame=" << kFrame
          << " seed=" << kSeed << "\n";
  summary << "harmonic_peak_coeff=8000 harmonic_rms=" << harmonic_rms
          << " snr10_noise_sigma=" << sigma_10db << "\n";
  summary << "signals=synthetic S16-scale float, not recorded speech\n";

  for (const Item& item : items) {
    const auto rows = Run(*item.x);
    WriteCsv(out_dir / (std::string(item.name) + ".csv"), rows);
    const Stats s = Summarize(rows);
    WriteSummaryLine(summary, item.name, s, Rms(*item.x), item.note);
    WriteSummaryLine(std::cout, item.name, s, Rms(*item.x), item.note);
  }
  std::cout << "wrote " << out_dir << "\n";
  return 0;
}

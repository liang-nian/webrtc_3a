#include "agc2_rnn_vad.h"

#include "rnn_activations.h"
#include "rnn_vad_weights.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <vector>

namespace webrtc_3a {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr int kFs24 = 24000;
constexpr int kFrame10 = 240;
constexpr int kFrame20 = 480;
constexpr int kMinPitch24 = kFs24 / 800;           // 30
constexpr int kMaxPitch24 = static_cast<int>(kFs24 / 62.5);  // 384
constexpr int kBuf24 = kMaxPitch24 + kFrame20;     // 864
constexpr int kInitialMinPitch24 = 3 * kMinPitch24;
constexpr int kInitialNumLags24 = kMaxPitch24 - kInitialMinPitch24;  // 294
constexpr int kRefineNumLags24 = kMaxPitch24 + 1;  // 385
constexpr int kBuf12 = kBuf24 / 2;                 // 432
constexpr int kMaxPitch12 = kMaxPitch24 / 2;       // 192
constexpr int kInitialMinPitch12 = kInitialMinPitch24 / 2;
constexpr int kNumLags12 = kMaxPitch12 - kInitialMinPitch12;  // 147
constexpr int kFrame20_12 = kFrame20 / 2;          // 240
constexpr int kMinPitch48 = kMinPitch24 * 2;       // 60
constexpr int kMaxPitch48 = kMaxPitch24 * 2;       // 768
constexpr int kNumLpc = 5;
constexpr int kNumBands = 22;
constexpr int kNumLower = 6;
constexpr int kCepHist = 8;
constexpr int kOpusBands = 20;
constexpr int kFeat = 42;
constexpr float kSilenceThreshold = 0.04f;
constexpr float kWeightsScale = 1.f / 256.f;
constexpr float kVadConfidence = 0.95f;
constexpr int kAdjacent = 12;
constexpr float kLevelConfidenceMs = 400.f;
constexpr float kLevelLeak = 1.f - 1.f / kLevelConfidenceMs;
constexpr float kInitialSpeechLevelDbfs = -40.f;  // -20 - 15 - 5
constexpr float kMinDbfs = -90.30899869919436f;
constexpr int kVadResetFrames = 150;  // 1500 ms
constexpr float kVolumeSpeechThreshold = 0.7f;
constexpr float kVolumeRatioThreshold = 0.6f;
constexpr int kVolumeWindow = 100;

constexpr int kOpusBins[kOpusBands - 1] = {4,  4,  4,  4,  4,  4,  4,  4,  8, 8,
                                           8,  8,  16, 16, 16, 24, 24, 32, 48};

constexpr float kOpusWeights[kFrame20 / 2] = {
    0.f,       0.25f,      0.5f,       0.75f,  0.f,       0.25f,     0.5f,
    0.75f,     0.f,        0.25f,      0.5f,   0.75f,     0.f,       0.25f,
    0.5f,      0.75f,      0.f,        0.25f,  0.5f,      0.75f,     0.f,
    0.25f,     0.5f,       0.75f,      0.f,    0.25f,     0.5f,      0.75f,
    0.f,       0.25f,      0.5f,       0.75f,  0.f,       0.125f,    0.25f,
    0.375f,    0.5f,       0.625f,     0.75f,  0.875f,    0.f,       0.125f,
    0.25f,     0.375f,     0.5f,       0.625f, 0.75f,     0.875f,    0.f,
    0.125f,    0.25f,      0.375f,     0.5f,   0.625f,    0.75f,     0.875f,
    0.f,       0.125f,     0.25f,      0.375f, 0.5f,      0.625f,    0.75f,
    0.875f,    0.f,        0.0625f,    0.125f, 0.1875f,   0.25f,     0.3125f,
    0.375f,    0.4375f,    0.5f,       0.5625f, 0.625f,   0.6875f,   0.75f,
    0.8125f,   0.875f,     0.9375f,    0.f,    0.0625f,   0.125f,    0.1875f,
    0.25f,     0.3125f,    0.375f,     0.4375f, 0.5f,     0.5625f,   0.625f,
    0.6875f,   0.75f,      0.8125f,    0.875f, 0.9375f,   0.f,       0.0625f,
    0.125f,    0.1875f,    0.25f,      0.3125f, 0.375f,   0.4375f,   0.5f,
    0.5625f,   0.625f,     0.6875f,    0.75f,  0.8125f,   0.875f,    0.9375f,
    0.f,       0.0416667f, 0.0833333f, 0.125f, 0.166667f, 0.208333f, 0.25f,
    0.291667f, 0.333333f,  0.375f,     0.416667f, 0.458333f, 0.5f,   0.541667f,
    0.583333f, 0.625f,     0.666667f,  0.708333f, 0.75f,  0.791667f, 0.833333f,
    0.875f,    0.916667f,  0.958333f,  0.f,    0.0416667f, 0.0833333f, 0.125f,
    0.166667f, 0.208333f,  0.25f,      0.291667f, 0.333333f, 0.375f,  0.416667f,
    0.458333f, 0.5f,       0.541667f,  0.583333f, 0.625f,  0.666667f, 0.708333f,
    0.75f,     0.791667f,  0.833333f,  0.875f, 0.916667f,  0.958333f,  0.f,
    0.03125f,  0.0625f,    0.09375f,   0.125f, 0.15625f,  0.1875f,   0.21875f,
    0.25f,     0.28125f,   0.3125f,    0.34375f, 0.375f,  0.40625f,  0.4375f,
    0.46875f,  0.5f,       0.53125f,   0.5625f, 0.59375f,  0.625f,    0.65625f,
    0.6875f,   0.71875f,   0.75f,      0.78125f, 0.8125f, 0.84375f,  0.875f,
    0.90625f,  0.9375f,    0.96875f,   0.f,    0.0208333f, 0.0416667f, 0.0625f,
    0.0833333f, 0.104167f, 0.125f,     0.145833f, 0.166667f, 0.1875f, 0.208333f,
    0.229167f, 0.25f,      0.270833f,  0.291667f, 0.3125f, 0.333333f, 0.354167f,
    0.375f,    0.395833f,  0.416667f,  0.4375f, 0.458333f, 0.479167f, 0.5f,
    0.520833f, 0.541667f,  0.5625f,    0.583333f, 0.604167f, 0.625f,  0.645833f,
    0.666667f, 0.6875f,    0.708333f,  0.729167f, 0.75f,   0.770833f, 0.791667f,
    0.8125f,   0.833333f,  0.854167f,  0.875f, 0.895833f,  0.916667f, 0.9375f,
    0.958333f, 0.979167f};

constexpr int kSubHarmonicMultipliers[14] = {3, 2, 3, 2, 5, 2, 3,
                                              2, 3, 2, 5, 2, 3, 2};
constexpr int kInitialPitchPeriodThresholds[14] = {
    20, 45, 80, 125, 180, 245, 320, 405, 500, 605, 720, 845, 980, 1125};

float Dot(const float* a, const float* b, int n) {
  float s = 0.f;
  for (int i = 0; i < n; ++i) {
    s += a[i] * b[i];
  }
  return s;
}

float FloatS16ToDbfs(float v) {
  if (v <= 1.f) {
    return kMinDbfs;
  }
  return 20.f * std::log10(v) + kMinDbfs;
}

void RealDftOrdered(const float* x, float* spec) {
  constexpr int n = kFrame20;
  double dc = 0.0;
  double ny = 0.0;
  for (int i = 0; i < n; ++i) {
    dc += x[i];
    ny += (i & 1) ? -x[i] : x[i];
  }
  spec[0] = static_cast<float>(dc);
  spec[1] = static_cast<float>(ny);
  for (int k = 1; k < n / 2; ++k) {
    const double w = -2.0 * kPi * k / n;
    const double dc_k = std::cos(w);
    const double ds_k = std::sin(w);
    double c = 1.0;
    double s = 0.0;
    double re = 0.0;
    double im = 0.0;
    for (int i = 0; i < n; ++i) {
      re += x[i] * c;
      im += x[i] * s;
      const double nc = c * dc_k - s * ds_k;
      const double ns = s * dc_k + c * ds_k;
      c = nc;
      s = ns;
    }
    spec[2 * k] = static_cast<float>(re);
    spec[2 * k + 1] = static_cast<float>(im);
  }
  spec[1] = 0.f;
}

std::vector<float> ScaleBias(const int8_t* src, int n) {
  std::vector<float> out(n);
  for (int i = 0; i < n; ++i) {
    out[i] = kWeightsScale * static_cast<float>(src[i]);
  }
  return out;
}

std::vector<float> PreprocessFc(const int8_t* src, int n, int output_size) {
  if (output_size == 1) {
    return ScaleBias(src, n);
  }
  const int input_size = n / output_size;
  std::vector<float> w(n);
  for (int o = 0; o < output_size; ++o) {
    for (int i = 0; i < input_size; ++i) {
      w[o * input_size + i] =
          kWeightsScale * static_cast<float>(src[i * output_size + o]);
    }
  }
  return w;
}

std::vector<float> PreprocessGru(const int8_t* src, int n, int output_size) {
  constexpr int kGates = 3;
  const int width = n / (output_size * kGates);
  const int stride_src = kGates * output_size;
  const int stride_dst = width * output_size;
  std::vector<float> dst(n);
  for (int g = 0; g < kGates; ++g) {
    for (int o = 0; o < output_size; ++o) {
      for (int i = 0; i < width; ++i) {
        dst[g * stride_dst + o * width + i] = kWeightsScale * static_cast<float>(
            src[i * stride_src + g * output_size + o]);
      }
    }
  }
  return dst;
}

void ComputeLpc(const float* x, int n, float lpc[kNumLpc]) {
  float auto_corr[kNumLpc] = {};
  for (int lag = 0; lag < kNumLpc; ++lag) {
    float s = 0.f;
    for (int i = 0; i < n - lag; ++i) {
      s += x[i] * x[i + lag];
    }
    auto_corr[lag] = s;
  }
  if (auto_corr[0] == 0.f) {
    std::fill(lpc, lpc + kNumLpc, 0.f);
    return;
  }
  auto_corr[0] *= 1.0001f;
  const float denoise[4] = {0.000064f, 0.000256f, 0.000576f, 0.001024f};
  for (int i = 0; i < 4; ++i) {
    auto_corr[i + 1] -= auto_corr[i + 1] * denoise[i];
  }
  float a[kNumLpc - 1] = {};
  float error = auto_corr[0];
  for (int i = 0; i < kNumLpc - 1; ++i) {
    float reflection = 0.f;
    for (int j = 0; j < i; ++j) {
      reflection += a[j] * auto_corr[i - j];
    }
    reflection += auto_corr[i + 1];
    constexpr float kMinError = 1e-6f;
    if (std::fabs(error) < kMinError) {
      error = std::copysign(kMinError, error);
    }
    reflection /= -error;
    a[i] = reflection;
    for (int j = 0; j < ((i + 1) >> 1); ++j) {
      const float tmp1 = a[j];
      const float tmp2 = a[i - 1 - j];
      a[j] = tmp1 + reflection * tmp2;
      a[i - 1 - j] = tmp2 + reflection * tmp1;
    }
    error -= reflection * reflection * error;
    if (error < 0.001f * auto_corr[0]) {
      break;
    }
  }
  a[0] *= 0.9f;
  a[1] *= 0.9f * 0.9f;
  a[2] *= 0.9f * 0.9f * 0.9f;
  a[3] *= 0.9f * 0.9f * 0.9f * 0.9f;
  constexpr float kC = 0.8f;
  lpc[0] = a[0] + kC;
  lpc[1] = a[1] + kC * a[0];
  lpc[2] = a[2] + kC * a[1];
  lpc[3] = a[3] + kC * a[2];
  lpc[4] = kC * a[3];
}

void LpResidual(const float* lpc, const float* x, float* y, int n) {
  y[0] = x[0];
  for (int i = 1; i < n; ++i) {
    float s = x[i];
    const int m = std::min(i, kNumLpc);
    for (int j = 0; j < m; ++j) {
      s += lpc[j] * x[i - 1 - j];
    }
    y[i] = s;
  }
}

struct PitchInfo {
  int period = 0;
  float strength = 0.f;
};

float AutoCorr24(const float* buf, int inverted_lag) {
  return Dot(buf + kMaxPitch24, buf + inverted_lag, kFrame20);
}

int PseudoOffset(float prev, float curr, float next) {
  if ((next - prev) > 0.7f * (curr - prev)) {
    return 1;
  }
  if ((prev - next) > 0.7f * (curr - next)) {
    return -1;
  }
  return 0;
}

int PseudoLag48(const float* buf, int lag) {
  int offset = 0;
  if (lag > 0 && lag < kMaxPitch24) {
    const int inv = kMaxPitch24 - lag;
    offset = PseudoOffset(AutoCorr24(buf, inv + 1), AutoCorr24(buf, inv),
                          AutoCorr24(buf, inv - 1));
  }
  return 2 * lag + offset;
}

int AlternativePeriod(int period, int multiplier, int divisor) {
  return (2 * multiplier * period + divisor) / (2 * divisor);
}

bool AlternativeStronger(PitchInfo last, PitchInfo initial, PitchInfo alt,
                         int divisor) {
  float lower = 0.f;
  if (std::abs(alt.period - last.period) <= 1) {
    lower = last.strength;
  } else if (std::abs(alt.period - last.period) == 2 &&
             initial.period > kInitialPitchPeriodThresholds[divisor - 2]) {
    lower = 0.5f * last.strength;
  }
  float threshold = std::max(0.3f, 0.7f * initial.strength - lower);
  if (alt.period < 3 * kMinPitch24) {
    threshold = std::max(0.4f, 0.85f * initial.strength - lower);
  } else if (alt.period < 2 * kMinPitch24) {
    threshold = std::max(0.5f, 0.9f * initial.strength - lower);
  }
  return alt.strength > threshold;
}

struct PitchEstimator {
  PitchInfo last{};

  int Estimate(const float* buf24) {
    float buf12[kBuf12];
    for (int i = 0; i < kBuf12; ++i) {
      buf12[i] = buf24[2 * i];
    }
    float ac12[kNumLags12];
    for (int k = 0; k < kNumLags12; ++k) {
      ac12[k] = Dot(buf12 + k, buf12 + kMaxPitch12, kFrame20_12);
    }

    struct Cand {
      int lag = 0;
      float num = -1.f;
      float den = 0.f;
      bool Stronger(const Cand& b) const {
        return num * b.den > b.num * den;
      }
    };
    float den = 1.f;
    for (int i = 0; i < kFrame20_12 + 1; ++i) {
      den += buf12[i] * buf12[i];
    }
    Cand best;
    Cand second;
    second.lag = 1;
    for (int lag = 0; lag < kNumLags12; ++lag) {
      if (ac12[lag] > 0.f) {
        Cand c{lag, ac12[lag] * ac12[lag], den};
        if (c.Stronger(second)) {
          if (c.Stronger(best)) {
            second = best;
            best = c;
          } else {
            second = c;
          }
        }
      }
      const float y_old = buf12[lag];
      const float y_new = buf12[lag + kFrame20_12];
      den -= y_old * y_old;
      den += y_new * y_new;
      den = std::max(0.f, den);
    }
    const int best24 = best.lag * 2;
    const int second24 = second.lag * 2;

    float y_energy[kRefineNumLags24];
    float yy = Dot(buf24, buf24, kFrame20);
    y_energy[0] = yy;
    for (int lag = 0; lag < kMaxPitch24; ++lag) {
      yy -= buf24[lag] * buf24[lag];
      yy += buf24[lag + kFrame20] * buf24[lag + kFrame20];
      yy = std::max(1.f, yy);
      y_energy[lag + 1] = yy;
    }

    auto inv_range = [](int lag) {
      struct R {
        int min;
        int max;
      };
      return R{std::max(lag - 2, 0), std::min(lag + 2, kInitialNumLags24 - 1)};
    };
    const bool swap = best24 > second24;
    auto r1 = inv_range(swap ? second24 : best24);
    auto r2 = inv_range(swap ? best24 : second24);
    float ac24[kInitialNumLags24];
    std::fill(ac24, ac24 + kInitialNumLags24, 0.f);
    int lags[20];
    int n_lags = 0;
    auto fill = [&](int lo, int hi) {
      for (int lag = lo; lag <= hi; ++lag) {
        ac24[lag] = AutoCorr24(buf24, lag);
        lags[n_lags++] = lag;
      }
    };
    if (r1.max + 1 >= r2.min) {
      fill(r1.min, r2.max);
    } else {
      fill(r1.min, r1.max);
      fill(r2.min, r2.max);
    }
    int best_inv = 0;
    float best_num = -1.f;
    float best_den = 0.f;
    for (int i = 0; i < n_lags; ++i) {
      const int lag = lags[i];
      if (ac24[lag] > 0.f) {
        const float num = ac24[lag] * ac24[lag];
        const float d = y_energy[lag];
        if (num * best_den > best_num * d) {
          best_inv = lag;
          best_num = num;
          best_den = d;
        }
      }
    }
    int pitch_lag_48 = best_inv * 2;
    if (best_inv > 0 && best_inv < kInitialNumLags24 - 1) {
      pitch_lag_48 +=
          PseudoOffset(ac24[best_inv + 1], ac24[best_inv], ac24[best_inv - 1]);
    }
    const int initial_period = kMaxPitch48 - pitch_lag_48;

    const float x_energy = y_energy[kMaxPitch24];
    auto strength_of = [x_energy](float xy, float ye) {
      return xy / std::sqrt(1.f + x_energy * ye);
    };
    struct Refined {
      int period;
      float strength;
      float xy;
      float ye;
    };
    Refined refined;
    refined.period = std::min(initial_period / 2, kMaxPitch24 - 1);
    refined.xy = AutoCorr24(buf24, kMaxPitch24 - refined.period);
    refined.ye = y_energy[kMaxPitch24 - refined.period];
    refined.strength = strength_of(refined.xy, refined.ye);
    const PitchInfo initial{refined.period, refined.strength};
    const PitchInfo prev{last.period / 2, last.strength};
    const int max_div = (2 * initial.period) / (2 * kMinPitch24 - 1);
    for (int div = 2; div <= max_div; ++div) {
      PitchInfo alt;
      alt.period = AlternativePeriod(initial.period, 1, div);
      int dual = AlternativePeriod(initial.period,
                                  kSubHarmonicMultipliers[div - 2], div);
      if (div == 2 && dual > kMaxPitch24) {
        dual = initial.period;
      }
      const float xy_p = AutoCorr24(buf24, kMaxPitch24 - alt.period);
      const float xy_s = AutoCorr24(buf24, kMaxPitch24 - dual);
      const float xy = 0.5f * (xy_p + xy_s);
      const float ye = 0.5f * (y_energy[kMaxPitch24 - alt.period] +
                               y_energy[kMaxPitch24 - dual]);
      alt.strength = strength_of(xy, ye);
      if (AlternativeStronger(prev, initial, alt, div)) {
        refined = {alt.period, alt.strength, xy, ye};
      }
    }
    refined.xy = std::max(0.f, refined.xy);
    float final_strength =
        (refined.ye <= refined.xy) ? 1.f : refined.xy / (refined.ye + 1.f);
    final_strength = std::min(refined.strength, final_strength);
    const int final_period =
        std::max(kMinPitch48, PseudoLag48(buf24, refined.period));
    last = {final_period, final_strength};
    return final_period;
  }
};

void BandCorr(const float* x, const float* y, float* out) {
  int k = 0;
  out[0] = 0.f;
  for (int band = 0; band < kOpusBands - 1; ++band) {
    out[band + 1] = 0.f;
    for (int j = 0; j < kOpusBins[band]; ++j) {
      const float v = x[2 * k] * y[2 * k] + x[2 * k + 1] * y[2 * k + 1];
      const float tmp = kOpusWeights[k] * v;
      out[band] += v - tmp;
      out[band + 1] += tmp;
      ++k;
    }
  }
  out[0] *= 2.f;
}

struct Spectral {
  float window[kFrame20] = {};
  float dct[kNumBands * kNumBands] = {};
  float cep[kCepHist][kNumBands] = {};
  int cep_tail = 0;
  float diff[(kCepHist - 1) * (kCepHist - 1)] = {};
  float ref_fft[kFrame20] = {};
  float lag_fft[kFrame20] = {};
  float ref_energy[kOpusBands] = {};
  float lag_energy[kOpusBands] = {};
  float cross[kOpusBands] = {};

  Spectral() {
    constexpr int half = kFrame20 / 2;
    for (int i = 0; i < half; ++i) {
      const double s = std::sin(0.5 * kPi * (i + 0.5) / half);
      const float w =
          static_cast<float>(std::sin(0.5 * kPi * s * s) / kFrame20);
      window[i] = w;
      window[kFrame20 - 1 - i] = w;
    }
    const double k = std::sqrt(0.5);
    for (int i = 0; i < kNumBands; ++i) {
      for (int j = 0; j < kNumBands; ++j) {
        dct[i * kNumBands + j] =
            static_cast<float>(std::cos((i + 0.5) * j * kPi / kNumBands));
      }
      dct[i * kNumBands] *= static_cast<float>(k);
    }
  }

  void Reset() {
    std::memset(cep, 0, sizeof(cep));
    cep_tail = 0;
    std::memset(diff, 0, sizeof(diff));
  }

  void PushCep(const float* values) {
    std::memcpy(cep[cep_tail], values, sizeof(float) * kNumBands);
    cep_tail = (cep_tail + 1) % kCepHist;
  }

  const float* Cep(int delay) const {
    int offset = cep_tail - 1 - delay;
    if (offset < 0) {
      offset += kCepHist;
    }
    return cep[offset];
  }

  void PushDiff(const float distances[kCepHist - 1]) {
    constexpr int s = kCepHist;
    std::memmove(diff, diff + s, ((s - 1) * (s - 1) - s) * sizeof(float));
    for (int i = 0; i < s - 1; ++i) {
      const int index = (s - 1 - i) * (s - 1) - 1;
      diff[index] = distances[i];
    }
  }

  float DiffAt(int delay1, int delay2) const {
    int row = kCepHist - 1 - delay1;
    int col = kCepHist - 1 - delay2;
    if (row > col) {
      std::swap(row, col);
    }
    return diff[row * (kCepHist - 1) + (col - 1)];
  }

  void Forward(const float* frame, float* spec) {
    float windowed[kFrame20];
    for (int i = 0; i < kFrame20; ++i) {
      windowed[i] = frame[i] * window[i];
    }
    RealDftOrdered(windowed, spec);
  }

  // Returns true when the reference frame is below the silence gate.
  bool Compute(const float* reference, const float* lagged, float* feature) {
    Forward(reference, ref_fft);
    BandCorr(ref_fft, ref_fft, ref_energy);
    float tot = 0.f;
    for (float e : ref_energy) {
      tot += e;
    }
    if (tot < kSilenceThreshold) {
      return true;
    }
    Forward(lagged, lag_fft);
    BandCorr(lag_fft, lag_fft, lag_energy);

    float log_e[kNumBands];
    float log_max = -2.f;
    float follow = -2.f;
    auto smooth = [&](float x) {
      x = std::max(log_max - 7.f, std::max(follow - 1.5f, x));
      log_max = std::max(log_max, x);
      follow = std::max(follow - 1.5f, x);
      return x;
    };
    for (int i = 0; i < kOpusBands; ++i) {
      log_e[i] = smooth(std::log10(1e-2f + ref_energy[i]));
    }
    for (int i = kOpusBands; i < kNumBands; ++i) {
      log_e[i] = smooth(-2.f);
    }
    float cepstrum[kNumBands];
    constexpr float kDctScale = 0.301511345f;
    for (int i = 0; i < kNumBands; ++i) {
      float s = 0.f;
      for (int j = 0; j < kNumBands; ++j) {
        s += log_e[j] * dct[j * kNumBands + i];
      }
      cepstrum[i] = s * kDctScale;
    }
    cepstrum[0] -= 12.f;
    cepstrum[1] -= 4.f;

    // Push first, then read delays 1..7. That is the order in
    // SpectralFeaturesExtractor::CheckSilenceComputeFeatures.
    PushCep(cepstrum);
    float distances[kCepHist - 1];
    for (int i = 0; i < kCepHist - 1; ++i) {
      const float* old = Cep(i + 1);
      float d = 0.f;
      for (int k = 0; k < kNumBands; ++k) {
        const float c = cepstrum[k] - old[k];
        d += c * c;
      }
      distances[i] = d;
    }
    PushDiff(distances);

    std::copy(cepstrum + kNumLower, cepstrum + kNumBands, feature + kNumLower);
    const float* curr = Cep(0);
    const float* prev1 = Cep(1);
    const float* prev2 = Cep(2);
    for (int i = 0; i < kNumLower; ++i) {
      feature[i] = curr[i] + prev1[i] + prev2[i];
      feature[kNumBands + i] = curr[i] - prev2[i];
      feature[kNumBands + kNumLower + i] = curr[i] - 2.f * prev1[i] + prev2[i];
    }
    BandCorr(ref_fft, lag_fft, cross);
    for (int i = 0; i < kOpusBands; ++i) {
      cross[i] /= std::sqrt(0.001f + ref_energy[i] * lag_energy[i]);
    }
    float corr_cep[kNumLower];
    for (int i = 0; i < kNumLower; ++i) {
      float s = 0.f;
      for (int j = 0; j < kOpusBands; ++j) {
        s += cross[j] * dct[j * kNumBands + i];
      }
      corr_cep[i] = s * kDctScale;
    }
    corr_cep[0] -= 1.3f;
    corr_cep[1] -= 0.9f;
    std::copy(corr_cep, corr_cep + kNumLower,
              feature + kNumBands + 2 * kNumLower);
    float variability = 0.f;
    for (int d1 = 0; d1 < kCepHist; ++d1) {
      float min_dist = 1e30f;
      for (int d2 = 0; d2 < kCepHist; ++d2) {
        if (d1 == d2) {
          continue;
        }
        min_dist = std::min(min_dist, DiffAt(d1, d2));
      }
      variability += min_dist;
    }
    feature[kFeat - 1] = variability / kCepHist - 2.1f;
    return false;
  }
};

struct Rnn {
  std::vector<float> in_bias;
  std::vector<float> in_w;
  std::vector<float> gru_bias;
  std::vector<float> gru_w;
  std::vector<float> gru_rw;
  std::vector<float> out_bias;
  std::vector<float> out_w;
  float h[24] = {};
  float fc1[24] = {};

  Rnn()
      : in_bias(ScaleBias(rnnoise::kInputDenseBias,
                          rnnoise::kInputLayerOutputSize)),
        in_w(PreprocessFc(rnnoise::kInputDenseWeights,
                          rnnoise::kInputLayerWeights,
                          rnnoise::kInputLayerOutputSize)),
        gru_bias(PreprocessGru(rnnoise::kHiddenGruBias,
                               rnnoise::kHiddenLayerBiases,
                               rnnoise::kHiddenLayerOutputSize)),
        gru_w(PreprocessGru(rnnoise::kHiddenGruWeights,
                            rnnoise::kHiddenLayerWeights,
                            rnnoise::kHiddenLayerOutputSize)),
        gru_rw(PreprocessGru(rnnoise::kHiddenGruRecurrentWeights,
                             rnnoise::kHiddenLayerWeights,
                             rnnoise::kHiddenLayerOutputSize)),
        out_bias(ScaleBias(rnnoise::kOutputDenseBias,
                           rnnoise::kOutputLayerOutputSize)),
        out_w(PreprocessFc(rnnoise::kOutputDenseWeights,
                           rnnoise::kOutputLayerWeights,
                           rnnoise::kOutputLayerOutputSize)) {}

  void Reset() { std::fill(h, h + 24, 0.f); }

  float Compute(const float* feat, bool silence) {
    if (silence) {
      Reset();
      return 0.f;
    }
    constexpr int kIn = 42;
    constexpr int kHid = 24;
    for (int o = 0; o < kHid; ++o) {
      fc1[o] = rnnoise::TansigApproximated(
          in_bias[o] + Dot(feat, in_w.data() + o * kIn, kIn));
    }
    float update[24];
    float reset[24];
    const int stride_w = kHid * kHid;
    auto gate = [&](int g, float* dst) {
      const float* bias = gru_bias.data() + g * kHid;
      const float* w = gru_w.data() + g * stride_w;
      const float* rw = gru_rw.data() + g * stride_w;
      for (int o = 0; o < kHid; ++o) {
        const float x = bias[o] + Dot(fc1, w + o * kHid, kHid) +
                        Dot(h, rw + o * kHid, kHid);
        dst[o] = rnnoise::SigmoidApproximated(x);
      }
    };
    gate(0, update);
    gate(1, reset);
    float reset_h[24];
    for (int o = 0; o < kHid; ++o) {
      reset_h[o] = h[o] * reset[o];
    }
    const float* bias = gru_bias.data() + 2 * kHid;
    const float* w = gru_w.data() + 2 * stride_w;
    const float* rw = gru_rw.data() + 2 * stride_w;
    for (int o = 0; o < kHid; ++o) {
      const float x = bias[o] + Dot(fc1, w + o * kHid, kHid) +
                      Dot(reset_h, rw + o * kHid, kHid);
      h[o] = update[o] * h[o] + (1.f - update[o]) * std::max(0.f, x);
    }
    const float y =
        out_bias[0] + Dot(h, out_w.data(), kHid);
    return rnnoise::SigmoidApproximated(y);
  }
};

struct Activity {
  int adjacent = 0;
  int frames_to_gain = kAdjacent;
  float level = kInitialSpeechLevelDbfs;
  float pre_num = kInitialSpeechLevelDbfs;
  float pre_den = 1.f;
  float pre_ms = kLevelConfidenceMs;
  float rel_num = kInitialSpeechLevelDbfs;
  float rel_den = 1.f;
  float rel_ms = kLevelConfidenceMs;
  int vol_frames = 0;
  int vol_speech = 0;
  bool first_volume_frame = true;

  void Reset() { *this = Activity{}; }

  void Update(Agc2VadFrame& out) {
    const float p = out.speech_probability;
    const float rms = out.rms_dbfs;
    if (p < kVadConfidence) {
      if (adjacent >= kAdjacent) {
        rel_num = pre_num;
        rel_den = pre_den;
        rel_ms = pre_ms;
      } else if (adjacent > 0) {
        pre_num = rel_num;
        pre_den = rel_den;
        pre_ms = rel_ms;
      }
      adjacent = 0;
      frames_to_gain = kAdjacent;
    } else {
      ++adjacent;
      if (pre_ms != 0.f) {
        pre_ms -= 10.f;
      }
      const float leak = (pre_ms == 0.f) ? kLevelLeak : 1.f;
      pre_num = pre_num * leak + rms * p;
      pre_den = pre_den * leak + p;
      if (adjacent >= kAdjacent) {
        level = std::clamp(pre_num / pre_den, -90.f, 30.f);
        out.speech_level_updated = true;
      }
      if (frames_to_gain > 0) {
        --frames_to_gain;
      }
    }
    out.confident_speech = p >= kVadConfidence;
    out.adjacent_speech_frames = adjacent;
    out.gain_increase_allowed = frames_to_gain == 0;
    out.speech_level_dbfs = level;

    if (p >= kVolumeSpeechThreshold) {
      ++vol_speech;
    }
    ++vol_frames;
    out.volume_window_frames = vol_frames;
    out.volume_speech_frames = vol_speech;
    if (vol_frames >= kVolumeWindow) {
      const float ratio =
          static_cast<float>(vol_speech) / static_cast<float>(kVolumeWindow);
      out.volume_window_closed = true;
      out.volume_speech_ratio = ratio;
      out.volume_update_allowed =
          !first_volume_frame && ratio >= kVolumeRatioThreshold;
      vol_frames = 0;
      vol_speech = 0;
    }
    first_volume_frame = false;
  }
};

}  // namespace

struct Agc2RnnVad::Impl {
  float pitch_buf[kBuf24] = {};
  PitchEstimator pitch;
  Spectral spectral;
  Rnn rnn;
  Activity activity;
  int time_to_reset = kVadResetFrames;

  void Reset() {
    std::fill(pitch_buf, pitch_buf + kBuf24, 0.f);
    pitch = PitchEstimator{};
    spectral.Reset();
    rnn.Reset();
    activity.Reset();
    time_to_reset = kVadResetFrames;
  }
};

Agc2RnnVad::Agc2RnnVad() : impl_(new Impl) {}

Agc2RnnVad::~Agc2RnnVad() { delete impl_; }

void Agc2RnnVad::Reset() { impl_->Reset(); }

float Agc2RnnVad::ComputeProbability(const float* features, bool is_silence) {
  return impl_->rnn.Compute(features, is_silence);
}

Agc2VadFrame Agc2RnnVad::ProcessFrame(const float* samples) {
  Agc2VadFrame out;
  bool rnn_reset = false;
  if (--impl_->time_to_reset <= 0) {
    impl_->rnn.Reset();
    impl_->time_to_reset = kVadResetFrames;
    rnn_reset = true;
  }
  std::memmove(impl_->pitch_buf, impl_->pitch_buf + kFrame10,
               (kBuf24 - kFrame10) * sizeof(float));
  std::memcpy(impl_->pitch_buf + kBuf24 - kFrame10, samples,
              kFrame10 * sizeof(float));

  float lpc[kNumLpc];
  ComputeLpc(impl_->pitch_buf, kBuf24, lpc);
  float residual[kBuf24];
  LpResidual(lpc, impl_->pitch_buf, residual, kBuf24);
  const int period = impl_->pitch.Estimate(residual);
  float feature[kFeat] = {};
  feature[kFeat - 2] = 0.01f * static_cast<float>(period - 300);
  const int lag24 = period / 2;
  const float* lagged = impl_->pitch_buf + (kMaxPitch24 - lag24);
  const float* reference = impl_->pitch_buf + (kBuf24 - kFrame20);
  const bool silence = impl_->spectral.Compute(reference, lagged, feature);

  double acc = 0.0;
  for (int i = 0; i < kFrame10; ++i) {
    acc += static_cast<double>(samples[i]) * samples[i];
  }
  const float rms = static_cast<float>(std::sqrt(acc / kFrame10));

  out.speech_probability = impl_->rnn.Compute(feature, silence);
  out.is_silence = silence;
  out.rnn_reset = rnn_reset || silence;
  out.pitch_period_48khz = period;
  out.pitch_hz =
      period > 0 ? static_cast<float>(48000.0 / period) : 0.f;
  out.pitch_feature = feature[kFeat - 2];
  out.rms_dbfs = FloatS16ToDbfs(rms);
  impl_->activity.Update(out);
  return out;
}

}  // namespace webrtc_3a

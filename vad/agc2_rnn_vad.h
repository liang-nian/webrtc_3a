/*
 * AGC2 RNN-VAD used by the WebRTC 3A capture path.
 *
 * Transcribed from WebRTC src
 * eeb7e6beb2e20c5a273922fdf22b75a75cc28ad5 (2026-11-07):
 *   modules/audio_processing/agc2/rnn_vad/
 *   modules/audio_processing/agc2/vad_wrapper.cc
 *   modules/audio_processing/agc2/speech_level_estimator_impl.cc
 *   modules/audio_processing/agc2/adaptive_digital_gain_controller.cc
 *   modules/audio_processing/agc2/input_volume_controller.cc
 *   modules/audio_processing/agc2/agc2_common.h
 *
 * Differences from that tree are listed in doc/vad.md. The callable entry
 * is 24 kHz, 10 ms, S16-scaled float. It does not resample.
 */
#ifndef WEBRTC_3A_AGC2_RNN_VAD_H_
#define WEBRTC_3A_AGC2_RNN_VAD_H_

namespace webrtc_3a {

constexpr int kAgc2VadSampleRateHz = 24000;
constexpr int kAgc2VadFrameSamples = 240;  // 10 ms at 24 kHz
constexpr int kAgc2VadFeatureSize = 42;

// Per-frame record. Speech-level and input-volume fields are the AGC2
// decisions that consume the probability. They are not part of the RNN.
struct Agc2VadFrame {
  float speech_probability = 0.f;
  bool is_silence = true;
  bool rnn_reset = false;
  int pitch_period_48khz = 0;
  float pitch_hz = 0.f;
  float pitch_feature = 0.f;
  float rms_dbfs = -90.30899869919436f;
  bool confident_speech = false;
  int adjacent_speech_frames = 0;
  bool gain_increase_allowed = false;
  float speech_level_dbfs = -40.f;
  bool speech_level_updated = false;
  int volume_window_frames = 0;
  int volume_speech_frames = 0;
  bool volume_window_closed = false;
  float volume_speech_ratio = -1.f;
  bool volume_update_allowed = false;
};

// One 10 ms frame in, one probability and the AGC2 activity state out.
// Samples use the S16 float scale (a full-scale sine is about +/-32768),
// which is what the upstream feature extractor and the silence gate expect.
class Agc2RnnVad {
 public:
  Agc2RnnVad();
  ~Agc2RnnVad();
  Agc2RnnVad(const Agc2RnnVad&) = delete;
  Agc2RnnVad& operator=(const Agc2RnnVad&) = delete;

  void Reset();
  Agc2VadFrame ProcessFrame(const float* samples);

  // Feeds the 42-d feature vector straight into the network. Used to
  // replay the upstream RnnVad reset/silence checks.
  float ComputeProbability(const float* features, bool is_silence);

 private:
  struct Impl;
  Impl* impl_;
};

}  // namespace webrtc_3a

#endif

/*
 * Circular speech-probability buffer from
 * modules/audio_processing/agc2/speech_probability_buffer.cc
 * at WebRTC src eeb7e6beb2e20c5a273922fdf22b75a75cc28ad5.
 *
 * The class is compiled into the input_volume_controller target and has
 * its own unit test. GainController2::Process and
 * MonoInputVolumeController::Process do not call it in that revision.
 * Kept here so the transient rule can be checked against that test.
 * The capture-path decision is Agc2RnnVad's 0.95 / 12-frame gate.
 */
#ifndef WEBRTC_3A_SPEECH_PROBABILITY_BUFFER_H_
#define WEBRTC_3A_SPEECH_PROBABILITY_BUFFER_H_

namespace webrtc_3a {

class SpeechProbabilityBuffer {
 public:
  explicit SpeechProbabilityBuffer(float low_probability_threshold);
  void Update(float probability);
  void Reset();
  bool IsActiveSegment() const;
  float SumProbabilities() const { return sum_probabilities_; }

 private:
  void RemoveTransient();

  float low_probability_threshold_;
  float sum_probabilities_ = 0.f;
  float probabilities_[100] = {};
  int buffer_index_ = 0;
  bool buffer_is_full_ = false;
  int num_high_probability_observations_ = 0;
};

}  // namespace webrtc_3a

#endif

#include "speech_probability_buffer.h"

#include <cassert>

namespace webrtc_3a {
namespace {

constexpr float kActivityThreshold = 0.9f;
constexpr int kNumAnalysisFrames = 100;
constexpr int kTransientWidthThreshold = 7;

}  // namespace

SpeechProbabilityBuffer::SpeechProbabilityBuffer(float low_probability_threshold)
    : low_probability_threshold_(low_probability_threshold) {
  assert(low_probability_threshold >= 0.f);
  assert(low_probability_threshold <= 1.f);
}

void SpeechProbabilityBuffer::Update(float probability) {
  assert(probability >= 0.f && probability <= 1.f);
  if (buffer_is_full_) {
    sum_probabilities_ -= probabilities_[buffer_index_];
  }
  if (probability <= low_probability_threshold_) {
    probability = 0.f;
    if (num_high_probability_observations_ <= kTransientWidthThreshold) {
      RemoveTransient();
    }
    num_high_probability_observations_ = 0;
  } else if (num_high_probability_observations_ <= kTransientWidthThreshold) {
    ++num_high_probability_observations_;
  }
  probabilities_[buffer_index_] = probability;
  sum_probabilities_ += probability;
  if (++buffer_index_ >= kNumAnalysisFrames) {
    buffer_index_ = 0;
    buffer_is_full_ = true;
  }
}

void SpeechProbabilityBuffer::RemoveTransient() {
  assert(num_high_probability_observations_ <= kTransientWidthThreshold);
  int index =
      (buffer_index_ > 0) ? (buffer_index_ - 1) : (kNumAnalysisFrames - 1);
  while (num_high_probability_observations_-- > 0) {
    sum_probabilities_ -= probabilities_[index];
    probabilities_[index] = 0.f;
    index = (index > 0) ? (index - 1) : (kNumAnalysisFrames - 1);
  }
}

bool SpeechProbabilityBuffer::IsActiveSegment() const {
  if (!buffer_is_full_) {
    return false;
  }
  return sum_probabilities_ >= kActivityThreshold * kNumAnalysisFrames;
}

void SpeechProbabilityBuffer::Reset() {
  sum_probabilities_ = 0.f;
  buffer_index_ = 0;
  buffer_is_full_ = false;
  num_high_probability_observations_ = 0;
  for (float& p : probabilities_) {
    p = 0.f;
  }
}

}  // namespace webrtc_3a

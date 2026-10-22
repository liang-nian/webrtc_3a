/*
 * Declarations for the two-band QMF extracted from
 * webrtc/common_audio/signal_processing/splitting_filter.c.
 * Each filter_state array holds 6 floats (3 cascaded allpasses × 2).
 */
#ifndef WEBRTC_3A_QMF_SPLITTING_FILTER_H_
#define WEBRTC_3A_QMF_SPLITTING_FILTER_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void WebRtcSpl_AnalysisQMF(const float* in_data,
                           size_t in_data_length,
                           float* low_band,
                           float* high_band,
                           float* filter_state1,
                           float* filter_state2);

void WebRtcSpl_SynthesisQMF(const float* low_band,
                            const float* high_band,
                            size_t band_length,
                            float* out_data,
                            float* filter_state1,
                            float* filter_state2);

#ifdef __cplusplus
}
#endif

#endif

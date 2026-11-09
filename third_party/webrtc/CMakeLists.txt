# Extracted WebRTC VAD sources. WEBRTC_TREE is the src root (default: the
# committed snapshot). The reference build points WEBRTC_TREE at a fresh
# download. Algorithm files are compiled unchanged.

if(NOT WEBRTC_TREE)
  set(WEBRTC_TREE ${CMAKE_CURRENT_SOURCE_DIR}/src)
endif()
set(WEBRTC_SHIM ${CMAKE_CURRENT_SOURCE_DIR}/shim)

add_library(webrtc_vad STATIC
  ${WEBRTC_TREE}/common_audio/vad/webrtc_vad.c
  ${WEBRTC_TREE}/common_audio/vad/vad_core.c
  ${WEBRTC_TREE}/common_audio/vad/vad_filterbank.c
  ${WEBRTC_TREE}/common_audio/vad/vad_gmm.c
  ${WEBRTC_TREE}/common_audio/vad/vad_sp.c
  ${WEBRTC_TREE}/common_audio/signal_processing/energy.c
  ${WEBRTC_TREE}/common_audio/signal_processing/get_scaling_square.c
  ${WEBRTC_TREE}/common_audio/signal_processing/resample_fractional.c
  ${WEBRTC_TREE}/common_audio/signal_processing/division_operations.c
  ${WEBRTC_TREE}/common_audio/signal_processing/spl_inl.c
  ${WEBRTC_TREE}/common_audio/signal_processing/resample_48khz.c
  ${WEBRTC_TREE}/common_audio/signal_processing/resample_by_2_internal.c
  ${WEBRTC_TREE}/common_audio/third_party/spl_sqrt_floor/spl_sqrt_floor.c
  ${WEBRTC_TREE}/common_audio/audio_util.cc
  ${WEBRTC_TREE}/common_audio/resampler/push_resampler.cc
  ${WEBRTC_TREE}/common_audio/resampler/push_sinc_resampler.cc
  ${WEBRTC_TREE}/common_audio/resampler/sinc_resampler.cc
  ${WEBRTC_TREE}/common_audio/resampler/sinc_resampler_sse.cc
  ${WEBRTC_TREE}/common_audio/resampler/sinc_resampler_avx2.cc
  ${WEBRTC_TREE}/modules/audio_processing/agc2/biquad_filter.cc
  ${WEBRTC_TREE}/modules/audio_processing/agc2/cpu_features.cc
  ${WEBRTC_TREE}/modules/audio_processing/agc2/rnn_vad/features_extraction.cc
  ${WEBRTC_TREE}/modules/audio_processing/agc2/rnn_vad/rnn.cc
  ${WEBRTC_TREE}/modules/audio_processing/agc2/rnn_vad/rnn_fc.cc
  ${WEBRTC_TREE}/modules/audio_processing/agc2/rnn_vad/rnn_gru.cc
  ${WEBRTC_TREE}/modules/audio_processing/agc2/rnn_vad/spectral_features.cc
  ${WEBRTC_TREE}/modules/audio_processing/agc2/rnn_vad/spectral_features_internal.cc
  ${WEBRTC_TREE}/modules/audio_processing/agc2/rnn_vad/pitch_search.cc
  ${WEBRTC_TREE}/modules/audio_processing/agc2/rnn_vad/pitch_search_internal.cc
  ${WEBRTC_TREE}/modules/audio_processing/agc2/rnn_vad/lp_residual.cc
  ${WEBRTC_TREE}/modules/audio_processing/agc2/rnn_vad/auto_correlation.cc
  ${WEBRTC_TREE}/modules/audio_processing/agc2/rnn_vad/vector_math_avx2.cc
  ${WEBRTC_TREE}/modules/audio_processing/agc2/vad_wrapper.cc
  ${WEBRTC_TREE}/modules/audio_processing/utility/pffft_wrapper.cc
  ${WEBRTC_TREE}/third_party/pffft/src/pffft.c
  ${WEBRTC_TREE}/third_party/rnnoise/src/rnn_vad_weights.cc
  ${WEBRTC_SHIM}/aligned_malloc.cc
  ${WEBRTC_SHIM}/cpu_info.cc
)
target_include_directories(webrtc_vad PUBLIC
  ${WEBRTC_SHIM}
  ${WEBRTC_TREE}
  ${WEBRTC_TREE}/third_party/pffft/src
)
target_compile_features(webrtc_vad PUBLIC cxx_std_20)
target_compile_options(webrtc_vad PRIVATE
  $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wno-unused-function -include cstring>
  $<$<COMPILE_LANGUAGE:C>:-Wall -Wno-unused-function>
)
set_source_files_properties(
  ${WEBRTC_TREE}/common_audio/resampler/sinc_resampler_avx2.cc
  ${WEBRTC_TREE}/modules/audio_processing/agc2/rnn_vad/vector_math_avx2.cc
  PROPERTIES COMPILE_OPTIONS "-mavx2;-mfma")

function(webrtc_vad_tool target_name src_file output_name)
  add_executable(${target_name} ${src_file})
  target_link_libraries(${target_name} PRIVATE webrtc_vad)
  target_compile_features(${target_name} PRIVATE cxx_std_20)
  set_target_properties(${target_name} PROPERTIES OUTPUT_NAME ${output_name})
endfunction()

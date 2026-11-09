#!/usr/bin/env python3
"""Download the pinned WebRTC / Chromium files used as the VAD reference.

The committed tree is third_party/webrtc/src. A second copy for the
reference build is written to output/webrtc-ref/src when --ref is set.
Algorithm files are not modified by this script.
"""

import argparse
import base64
import hashlib
import pathlib
import time
import urllib.request

WEBRTC = "eeb7e6beb2e20c5a273922fdf22b75a75cc28ad5"
CHROMIUM_TP = "63d42f6e6a54c9463fcb599001ca81ff2fd3dc8e"

WEBRTC_FILES = [
    "common_audio/vad/include/webrtc_vad.h",
    "common_audio/vad/vad_core.h",
    "common_audio/vad/vad_core.c",
    "common_audio/vad/vad_filterbank.h",
    "common_audio/vad/vad_filterbank.c",
    "common_audio/vad/vad_gmm.h",
    "common_audio/vad/vad_gmm.c",
    "common_audio/vad/vad_sp.h",
    "common_audio/vad/vad_sp.c",
    "common_audio/vad/webrtc_vad.c",
    "common_audio/signal_processing/include/signal_processing_library.h",
    "common_audio/signal_processing/include/spl_inl.h",
    "common_audio/signal_processing/resample_48khz.c",
    "common_audio/signal_processing/resample_by_2_internal.h",
    "common_audio/signal_processing/resample_by_2_internal.c",
    "common_audio/signal_processing/energy.c",
    "common_audio/signal_processing/get_scaling_square.c",
    "common_audio/signal_processing/resample_fractional.c",
    "common_audio/signal_processing/division_operations.c",
    "common_audio/signal_processing/spl_inl.c",
    "common_audio/third_party/spl_sqrt_floor/spl_sqrt_floor.h",
    "common_audio/third_party/spl_sqrt_floor/spl_sqrt_floor.c",
    "common_audio/include/audio_util.h",
    "common_audio/audio_util.cc",
    "common_audio/resampler/include/push_resampler.h",
    "common_audio/resampler/push_resampler.cc",
    "common_audio/resampler/push_sinc_resampler.h",
    "common_audio/resampler/push_sinc_resampler.cc",
    "common_audio/resampler/sinc_resampler.h",
    "common_audio/resampler/sinc_resampler.cc",
    "common_audio/resampler/sinc_resampler_avx2.cc",
    "common_audio/resampler/sinc_resampler_sse.cc",
    "api/audio/audio_view.h",
    "modules/audio_processing/utility/pffft_wrapper.h",
    "modules/audio_processing/utility/pffft_wrapper.cc",
    "modules/audio_processing/agc2/biquad_filter.h",
    "modules/audio_processing/agc2/biquad_filter.cc",
    "modules/audio_processing/agc2/cpu_features.h",
    "modules/audio_processing/agc2/cpu_features.cc",
    "modules/audio_processing/agc2/agc2_common.h",
    "modules/audio_processing/agc2/rnn_vad/common.h",
    "modules/audio_processing/agc2/rnn_vad/features_extraction.h",
    "modules/audio_processing/agc2/rnn_vad/features_extraction.cc",
    "modules/audio_processing/agc2/rnn_vad/rnn.h",
    "modules/audio_processing/agc2/rnn_vad/rnn.cc",
    "modules/audio_processing/agc2/rnn_vad/rnn_fc.h",
    "modules/audio_processing/agc2/rnn_vad/rnn_fc.cc",
    "modules/audio_processing/agc2/rnn_vad/rnn_gru.h",
    "modules/audio_processing/agc2/rnn_vad/rnn_gru.cc",
    "modules/audio_processing/agc2/rnn_vad/spectral_features.h",
    "modules/audio_processing/agc2/rnn_vad/spectral_features.cc",
    "modules/audio_processing/agc2/rnn_vad/spectral_features_internal.h",
    "modules/audio_processing/agc2/rnn_vad/spectral_features_internal.cc",
    "modules/audio_processing/agc2/rnn_vad/pitch_search.h",
    "modules/audio_processing/agc2/rnn_vad/pitch_search.cc",
    "modules/audio_processing/agc2/rnn_vad/pitch_search_internal.h",
    "modules/audio_processing/agc2/rnn_vad/pitch_search_internal.cc",
    "modules/audio_processing/agc2/rnn_vad/lp_residual.h",
    "modules/audio_processing/agc2/rnn_vad/lp_residual.cc",
    "modules/audio_processing/agc2/rnn_vad/auto_correlation.h",
    "modules/audio_processing/agc2/rnn_vad/auto_correlation.cc",
    "modules/audio_processing/agc2/rnn_vad/ring_buffer.h",
    "modules/audio_processing/agc2/rnn_vad/sequence_buffer.h",
    "modules/audio_processing/agc2/rnn_vad/symmetric_matrix_buffer.h",
    "modules/audio_processing/agc2/rnn_vad/vector_math.h",
    "modules/audio_processing/agc2/rnn_vad/vector_math_avx2.cc",
    "modules/audio_processing/agc2/vad_wrapper.h",
    "modules/audio_processing/agc2/vad_wrapper.cc",
]

CHROMIUM_FILES = [
    ("pffft/src/pffft.h", "third_party/pffft/src/pffft.h"),
    ("pffft/src/pffft.c", "third_party/pffft/src/pffft.c"),
    ("pffft/LICENSE", "third_party/pffft/LICENSE"),
    ("pffft/README.chromium", "third_party/pffft/README.chromium"),
    ("rnnoise/src/rnn_activations.h", "third_party/rnnoise/src/rnn_activations.h"),
    ("rnnoise/src/rnn_vad_weights.h", "third_party/rnnoise/src/rnn_vad_weights.h"),
    ("rnnoise/src/rnn_vad_weights.cc", "third_party/rnnoise/src/rnn_vad_weights.cc"),
    ("rnnoise/README.chromium", "third_party/rnnoise/README.chromium"),
]


def download(url: str) -> bytes:
    last = None
    for i in range(5):
        try:
            req = urllib.request.Request(url, headers={"User-Agent": "webrtc-3a-study"})
            with urllib.request.urlopen(req, timeout=90) as r:
                data = r.read()
            if url.endswith("format=TEXT"):
                data = base64.b64decode(data)
            return data
        except Exception as exc:  # noqa: BLE001
            last = exc
            time.sleep(1.5 * (i + 1))
    raise RuntimeError(f"failed {url}: {last}")


def put(root: pathlib.Path, rel: str, data: bytes) -> None:
    path = root / rel
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)
    print(f"{len(data):8d}  {hashlib.sha256(data).hexdigest()[:12]}  {rel}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--dest", type=pathlib.Path, required=True)
    args = parser.parse_args()
    root = args.dest
    wbase = f"https://webrtc.googlesource.com/src/+/{WEBRTC}"
    cbase = f"https://chromium.googlesource.com/chromium/src/third_party/+/{CHROMIUM_TP}"
    for rel in WEBRTC_FILES:
        put(root, rel, download(wbase + "/" + rel + "?format=TEXT"))
    for src, dest in CHROMIUM_FILES:
        put(root, dest, download(cbase + "/" + src + "?format=TEXT"))
    (root / "SOURCE_PIN.txt").write_text(
        "webrtc_src " + WEBRTC + "\nchromium_third_party " + CHROMIUM_TP + "\n"
    )
    print("wrote", root)


if __name__ == "__main__":
    main()

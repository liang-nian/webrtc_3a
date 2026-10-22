#!/usr/bin/env python3
"""Build qmf_chirp, run the linear-chirp empty reconstruction, plot the STFT."""

import argparse
import subprocess
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

plt.rcParams["font.sans-serif"] = ["STHeiti", "Heiti SC", "Arial Unicode MS"]
plt.rcParams["axes.unicode_minus"] = False

FS = 32000
N_FFT = 1024
HOP = 256
VMIN = -80.0
VMAX = 0.0
FMAX = 16000.0


def stft_db(x: np.ndarray) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    window = np.hanning(N_FFT)
    n_frames = 1 + (len(x) - N_FFT) // HOP
    spec = np.empty((N_FFT // 2 + 1, n_frames), dtype=np.float64)
    for i in range(n_frames):
        frame = x[i * HOP : i * HOP + N_FFT] * window
        spec[:, i] = np.abs(np.fft.rfft(frame, n=N_FFT))
    spec_db = 20.0 * np.log10(np.maximum(spec, 1e-12))
    freqs = np.fft.rfftfreq(N_FFT, d=1.0 / FS)
    times = (np.arange(n_frames) * HOP + N_FFT / 2.0) / FS
    return spec_db, freqs, times


def build_and_run(root: Path, out_dir: Path) -> None:
    build_dir = root / "output" / "qmf"
    subprocess.check_call(
        [
            "cmake",
            "-S",
            str(root),
            "-B",
            str(build_dir),
            "-DCMAKE_BUILD_TYPE=Release",
        ]
    )
    subprocess.check_call(
        ["cmake", "--build", str(build_dir), "--target", "qmf_chirp"]
    )
    out_dir.mkdir(parents=True, exist_ok=True)
    subprocess.check_call([str(build_dir / "qmf_chirp"), str(out_dir)])


def main() -> None:
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--out",
        type=Path,
        default=root / "doc" / "image" / "qmf_fig_chirp_stft.png",
    )
    args = parser.parse_args()
    in_dir = root / "output" / "sim" / "qmf"
    build_and_run(root, in_dir)

    x = np.fromfile(in_dir / "in.f32", dtype=np.float32).astype(np.float64)
    y = np.fromfile(in_dir / "out.f32", dtype=np.float32).astype(np.float64)
    if x.shape != y.shape or len(x) < N_FFT:
        raise SystemExit(f"bad lengths in={len(x)} out={len(y)}")

    fig, axes = plt.subplots(2, 1, figsize=(9.0, 6.2), sharex=True)
    im = None
    for ax, sig, name in zip(axes, (x, y), ("输入", "输出（空跑重建）")):
        spec, freqs, times = stft_db(sig)
        mask = freqs <= FMAX
        im = ax.imshow(
            spec[mask, :],
            origin="lower",
            aspect="auto",
            extent=[times[0], times[-1], 0.0, FMAX / 1000.0],
            cmap="magma",
            vmin=VMIN,
            vmax=VMAX,
            interpolation="nearest",
        )
        ax.set_ylabel("频率 / kHz")
        ax.set_title(name, loc="left", fontsize=11)
        ax.set_ylim(0, FMAX / 1000.0)
    axes[-1].set_xlabel("时间 / s")
    fig.suptitle(
        r"QMF 空跑：线性扫频输入 / 输出 STFT（$f_s=32\,\mathrm{kHz}$）",
        y=0.995,
        fontsize=12,
    )
    cbar = fig.colorbar(im, ax=axes.ravel().tolist(), fraction=0.03, pad=0.02)
    cbar.set_label("dB")
    fig.subplots_adjust(hspace=0.28, right=0.90, top=0.90)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(args.out, dpi=140, bbox_inches="tight")
    copy = in_dir / "chirp_stft.png"
    fig.savefig(copy, dpi=140, bbox_inches="tight")
    print("saved", args.out)
    print("saved", copy)


if __name__ == "__main__":
    main()

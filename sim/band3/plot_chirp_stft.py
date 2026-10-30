#!/usr/bin/env python3
"""Build band3_chirp, run the linear-chirp empty reconstruction, plot STFT and the filter bank."""

import argparse
import subprocess
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

plt.rcParams["font.sans-serif"] = ["STHeiti", "Heiti SC", "Arial Unicode MS"]
plt.rcParams["axes.unicode_minus"] = False

FS = 48000
N_FFT = 1024
HOP = 256
VMIN = -80.0
VMAX = 0.0
FMAX = 24000.0

# Same literals as band3/three_band_filter_bank.cc. Rows are residues
# r = 0, 1, 2, 4, 5, 6, 7, 8, 10, 11; columns are kappa = 0..3.
COEFFS = np.array(
    [
        [-0.00047749, -0.00496888, +0.16547118, +0.00425496],
        [-0.00173287, -0.01585778, +0.14989004, +0.00994113],
        [-0.00304815, -0.02536082, +0.12154542, +0.01157993],
        [-0.00346946, -0.02587886, +0.04760441, +0.00607594],
        [-0.00154717, -0.01136076, +0.01387458, +0.00186353],
        [+0.00186353, +0.01387458, -0.01136076, -0.00154717],
        [+0.00607594, +0.04760441, -0.02587886, -0.00346946],
        [+0.00983212, +0.08543175, -0.02982767, -0.00383509],
        [+0.00994113, +0.14989004, -0.01585778, -0.00173287],
        [+0.00425496, +0.16547118, -0.00496888, -0.00047749],
    ],
    dtype=np.float64,
)
RESIDUES = np.array([0, 1, 2, 4, 5, 6, 7, 8, 10, 11])


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


def prototype() -> np.ndarray:
    p = np.zeros(48, dtype=np.float64)
    for row, residue in enumerate(RESIDUES):
        for kappa in range(4):
            p[12 * kappa + residue] = COEFFS[row, kappa]
    return p


def modulated() -> list[np.ndarray]:
    p = prototype()
    k = np.arange(48)
    bands = []
    for b in range(3):
        c = 2.0 * np.cos(2.0 * np.pi * k * (2 * b + 1) / 12.0)
        bands.append(p * c)
    return bands


def freq_response(h: np.ndarray, n_fft: int = 4096) -> tuple[np.ndarray, np.ndarray]:
    spec = np.fft.rfft(h, n=n_fft)
    freqs = np.fft.rfftfreq(n_fft, d=1.0 / FS)
    return freqs, spec


def build_and_run(root: Path, out_dir: Path) -> None:
    build_dir = root / "output" / "band3"
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
        ["cmake", "--build", str(build_dir), "--target", "band3_chirp"]
    )
    out_dir.mkdir(parents=True, exist_ok=True)
    subprocess.check_call([str(build_dir / "band3_chirp"), str(out_dir)])


def plot_chirp(x: np.ndarray, y: np.ndarray, path: Path) -> None:
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
        r"三分频空跑：线性扫频输入 / 输出 STFT（$f_s=48\,\mathrm{kHz}$）",
        y=0.995,
        fontsize=12,
    )
    cbar = fig.colorbar(im, ax=axes.ravel().tolist(), fraction=0.03, pad=0.02)
    cbar.set_label("dB")
    fig.subplots_adjust(hspace=0.28, right=0.90, top=0.90)
    path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(path, dpi=140, bbox_inches="tight")
    plt.close(fig)
    print("saved", path)


def plot_filters(path: Path) -> None:
    bands = modulated()
    fig, axes = plt.subplots(2, 1, figsize=(9.0, 6.2), sharex=True)
    names = ("低带", "中带", "高带")
    for b, h in enumerate(bands):
        freqs, spec = freq_response(h)
        mag = 20.0 * np.log10(np.maximum(np.abs(spec), 1e-12))
        axes[0].plot(freqs / 1000.0, mag, label=names[b], lw=1.2)
        axes[1].plot(freqs / 1000.0, mag + 20.0 * np.log10(3.0), label=names[b], lw=1.2)
    for ax, title in zip(axes, (r"分析 $|H_b|$", r"综合 $|F_b|=|3H_b|$")):
        ax.set_xlim(0, 24)
        ax.set_ylim(-80, 20)
        ax.set_ylabel("幅度 / dB")
        ax.set_title(title, loc="left", fontsize=11)
        ax.legend(frameon=False, ncol=3, loc="lower right")
        ax.axhline(0.0, color="#a8a29e", lw=0.6)
    axes[-1].set_xlabel("频率 / kHz")
    fig.suptitle(r"三分频滤波器（$f_s=48\,\mathrm{kHz}$）", y=0.995, fontsize=12)
    fig.subplots_adjust(hspace=0.28, top=0.90)
    path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(path, dpi=140, bbox_inches="tight")
    plt.close(fig)
    print("saved", path)


def plot_ta(path: Path) -> None:
    bands = modulated()
    n_fft = 4096
    specs = [freq_response(h, n_fft)[1] for h in bands]
    freqs = freq_response(bands[0], n_fft)[0]
    # H(e^{jw} W^ell) is a shift of -ell * fs/3 on the frequency axis.
    shift = n_fft // 3

    def rotated(spec: np.ndarray, ell: int) -> np.ndarray:
        full = np.fft.fft(np.fft.irfft(spec, n=n_fft), n=n_fft)
        full = np.roll(full, ell * shift)
        return full[: n_fft // 2 + 1]

    t = sum(s * s for s in specs)
    a1 = sum(s * rotated(s, 1) for s in specs)
    a2 = sum(s * rotated(s, 2) for s in specs)
    fig, ax = plt.subplots(figsize=(9.0, 3.6))
    for curve, name in (
        (t, r"$|T|$"),
        (a1, r"$|A_1|$"),
        (a2, r"$|A_2|$"),
    ):
        mag = 20.0 * np.log10(np.maximum(np.abs(curve), 1e-12))
        ax.plot(freqs / 1000.0, mag, label=name, lw=1.2)
    ax.set_xlim(0, 24)
    ax.set_ylim(-80, 10)
    ax.set_xlabel("频率 / kHz")
    ax.set_ylabel("幅度 / dB")
    ax.set_title(r"空跑失真 $|T|$ 与混叠 $|A_\ell|$", loc="left", fontsize=11)
    ax.legend(frameon=False, ncol=3, loc="lower right")
    ax.axhline(0.0, color="#a8a29e", lw=0.6)
    fig.subplots_adjust(top=0.86)
    path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(path, dpi=140, bbox_inches="tight")
    plt.close(fig)
    print("saved", path)
    t_db = 20.0 * np.log10(np.maximum(np.abs(t), 1e-12))
    a_db = 20.0 * np.log10(np.maximum(np.maximum(np.abs(a1), np.abs(a2)), 1e-12))
    print(
        "T_db min/max",
        float(t_db.min()),
        float(t_db.max()),
        "A_db max",
        float(a_db.max()),
    )


def main() -> None:
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--out",
        type=Path,
        default=root / "doc" / "image" / "band3_fig_chirp_stft.png",
    )
    args = parser.parse_args()
    in_dir = root / "output" / "sim" / "band3"
    build_and_run(root, in_dir)

    x = np.fromfile(in_dir / "in.f32", dtype=np.float32).astype(np.float64)
    y = np.fromfile(in_dir / "out.f32", dtype=np.float32).astype(np.float64)
    if x.shape != y.shape or len(x) < N_FFT:
        raise SystemExit(f"bad lengths in={len(x)} out={len(y)}")

    plot_chirp(x, y, args.out)
    fig_copy = in_dir / "chirp_stft.png"
    fig_copy.write_bytes(args.out.read_bytes())
    print("saved", fig_copy)
    image_dir = root / "doc" / "image"
    plot_filters(image_dir / "band3_fig_filters.png")
    plot_ta(image_dir / "band3_fig_TA.png")


if __name__ == "__main__":
    main()

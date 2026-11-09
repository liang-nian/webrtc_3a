#!/usr/bin/env python3
"""Build vad_check and vad_sim, run them, plot the measured probabilities."""

import argparse
import subprocess
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

plt.rcParams["font.sans-serif"] = [
    "WenQuanYi Micro Hei",
    "Droid Sans Fallback",
    "STHeiti",
    "Arial Unicode MS",
]
plt.rcParams["axes.unicode_minus"] = False


def build_and_run(root: Path, out_dir: Path) -> None:
    build_dir = root / "output" / "vad"
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
        ["cmake", "--build", str(build_dir), "--target", "vad_check", "vad_sim"]
    )
    subprocess.check_call([str(build_dir / "vad_check")])
    out_dir.mkdir(parents=True, exist_ok=True)
    subprocess.check_call([str(build_dir / "vad_sim"), str(out_dir)])


def load_csv(path: Path) -> dict[str, np.ndarray]:
    data = np.genfromtxt(path, delimiter=",", names=True)
    return {name: np.asarray(data[name]) for name in data.dtype.names}


def save(fig, path: Path, copy: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(path, dpi=140, bbox_inches="tight")
    fig.savefig(copy, dpi=140, bbox_inches="tight")
    print("saved", path)
    print("saved", copy)


def main() -> None:
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--out-dir",
        type=Path,
        default=root / "doc" / "image",
    )
    args = parser.parse_args()
    sim_dir = root / "output" / "sim" / "vad"
    build_and_run(root, sim_dir)

    series = [
        ("silence", "全 0"),
        ("noise_std4000", "高斯噪声，标准差 4000"),
        ("harmonic_150", "合成谐波 150 Hz"),
        ("syllable_150", "合成谐波，300 ms 开 / 200 ms 关"),
    ]
    fig, axes = plt.subplots(len(series), 1, figsize=(9.2, 7.4), sharex=True)
    for ax, (name, title) in zip(axes, series):
        d = load_csv(sim_dir / f"{name}.csv")
        ax.plot(d["t"], d["p"], color="#1f4e79", lw=1.0)
        ax.axhline(0.95, color="#c45c26", ls="--", lw=0.8)
        ax.set_ylim(-0.05, 1.05)
        ax.set_ylabel("概率")
        ax.set_title(title, loc="left", fontsize=11)
    axes[-1].set_xlabel("时间 / s")
    fig.suptitle(
        r"AGC2 RNN-VAD 语音概率（合成信号，$f_s=24\,\mathrm{kHz}$，S16 标度）",
        y=0.995,
        fontsize=12,
    )
    fig.subplots_adjust(hspace=0.38, top=0.93)
    save(fig, args.out_dir / "vad_fig_prob.png", sim_dir / "prob.png")
    plt.close(fig)

    fig, axes = plt.subplots(2, 1, figsize=(9.2, 5.6), sharex=True)
    for ax, name, title in (
        (axes[0], "burst_80ms", "80 ms 合成谐波插入噪声"),
        (axes[1], "burst_200ms", "200 ms 合成谐波插入噪声"),
    ):
        d = load_csv(sim_dir / f"{name}.csv")
        ax.plot(d["t"], d["p"], color="#1f4e79", lw=1.1, label="语音概率")
        ax.plot(
            d["t"],
            d["adjacent"] / 12.0,
            color="#2a9d8f",
            lw=1.0,
            label="相邻帧 / 12",
        )
        ax.axhline(0.95, color="#c45c26", ls="--", lw=0.8)
        ax.set_ylim(-0.05, 1.85)
        ax.set_ylabel("归一化")
        ax.set_title(title, loc="left", fontsize=11)
        ax.legend(loc="upper right", fontsize=8, frameon=False)
    axes[-1].set_xlabel("时间 / s")
    fig.suptitle("0.95 门限与 12 帧相邻计数", y=0.995, fontsize=12)
    fig.subplots_adjust(hspace=0.32, top=0.90)
    save(fig, args.out_dir / "vad_fig_burst.png", sim_dir / "burst.png")
    plt.close(fig)

    fig, ax = plt.subplots(figsize=(9.2, 3.6))
    for name, label, color in (
        ("harmonic_150", "合成谐波 150 Hz", "#1f4e79"),
        ("noise_std4000", "高斯噪声", "#c45c26"),
    ):
        d = load_csv(sim_dir / f"{name}.csv")
        voiced = d["silence"] < 0.5
        ax.plot(
            d["t"][voiced],
            d["pitch_hz"][voiced],
            color=color,
            lw=1.0,
            label=label,
        )
    ax.set_ylim(0, 400)
    ax.set_xlabel("时间 / s")
    ax.set_ylabel("基频估计 / Hz")
    ax.set_title("非静音帧的 48 kHz 基音周期换算", loc="left", fontsize=11)
    ax.legend(frameon=False, fontsize=9)
    fig.subplots_adjust(top=0.86)
    save(fig, args.out_dir / "vad_fig_pitch.png", sim_dir / "pitch.png")
    plt.close(fig)


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Re-fetch upstream VAD sources, build the reference, and compare it to the
committed extract on real speech. Exits non-zero if any frame disagrees.
"""

import argparse
import hashlib
import subprocess
import wave
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

plt.rcParams["font.sans-serif"] = [
    "WenQuanYi Micro Hei",
    "Droid Sans Fallback",
    "STHeiti",
]
plt.rcParams["axes.unicode_minus"] = False

SEED = 20261109


def run(cmd, cwd):
    print("+", " ".join(str(c) for c in cmd))
    subprocess.check_call(cmd, cwd=cwd)


def read_wav(path: Path) -> tuple[np.ndarray, int]:
    with wave.open(str(path)) as wav:
        if wav.getnchannels() != 1 or wav.getsampwidth() != 2:
            raise SystemExit(f"expected mono s16 wav: {path}")
        rate = wav.getframerate()
        pcm = np.frombuffer(wav.readframes(wav.getnframes()), dtype="<i2").copy()
    return pcm, rate


def write_pcm(path: Path, pcm: np.ndarray) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    pcm.astype("<i2").tofile(path)


def gmm(binary: Path, pcm: Path, rate: int, frame_ms: int, mode: int):
    out = subprocess.check_output(
        [str(binary), str(pcm), str(rate), str(frame_ms), str(mode)],
        text=True,
    )
    rows = []
    for line in out.splitlines()[1:]:
        frame, raw, public = line.split()
        rows.append((int(frame), int(raw), int(public)))
    return rows


def rnn(binary: Path, pcm: Path, rate: int):
    out = subprocess.check_output([str(binary), str(pcm), str(rate)], text=True)
    rows = []
    for line in out.splitlines()[1:]:
        frame, prob = line.split()
        rows.append((int(frame), float.fromhex(prob)))
    return rows


def compare_gmm(learn, ref, label, failures):
    if len(learn) != len(ref):
        failures.append(f"{label} length {len(learn)} vs {len(ref)}")
        return
    raw_bad = pub_bad = 0
    for a, b in zip(learn, ref):
        if a[1] != b[1]:
            raw_bad += 1
        if a[2] != b[2]:
            pub_bad += 1
    speech = sum(r[2] for r in learn)
    print(
        f"GMM {label} frames={len(learn)} raw_mismatch={raw_bad} "
        f"public_mismatch={pub_bad} public_speech_frames={speech}"
    )
    if raw_bad or pub_bad:
        failures.append(f"{label} raw={raw_bad} public={pub_bad}")


def compare_rnn(learn, ref, label, failures):
    if len(learn) != len(ref):
        failures.append(f"{label} length {len(learn)} vs {len(ref)}")
        return
    err = np.array([abs(a[1] - b[1]) for a, b in zip(learn, ref)], dtype=np.float64)
    over = int(np.count_nonzero(err > 0))
    thr_l = np.array([a[1] >= 0.95 for a in learn])
    thr_r = np.array([b[1] >= 0.95 for b in ref])
    thr_bad = int(np.count_nonzero(thr_l != thr_r))
    print(
        f"RNN {label} frames={len(learn)} max_abs={err.max() if len(err) else 0:.3e} "
        f"mean_abs={err.mean() if len(err) else 0:.3e} nonzero={over} "
        f"threshold_0.95_mismatch={thr_bad}"
    )
    # Same sources and the same compiler flags must be bit-identical.
    if over or thr_bad:
        failures.append(
            f"{label} nonzero={over} max={err.max()} thr={thr_bad}"
        )


def main() -> None:
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser()
    parser.add_argument("--skip-fetch", action="store_true")
    args = parser.parse_args()
    ref_src = root / "output" / "webrtc-ref" / "src"
    learn_bin = root / "output" / "vad"
    ref_bin = root / "output" / "vad-ref"
    sim_dir = root / "output" / "sim" / "vad-align"
    sim_dir.mkdir(parents=True, exist_ok=True)

    if not args.skip_fetch:
        run(["python3", "tools/fetch_webrtc_src.py", "--dest", str(ref_src)], root)
    diff = subprocess.run(
        ["diff", "-rq", str(root / "third_party" / "webrtc" / "src"), str(ref_src)],
        text=True,
        capture_output=True,
    )
    if diff.returncode != 0:
        print(diff.stdout)
        print(diff.stderr)
        raise SystemExit("committed extract differs from the re-fetched upstream tree")
    print("source diff: committed extract == re-fetched upstream")

    run(
        [
            "cmake",
            "-S",
            str(root),
            "-B",
            str(ref_bin),
            f"-DWEBRTC_TREE={ref_src}",
            "-DCMAKE_BUILD_TYPE=Release",
        ],
        root,
    )
    run(["cmake", "--build", str(ref_bin), "--target", "gmm_learn", "rnn_learn"], root)
    run(
        ["cmake", "--build", str(learn_bin), "--target", "gmm_learn", "rnn_learn", "qmf_chirp", "band3_chirp"],
        root,
    )

    speech, rate = read_wav(root / "speech" / "1089-134686-0000.wav")
    if rate != 16000:
        raise SystemExit(f"unexpected wav rate {rate}")
    rng = np.random.default_rng(SEED)
    rms = float(np.sqrt(np.mean(speech.astype(np.float64) ** 2)))
    sigma = rms / (10 ** (10 / 20))
    mixed = np.clip(
        np.rint(speech.astype(np.float64) + rng.normal(0.0, sigma, size=speech.shape)),
        -32768,
        32767,
    ).astype(np.int16)
    pause = np.concatenate(
        [speech[:32000], np.zeros(8000, dtype=np.int16), speech[32000:]]
    )
    signals = {
        "speech16": speech,
        "silence16": np.zeros(16000, dtype=np.int16),
        "pause16": pause,
        "noise10db16": mixed,
        "decimate8": speech[::2].copy(),
        "hold32": np.repeat(speech, 2),
        "hold48": np.repeat(speech, 3),
    }
    rates = {
        "speech16": 16000,
        "silence16": 16000,
        "pause16": 16000,
        "noise10db16": 16000,
        "decimate8": 8000,
        "hold32": 32000,
        "hold48": 48000,
    }
    for name, pcm in signals.items():
        write_pcm(sim_dir / f"{name}.s16", pcm)

    learn_gmm = learn_bin / "gmm_learn"
    ref_gmm = ref_bin / "gmm_learn"
    learn_rnn = learn_bin / "rnn_learn"
    ref_rnn = ref_bin / "rnn_learn"
    failures = []

    # Invalid rate must fail the same way on both binaries.
    for binary, tag in ((learn_gmm, "learn"), (ref_gmm, "ref")):
        bad = subprocess.run(
            [str(binary), str(sim_dir / "speech16.s16"), "22050", "10", "0"],
            capture_output=True,
            text=True,
        )
        print(f"GMM invalid rate {tag} exit={bad.returncode}")
        if bad.returncode == 0:
            failures.append(f"invalid rate accepted by {tag}")

    gmm_cases = []
    for name in ("speech16", "silence16", "pause16", "noise10db16"):
        for frame_ms in (10, 20, 30):
            for mode in (0, 1, 2, 3):
                gmm_cases.append((name, rates[name], frame_ms, mode))
    for name in ("decimate8", "hold32", "hold48"):
        for frame_ms in (10, 30):
            for mode in (0, 3):
                gmm_cases.append((name, rates[name], frame_ms, mode))

    stored = {}
    stored_ref = {}
    for name, rate_hz, frame_ms, mode in gmm_cases:
        label = f"{name} {rate_hz}Hz {frame_ms}ms mode{mode}"
        a = gmm(learn_gmm, sim_dir / f"{name}.s16", rate_hz, frame_ms, mode)
        b = gmm(ref_gmm, sim_dir / f"{name}.s16", rate_hz, frame_ms, mode)
        compare_gmm(a, b, label, failures)
        if (name, frame_ms, mode) in {
            ("speech16", 10, 2),
            ("noise10db16", 10, 2),
            ("pause16", 10, 2),
            ("silence16", 10, 0),
        }:
            stored[("gmm", name, frame_ms, mode)] = a
            stored_ref[("gmm", name, frame_ms, mode)] = b

    rnn_cases = [
        "speech16",
        "silence16",
        "pause16",
        "noise10db16",
        "decimate8",
        "hold48",
    ]
    for name in rnn_cases:
        label = f"{name} {rates[name]}Hz 10ms"
        a = rnn(learn_rnn, sim_dir / f"{name}.s16", rates[name])
        b = rnn(ref_rnn, sim_dir / f"{name}.s16", rates[name])
        compare_rnn(a, b, label, failures)
        stored[("rnn", name)] = a
        stored_ref[("rnn", name)] = b

    # Plots use the committed-extract output. The numeric check above is the
    # reference overlay; differences are identically zero when the test passes.
    speech_p = stored[("rnn", "speech16")]
    speech_p_ref = stored_ref[("rnn", "speech16")]
    noise_p = stored[("rnn", "noise10db16")]
    pause_p = stored[("rnn", "pause16")]
    gmm_speech = stored[("gmm", "speech16", 10, 2)]
    gmm_speech_ref = stored_ref[("gmm", "speech16", 10, 2)]
    gmm_noise = stored[("gmm", "noise10db16", 10, 2)]
    gmm_pause = stored[("gmm", "pause16", 10, 2)]

    def save(fig, name):
        dest = root / "doc" / "image" / name
        copy = sim_dir / name
        fig.savefig(dest, dpi=140, bbox_inches="tight")
        fig.savefig(copy, dpi=140, bbox_inches="tight")
        print("saved", dest)

    t_s = np.arange(len(speech)) / 16000
    fig, axes = plt.subplots(3, 1, figsize=(9.4, 7.2), sharex=True)
    axes[0].plot(t_s, speech, color="#334155", lw=0.4)
    axes[0].set_ylabel("幅度")
    axes[0].set_title(
        "LibriSpeech 1089-134686-0000，16 kHz / 16 bit / 单声道，真实人声",
        loc="left",
        fontsize=11,
    )
    gt = np.array([r[0] for r in gmm_speech]) * 0.01
    gv = np.array([r[2] for r in gmm_speech])
    axes[1].step(gt, gv, where="post", color="#1f4e79", lw=1.0, label="提交的摘录")
    axes[1].step(
        np.array([r[0] for r in gmm_speech_ref]) * 0.01,
        np.array([r[2] for r in gmm_speech_ref]),
        where="post",
        color="#c45c26",
        lw=0.8,
        ls="--",
        label="重新下载的上游",
    )
    axes[1].set_ylim(-0.1, 1.2)
    axes[1].set_ylabel("公开判决")
    axes[1].set_title("GMM-VAD  mode 2，10 ms，16 kHz", loc="left", fontsize=11)
    axes[1].legend(frameon=False, fontsize=8, loc="upper right")
    rt = np.array([r[0] for r in speech_p]) * 0.01
    rp = np.array([r[1] for r in speech_p])
    axes[2].plot(rt, rp, color="#1f4e79", lw=1.0, label="提交的摘录")
    axes[2].plot(
        np.array([r[0] for r in speech_p_ref]) * 0.01,
        np.array([r[1] for r in speech_p_ref]),
        color="#c45c26",
        lw=0.8,
        ls="--",
        label="重新下载的上游",
    )
    axes[2].axhline(0.95, color="#888888", ls=":", lw=0.8)
    axes[2].set_ylim(-0.05, 1.05)
    axes[2].set_ylabel("概率")
    axes[2].set_xlabel("时间 / s")
    axes[2].set_title(
        "AGC2 RNN-VAD，10 ms，输入 16 kHz，内部 PushResampler 到 24 kHz",
        loc="left",
        fontsize=11,
    )
    axes[2].legend(frameon=False, fontsize=8, loc="lower right")
    fig.subplots_adjust(hspace=0.35, top=0.96)
    save(fig, "vad_fig_speech.png")
    plt.close(fig)

    fig, axes = plt.subplots(2, 1, figsize=(9.4, 5.4), sharex=True)
    axes[0].step(gt, gv, where="post", color="#1f4e79", lw=1.0, label="干净人声")
    nt = np.array([r[0] for r in gmm_noise]) * 0.01
    nv = np.array([r[2] for r in gmm_noise])
    axes[0].step(nt, nv, where="post", color="#c45c26", lw=0.9, label="人声 + 10 dB 高斯噪声")
    axes[0].set_ylim(-0.1, 1.2)
    axes[0].set_ylabel("GMM 公开判决")
    axes[0].set_title("mode 2，10 ms，16 kHz。噪声是合成的，人声不是。", loc="left", fontsize=11)
    axes[0].legend(frameon=False, fontsize=8)
    axes[1].plot(rt, rp, color="#1f4e79", lw=1.0, label="干净人声")
    npt = np.array([r[0] for r in noise_p]) * 0.01
    npp = np.array([r[1] for r in noise_p])
    axes[1].plot(npt, npp, color="#c45c26", lw=0.9, label="人声 + 10 dB 高斯噪声")
    axes[1].axhline(0.95, color="#888888", ls=":", lw=0.8)
    axes[1].set_ylim(-0.05, 1.05)
    axes[1].set_ylabel("RNN 概率")
    axes[1].set_xlabel("时间 / s")
    axes[1].legend(frameon=False, fontsize=8)
    fig.subplots_adjust(hspace=0.28, top=0.92)
    save(fig, "vad_fig_noise.png")
    plt.close(fig)

    fig, axes = plt.subplots(2, 1, figsize=(9.4, 5.2), sharex=True)
    pt = np.arange(len(pause)) / 16000
    axes[0].plot(pt, pause, color="#334155", lw=0.4)
    axes[0].axvspan(2.0, 2.5, color="#f4a261", alpha=0.35, label="插入的数字静音")
    axes[0].set_ylabel("幅度")
    axes[0].set_title("真实人声在 2.0–2.5 s 插入 0.5 s 静音。16 kHz。", loc="left", fontsize=11)
    axes[0].legend(frameon=False, fontsize=8)
    pgt = np.array([r[0] for r in gmm_pause]) * 0.01
    pgv = np.array([r[2] for r in gmm_pause])
    prt = np.array([r[0] for r in pause_p]) * 0.01
    prp = np.array([r[1] for r in pause_p])
    axes[1].step(pgt, pgv, where="post", color="#1f4e79", lw=1.0, label="GMM mode 2 公开判决")
    axes[1].plot(prt, prp, color="#2a9d8f", lw=1.0, label="RNN 概率")
    axes[1].axhline(0.95, color="#888888", ls=":", lw=0.8)
    axes[1].set_xlim(1.4, 3.2)
    axes[1].set_ylim(-0.05, 1.15)
    axes[1].set_xlabel("时间 / s")
    axes[1].set_ylabel("判决 / 概率")
    axes[1].legend(frameon=False, fontsize=8)
    fig.subplots_adjust(hspace=0.28, top=0.92)
    save(fig, "vad_fig_pause.png")
    plt.close(fig)

    diff = np.array(
        [abs(a[1] - b[1]) for a, b in zip(speech_p, speech_p_ref)], dtype=np.float64
    )
    gmm_diff = np.array(
        [0 if a[1] == b[1] and a[2] == b[2] else 1 for a, b in zip(gmm_speech, gmm_speech_ref)]
    )
    fig, axes = plt.subplots(2, 1, figsize=(9.4, 4.6), sharex=True)
    axes[0].step(gt, gmm_diff, where="post", color="#1f4e79", lw=1.0)
    axes[0].set_ylim(-0.05, 1.05)
    axes[0].set_ylabel("GMM 不一致")
    axes[0].set_title(
        "16 kHz / 10 ms / mode 2。0 表示原始判决和公开判决都与上游相同。",
        loc="left",
        fontsize=11,
    )
    axes[1].plot(rt, diff, color="#c45c26", lw=1.0)
    axes[1].set_ylabel("|Δp|")
    axes[1].set_xlabel("时间 / s")
    axes[1].set_title("RNN 概率绝对误差。输入 16 kHz，10 ms。", loc="left", fontsize=11)
    fig.subplots_adjust(hspace=0.35, top=0.90)
    save(fig, "vad_fig_diff.png")
    plt.close(fig)

    sha = hashlib.sha256((root / "speech" / "1089-134686-0000.wav").read_bytes()).hexdigest()
    summary = sim_dir / "align_summary.txt"
    summary.write_text(
        f"wav_sha256={sha}\n"
        f"speech_rms={rms:.4f}\n"
        f"noise_sigma_10db={sigma:.4f}\n"
        f"seed={SEED}\n"
        f"failures={len(failures)}\n"
        + "\n".join(failures)
        + "\n"
    )
    print("wrote", summary)
    if failures:
        print("FAILED")
        for item in failures:
            print(" ", item)
        raise SystemExit(1)
    print("align_vad passed")


if __name__ == "__main__":
    main()

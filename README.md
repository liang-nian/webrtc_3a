# webrtc_3a

WebRTC 音频链路的笔记和仿真。笔记在 `doc/`。两带 QMF 在 `qmf/`，三分频在 `band3/`。两种 VAD 的源码摘录在 `third_party/webrtc/`：定点 GMM 见 `doc/gmm_vad.md`，AGC2 RNN-VAD 见 `doc/vad.md`。仿真脚本在 `sim/qmf/`、`sim/band3/`、`sim/vad/`。

依赖：CMake 3.16、C/C++ 编译器、Python 3、NumPy、Matplotlib。在仓库根目录执行。

## 仿真

两带 QMF 线性扫频空跑。32 kHz、1 s、幅度 0.8，频率从 100 Hz 升到 15.5 kHz。按 10 ms 分帧调用 `WebRtcSpl_AnalysisQMF`，子带原样送回，再 `WebRtcSpl_SynthesisQMF`。笔记见 `doc/qmf.md`。输入输出写到 `output/sim/qmf/in.f32`、`out.f32`，图写到 `doc/image/qmf_fig_chirp_stft.png` 和 `output/sim/qmf/chirp_stft.png`，可执行文件在 `output/qmf/`。

```bash
python3 sim/qmf/plot_chirp_stft.py
```

三分频线性扫频空跑。48 kHz、1 s、幅度 0.8，频率从 100 Hz 升到 23 kHz。按 10 ms（480 点）分帧调用 `ThreeBandFilterBank::Analysis`，子带原样送回，再 `Synthesis`。笔记见 `doc/band3.md`。输入输出写到 `output/sim/band3/in.f32`、`out.f32`，图写到 `doc/image/band3_fig_filters.png`、`band3_fig_TA.png`、`band3_fig_chirp_stft.png` 和 `output/sim/band3/chirp_stft.png`，可执行文件在 `output/band3/`。

```bash
python3 sim/band3/plot_chirp_stft.py
```

两种 VAD 和重新下载的上游逐帧对齐，并用 `speech/` 里的 LibriSpeech 录音出图。脚本会下载、`diff`、编译两边、比较，再写 `doc/image/vad_fig_speech.png`、`vad_fig_noise.png`、`vad_fig_pause.png`、`vad_fig_diff.png`。有一帧不一致就非 0 退出。

```bash
python3 sim/vad/align_vad.py
```

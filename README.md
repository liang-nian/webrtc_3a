# webrtc_3a

WebRTC 音频链路的笔记和仿真。笔记在 `doc/`。两带 QMF 在 `qmf/`，三分频在 `band3/`。仿真脚本在 `sim/qmf/`、`sim/band3/`，会自己用 CMake 编译，再跑扫频、出图。

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

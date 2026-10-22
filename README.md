# webrtc_3a

WebRTC 音频链路的笔记和仿真。笔记在 `doc/`，两带 QMF 在 `qmf/`，仿真脚本在 `sim/qmf/`。脚本会自己用 CMake 编译，再跑扫频、出图。

依赖：CMake 3.16、C/C++ 编译器、Python 3、NumPy、Matplotlib。在仓库根目录执行。

## 仿真

两带 QMF 线性扫频空跑。32 kHz、1 s、幅度 0.8，频率从 100 Hz 升到 15.5 kHz。按 10 ms 分帧调用 `WebRtcSpl_AnalysisQMF`，子带原样送回，再 `WebRtcSpl_SynthesisQMF`。笔记见 `doc/qmf.md`。输入输出写到 `output/sim/qmf/in.f32`、`out.f32`，图写到 `doc/image/qmf_fig_chirp_stft.png` 和 `output/sim/qmf/chirp_stft.png`，可执行文件在 `output/qmf/`。

```bash
python3 sim/qmf/plot_chirp_stft.py
```

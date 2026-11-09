# temp/vad

这次仿真结果的留存，给合入前核对用。合入时可以整目录删掉。`doc/image/` 里的图没有移动。

中文总结和图在 [REPORT.md](REPORT.md)。下面仍是留存和补跑的区分。

## 来源

命令是仓库里的 `python3 sim/vad/align_vad.py`。它比较两份编译：

- 学习版：`output/vad/gmm_learn`、`output/vad/rnn_learn`（提交里的 WebRTC 摘录）
- 原版：`output/vad-ref/` 下同名程序（脚本当时重新下载的同一批源码）

语音是 `speech/1089-134686-0000.wav`（LibriSpeech test-clean，CC BY 4.0）。派生 PCM 仍在 `output/sim/vad-align/`，该目录被 `.gitignore` 忽略，没有放进本目录。

## 哪些是当时留下的，哪些是补跑的

| 路径 | 性质 |
|------|------|
| `retained/align_summary.txt` | 2026-11-09 02:44 UTC 那次 `align_vad.py` 写下的汇总，原样复制 |
| `retained/figures/*.png` | 同一次运行写入 `doc/image/` 的四张图的副本，字节与 `doc/image` 相同 |
| `retained/MISSING.txt` | 当时没有落盘的东西 |
| `rerun/` | 2026-11-09 09:36 UTC 用**还在磁盘上的那两对可执行文件**和同一批 PCM 再跑了一遍。没有重新下载、没有重新编译 |

`align_vad.py` 只在标准输出里打印每个条件的帧数和误差，不写逐帧文件。那次标准输出没有终端日志留下来，所以逐帧表是补跑得到的，不是把旧日志改写进去的。补跑的汇总数和 02:44 的打印一致（例如 16 kHz、10 ms、mode 2 的公开语音帧仍是 978；RNN 最大绝对误差仍是 0）。

## 复现

完整流程（会重新下载并编译）：

```bash
python3 sim/vad/align_vad.py
```

本目录里的逐帧文件是对已有程序的再调用，参数与 `sim/vad/align_vad.py` 里的用例相同：

```bash
output/vad/gmm_learn     <pcm> <rate> <frame_ms> <mode>
output/vad-ref/gmm_learn <pcm> <rate> <frame_ms> <mode>
output/vad/rnn_learn     <pcm> <rate>
output/vad-ref/rnn_learn <pcm> <rate>
```

PCM 为 `output/sim/vad-align/<name>.s16`。

## 逐帧文件

- `rerun/gmm/*.learn.txt`、`*.ref.txt`：程序自己的 stdout（`frame raw public`）
- `rerun/gmm/*.error.csv`：`learn_raw,ref_raw,raw_mismatch,learn_public,ref_public,public_mismatch`
- `rerun/rnn/*.learn.txt`、`*.ref.txt`：十六进制概率
- `rerun/rnn/*.error.csv`：两边概率和绝对误差，以及 0.95 门限是否不同
- `rerun/summary.csv`：每个条件一行
- `rerun/logs/rerun.log`：这次补跑的打印
- `rerun/logs/gmm_invalid_rate_*.txt`：22050 Hz 非法采样率，两边退出码都是 2

GMM 60 组（16 kHz 四条信号 × 10/20/30 ms × mode 0–3，外加 8/32/48 kHz 的 10 ms 与 30 ms、mode 0 与 3）。RNN 6 组（`speech16`、`silence16`、`pause16`、`noise10db16`、`decimate8`、`hold48`，都是 10 ms）。`hold32` 只在 GMM 里，和 `align_vad.py` 一样。

补跑结果：全部 GMM 原始判决和公开判决的不一致帧数是 0；全部 RNN 的最大绝对误差、平均绝对误差、非零误差帧数、0.95 门限不一致帧数都是 0。

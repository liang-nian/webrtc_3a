# AGC2 RNN-VAD

48 kHz 的捕获链路在合带之后把 10 ms 全带交给 AGC2。本篇只解析这一处用到的语音活动检测：`modules/audio_processing/agc2/rnn_vad`，外加 `vad_wrapper`。定点 GMM 是另一篇，`doc/gmm_vad.md`。

可运行的代码是提交 `eeb7e6beb2e20c5a273922fdf22b75a75cc28ad5`（2026-11-07）的摘录，放在 `third_party/webrtc/src`。权重来自 Chromium `third_party` `63d42f6e6a54c9463fcb599001ca81ff2fd3dc8e`，RNNoise 基线 `91ef401f4c3536c6de999ac609262691ec888c4c`（BSD-3-Clause）。构建适配见 `third_party/webrtc/ADAPTATIONS.md`。频谱用的是上游的 PFFFT，16/48 kHz 输入用的是上游的 `PushResampler`，不是直接 DFT。

同一棵树里还有两类名字也叫 VAD 的东西。GMM 的笔记和仿真在 `doc/gmm_vad.md`。本篇不把它们和 RNN 混在一起：

| 位置 | 是什么 | 和本篇的关系 |
|------|--------|----------------|
| `common_audio/vad` | 定点 GMM，`WebRtcVad_Process`，8/16/32/48 kHz，10/20/30 ms | 公开 API 和旧代码。不是 AGC2 的默认检测器 |
| `modules/audio_processing/vad` | 基音 + GMM，给 AGC1 | `AudioProcessingImpl` 在本提交里构造 AGC2 时写死 `use_internal_vad=true`，不走这条 |
| `agc2/rnn_vad` | 24 kHz 特征 + 全连接 + GRU | 本篇 |

`doc/webrtc_3a.md` 里「无 NS 时自带、有 NS 时可复用」说的是更早的接法。本提交的 `GainController2::Process` 只在内部 `vad_` 存在时调用 `Analyze`，没有从噪声抑制器读语音概率。NS 自己的语音概率是另一件事。

`speech_probability_buffer` 编进了 `input_volume_controller` 这个构建目标，单元测试也在，但 `GainController2::Process` 和 `MonoInputVolumeController::Process` 都没有调用它。下面只把它当作一份对照过测试数字的旁路公式，不放进仿真曲线。

ps:公式按源码的运算顺序写。x86 上点积会走摘录里的 SSE2 或 AVX2，下面写的是同一只点积的标量形式。

**符号**

| 符号 | 含义 |
|------|------|
| $x(n)$ | 24 kHz、S16 标度的 float。满幅正弦大约是 $\pm 32768$，不是 $[-1,1]$ |
| $N_{10}, N_{20}$ | 240、480，即 10 ms、20 ms |
| $T_{48}$ | 基音周期，单位是 48 kHz 上的样点 |
| $\mathbf{f}\in\mathbb{R}^{42}$ | 送进 RNN 的特征 |
| $p$ | RNN 输出的语音概率，静音门打开时强制为 0 |
| $a_j$ | 5 阶 LPC 系数，$j=0,\ldots,4$ |

---

## 1. 数学模型

### 1.1 帧从哪来

`VoiceActivityDetectorWrapper` 每 10 ms 做三件事：

1. 倒数计数。初值 $1500/10=150$。减到 0 就 `RnnVad::Reset()`，只清 GRU 状态，特征缓冲不动，然后把计数拨回 150。
2. 只取第一声道，用 `PushResampler` 变到 24 kHz，得到 240 点。本仓库的仿真不包含这只重采样器，调用方直接给 24 kHz。
3. `FeaturesExtractor::CheckSilenceComputeFeatures`，再 `RnnVad::ComputeVadProbability`。

高通写在特征提取器里，系数是 24 kHz 上 60 Hz 的 2 阶 Butterworth，但构造函数把 `use_high_pass_filter_` 设成 `false`。当前这条路径不过高通。APM 前面已有独立的高通，见 `doc/webrtc_3a.md` §2.2。

### 1.2 基音缓冲和 LPC

240 点推进长度 $kBufSize24kHz = 384+480 = 864$ 的缓冲。384 是 24 kHz 上的最长基音（$24000/62.5$），480 是分析帧。

对这 864 点做自相关 $r(\ell)=\sum_n x(n)x(n+\ell)$，$\ell=0,\ldots,4$。$r(0)=0$ 时系数全 0。否则按源码做两步预处理：

$$
r(0)\leftarrow 1.0001\,r(0),\qquad
r(\ell)\leftarrow r(\ell)\big(1-(0.008\ell)^2\big),\quad \ell=1,\ldots,4.
$$

后面是 Levinson 递推。反射系数

$$
k_i=-\frac{r(i+1)+\sum_{j=0}^{i-1}a_j r(i-j)}{e},
$$

$|e|<10^{-6}$ 时把 $e$ 拉到这个幅度并保留符号。$e<0.001\,r(0)$ 就停。递推完再乘 $0.9^{j+1}$，然后做一次

$$
\begin{aligned}
a'_0 &= 0.9\,a_0+0.8,\\
a'_j &= 0.9^{j+1}a_j+0.8\cdot 0.9^{j}a_{j-1},\quad j=1,2,3,\\
a'_4 &= 0.8\cdot 0.9^{4}a_3.
\end{aligned}
$$

残差是

$$
e(n)=x(n)+\sum_{j=0}^{4} a'_j\,x(n-1-j),
$$

边界上缺的历史不补，只加已经存在的项。基音搜索吃的是 $e(n)$，频谱特征吃的仍是原波形。

### 1.3 基音

搜索分四步，和 `pitch_search.cc` 的顺序一致。

1. 2 倍抽取，不抗混叠：$y_{12}(i)=e(2i)$，432 点、12 kHz。
2. 在 12 kHz 上，倒滞后 $k=0,\ldots,146$ 的相关是最近 240 点与向前滑 240 点的点积

$$
c_{12}(k)=\sum_{n=0}^{239} y_{12}(k+n)\,y_{12}(192+n).
$$

源码用 512 点 PFFFT 做同一只线性相关。这里直接点积。正的 $c_{12}$ 按 $c^2/E_y$ 选最大和次大，比较时用交叉相乘，不除法。$E_y$ 的初值是前 241 点能量加 1，然后每步丢掉旧样点、补进新样点，并夹到不小于 0。这和源码一致，不是整齐的 240 点能量。
3. 两个候选的倒滞后乘 2，回到 24 kHz。只在各自 $\pm 2$ 的邻域里重算 24 kHz 点积，再用滑窗能量做同样的比值比较。邻域端点外的相关被写成 0，供下面的伪插值读取。
4. 伪插值把 24 kHz 倒滞后变成 48 kHz 的一个整数偏移，取值 $\{-1,0,+1\}$：

$$
\delta=
\begin{cases}
+1, & c_{+}-c_{-}>0.7\,(c_0-c_{-}),\\
-1, & c_{-}-c_{+}>0.7\,(c_0-c_{+}),\\
0, & \text{其余}.
\end{cases}
$$

源码把「下一个」和「上一个」按倒滞后的存储顺序传入，注释里留了符号可能反了的 TODO。这里保持那个实参顺序。

得到初值 $T_{48}^{(0)}$ 之后，`ComputeExtendedPitchPeriod48kHz` 在 24 kHz 周期上试子谐波。除数 $d=2,\ldots,d_{\max}$，

$$
T(m,d)=\Big\lfloor\frac{2m\,T+d}{2d}\Big\rfloor,
$$

$m=1$ 是候选，$m$ 还从 $\{3,2,3,2,5,\ldots\}$ 里再取一个陪衬周期，两者的相关和能量各取平均。强度

$$
s=\frac{xy}{\sqrt{1+E_x E_y}}.
$$

和上一帧周期差 1 个样点时，门限减去上一帧强度；差 2 且初值周期够长时减一半。短周期的门限抬到 0.4 或 0.5，用来压高频假峰。最后

$$
s_{\mathrm{final}}=\min\Big(s,\ \mathbf{1}[E_y\le xy]\,+\,\mathbf{1}[E_y>xy]\frac{xy}{E_y+1}\Big),
$$

周期再做一次伪插值，并不小于 $kMinPitch48kHz=60$。

写进特征的是归一化周期，不是赫兹：

$$
f_{40}=0.01\,(T_{48}-300).
$$

150 Hz 对应 $T_{48}=48000/150=320$，$f_{40}=0.2$。250 Hz 对应 $192$，$f_{40}=-1.08$。

### 1.4 频谱特征

参考帧是缓冲里最新的 480 点。滞后帧起点是 $384-T_{48}/2$，长度同样 480。整数除法。

窗是 Vorbis 窗，再除以 480：

$$
w(i)=\frac{1}{480}\sin\Big(\frac{\pi}{2}\sin^2\big(\frac{\pi}{2}\frac{i+0.5}{240}\big)\Big),
$$

对称贴到整帧。谱是 PFFFT 的不归一化实变换，`ordered=true`：下标 0 是直流，下标 1 是奈奎斯特，随后是 $(X_k)$ 的实部、虚部。奈奎斯特系数写成 0。12 kHz 的基音相关也是 PFFFT 上的线性卷积，不是手写点积。

20 个 Opus 带（24 kHz、20 ms 的带边界，奈奎斯特以上的三带不单独存在）用三角权重把 $|X|^2$ 分到相邻带。第 0 带再乘 2。带能量之和小于 $0.04$ 就判静音：后面的倒谱不写，GRU 清零，$p=0$。$f_{40}$ 此时已经写过，但是网络不用它。

未静音时，对数带能量做一次沿频率的抬升

$$
u_i=\max(\ell_{\max}-7,\ \max(f-1.5,\ \log_{10}(10^{-2}+E_i))),
$$

$\ell_{\max}$ 和 $f$ 随带号更新。20 带之后的两带按 $\log_{10}(10^{-2})=-2$ 送进同一个平滑。DCT

$$
c_i=0.301511345\sum_{j} u_j\cos\Big(\frac{(j+0.5)\,i\,\pi}{22}\Big)
$$

（$i=0$ 的余弦项再乘 $\sqrt{1/2}$，这个因子做进表里）。然后 $c_0\leftarrow c_0-12$，$c_1\leftarrow c_1-4$。

42 维的摆法：

| 下标 | 内容 |
|------|------|
| $0\ldots 5$ | 最近三帧低阶倒谱之和 $c+c^{(-1)}+c^{(-2)}$，没有除以 3 |
| $6\ldots 21$ | $c_6,\ldots,c_{21}$ |
| $22\ldots 27$ | 一阶差分 $c-c^{(-2)}$ |
| $28\ldots 33$ | 二阶差分 $c-2c^{(-1)}+c^{(-2)}$ |
| $34\ldots 39$ | 参考帧和滞后帧的带互相关，归一化 $\sqrt{0.001+E_{\mathrm{ref}}E_{\mathrm{lag}}}$ 之后的 DCT 前 6 项，$c_0$ 再减 1.3，$c_1$ 再减 0.9 |
| $40$ | $f_{40}$ |
| $41$ | 8 帧倒谱距离矩阵里，每个延迟的最小距离取平均，再减 2.1 |

历史不足时，环缓冲里是 0。静音帧不推进这只环。

### 1.5 网络

权重是 int8，乘 $1/256$ 变成 float。全连接在输出维上转置之后做点积，输出维为 1 的最后一层不转置。GRU 的三组门（更新、复位、状态）按源码的下标重排。

$$
\begin{aligned}
\mathbf{z}_1&=\tanh_{\mathrm{tab}}(W_1\mathbf{f}+\mathbf{b}_1),\\
\mathbf{u}&=\sigma_{\mathrm{tab}}(W_u\mathbf{z}_1+R_u\mathbf{h}+\mathbf{b}_u),\\
\mathbf{r}&=\sigma_{\mathrm{tab}}(W_r\mathbf{z}_1+R_r\mathbf{h}+\mathbf{b}_r),\\
\tilde{\mathbf{h}}&=\mathrm{ReLU}(W_h\mathbf{z}_1+R_h(\mathbf{h}\odot\mathbf{r})+\mathbf{b}_h),\\
\mathbf{h}&\leftarrow \mathbf{u}\odot\mathbf{h}+(1-\mathbf{u})\odot\tilde{\mathbf{h}},\\
p&=\sigma_{\mathrm{tab}}(W_2\mathbf{h}+b_2).
\end{aligned}
$$

$\tanh_{\mathrm{tab}}$ 是 RNNoise 的 201 点表，范围 $[-8,8]$，表外饱和到 $\pm 1$，表内再用 $(1-y^2)$ 做一次修正。$\sigma_{\mathrm{tab}}(x)=\tfrac12+\tfrac12\tanh_{\mathrm{tab}}(x/2)$。隐藏层 24。点积按标量顺序累加，没有 AVX2 / NEON。

静音时不跑这三层，直接把 $\mathbf{h}$ 清零并返回 0。这和每 150 帧的复位是同一只 `Reset`，只是静音每帧都会触发。

### 1.6 概率怎么变成 AGC2 的判决

这一节是 `speech_level_estimator_impl.cc` 和 `adaptive_digital_gain_controller.cc` 里的消费逻辑。这次编出来的程序停在 `VoiceActivityDetectorWrapper` 的概率，没有把电平估计和增益曲线链进去。

默认门限 $kVadConfidenceThreshold=0.95$。相邻帧阈值 $kAdjacentSpeechFramesThreshold=12$。

自适应数字增益里，`frames_to_gain_increase_allowed` 初值是 12。$p<0.95$ 把它拨回 12；$p\ge 0.95$ 就减 1。减到 0 才允许增益上升。第一次减到 0 的那一帧，允许的上升量再乘 12（每 10 ms 的默认步长是 $6/100=0.06\,\mathrm{dB}$，本仿真不往下算增益曲线）。

语音电平估计的初值用默认配置算：

$$
L_0=\mathrm{clamp}(-20-15-5,\ -90,\ 30)=-40\ \mathrm{dBFS}.
$$

$-20$ 是饱和保护的初始余量，$-15$ 和 $-5$ 是 `AdaptiveDigital` 的 `initial_gain_db`、`headroom_db`。只在 $p\ge 0.95$ 的帧上更新

$$
\begin{aligned}
\nu &\leftarrow \lambda \nu + r\,p,\\
\delta &\leftarrow \lambda \delta + p,\\
\lambda &=
\begin{cases}
1, & \text{累计语音帧不足 }400\,\mathrm{ms},\\
1-1/400, & \text{已满},
\end{cases}
\end{aligned}
$$

$r$ 是本帧 RMS 的 dBFS，$v\le 1$ 时为 $-90.309$。相邻帧数达到 12 才把 $\nu/\delta$ 夹到 $[-90,30]$ 写进对外电平。一段语音不足 12 帧就遇到 $p<0.95$ 时，初步状态退回上一次可靠状态，对外电平不动。够 12 帧之后的第一帧非语音，才把初步状态提交成可靠状态。

输入音量控制器另用一扇更松的门。默认 `speech_probability_threshold=0.7`，窗口 `update_input_volume_wait_frames=100`，`speech_ratio_threshold=0.6`。窗口内 $p\ge 0.7$ 的帧数占比达到 0.6，才允许按电平原误差改音量。对象的第一帧有一个 `is_first_frame_` 豁免，窗口长度是 100 时它挡不住第一次结算。

### 1.7 旁路的概率缓冲

`SpeechProbabilityBuffer` 存 100 帧。低于调用者给定阈值的概率先写成 0。若此前连续高概率不超过 7 帧，就把这几帧也改回 0，并从和里减掉。缓冲满且和 $\ge 0.9\times 100$ 才叫活动段。本提交的处理函数没有用它。`vad_check` 用它的单元测试数字对过：三帧 1 再来一帧 0，和从 3 变成 0；九帧 1 再来一帧 0，和留在 9。

---

## 2. 实例

$f_{40}$ 只是周期的平移缩放。150 Hz 若被收成 $T_{48}=320$，特征就是 0.2；250 Hz 若被收成 192，特征就是 -1.08。这次仿真没有单独打印特征，周期对不对体现在和上游概率逐帧相同。

全 0、16 kHz、10 ms，100 帧。`rnn_learn` 打出的概率全部是 0。静音门把 GRU 清掉了。

概率缓冲的手算仍由 `vad_check` 对：阈值 0.2，三个 1 之后再来一个 0，和从 3 变成 0；九个 1 之后再来一个 0，和留在 9。这个缓冲不在 `Process` 上。

---

## 3. 仿真

跑的是 `VoiceActivityDetectorWrapper`：10 ms 一帧，采样率不是 24 kHz 时走摘下来的 `PushResampler`，每 1500 ms 清一次 GRU。学习程序和参考程序是两次编译，源码树一个是仓库里的摘录，一个是 `align_vad.py` 当场重新下载的同一批文件。`diff -rq` 不为空，或者任意一帧概率的十六进制浮点不一样，脚本就非 0 退出。

```bash
python3 sim/vad/align_vad.py
```

人声是 `speech/1089-134686-0000.wav`，16 kHz。另外构造了全 0、在 2.0 s 处插入 0.5 s 数字静音，以及按种子 `20261109` 加上 10 dB 高斯噪声。8 kHz 隔点抽取和 48 kHz 样点保持只用来覆盖重采样入口，不是原始录音。语料没有逐帧真值，下面的计数不是准确率。

16 kHz 真实语音，1043 帧：平均概率 0.614，最小 $7.0\times 10^{-5}$，最大 0.999986，其中 519 帧 $\ge 0.95$，没有一帧精确为 0。全 0 的 100 帧概率都是 0。和重新下载的上游比，最大绝对误差是 0，平均绝对误差是 0，超差帧数是 0，0.95 门限判决不一致的帧数也是 0。8 kHz、48 kHz、插入静音和 10 dB 噪声同样是 0。

![真实语音、GMM 判决和 RNN 概率](image/vad_fig_speech.png)

![干净人声和加噪](image/vad_fig_noise.png)

![插入静音](image/vad_fig_pause.png)

![逐帧差](image/vad_fig_diff.png)

---

## 4. 和原版差在哪

算法文件没有改。差只在 `third_party/webrtc/ADAPTATIONS.md` 里的头文件垫片：`RTC_DCHECK` 按 Release 丢掉、CPU 指令检测用 `__builtin_cpu_supports`、对齐分配用 `posix_memalign`。参考二进制用同一套垫片，所以两边的 SIMD 核选择一致。概率逐帧用十六进制浮点比较，这次是全 0 误差，不是只看 0.95 门限。

高通标志在摘录里仍是 `false`。网络权重仍是 int8 乘 $1/256$ 之后的 float，没有另一套定点 RNN。增益曲线、噪声底和限幅器没有算。GMM 不在本篇。

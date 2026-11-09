# 真实语音样本

`1089-134686-0000.wav` 是 LibriSpeech test-clean 的一条真人朗读，说话人 1089，章节 134686，语句 0000。语料是公开有声书，许可是 Creative Commons Attribution 4.0。

| 项 | 值 |
|----|----|
| 语料 | OpenSLR SLR12，LibriSpeech test-clean |
| 语料地址 | https://www.openslr.org/12 |
| 语料压缩包 | https://www.openslr.org/resources/12/test-clean.tar.gz |
| 压缩包内路径 | `LibriSpeech/test-clean/1089/134686/1089-134686-0000.flac` |
| 本仓库取用的文件 | https://github.com/amd/LIRA/raw/main/datasets/LibriSpeech/1089-134686-0000.wav |
| 许可 | CC BY 4.0，https://creativecommons.org/licenses/by/4.0/ |
| 引用 | V. Panayotov, G. Chen, D. Povey, S. Khudanpur. LibriSpeech: An ASR Corpus Based on Public Domain Audio Books. IEEE ICASSP 2015. |
| 格式 | WAV PCM，16 kHz，16 bit，单声道，166960 点，10.435 s |
| SHA-256 | `e0bb45200ea542afe89f10958c629b19af07a5f30786021e24b93d907582c231` |

文件头里的编码软件是 Lavf62.0。仓库没有再改采样率、增益或长度。LibriSpeech 没有逐帧语音/静音标注，所以仿真只核对两个实现是否一致，并看判决随时间怎么走，不报准确率。

复现下载：

```bash
python3 - << 'PY'
import urllib.request, hashlib
url = "https://github.com/amd/LIRA/raw/main/datasets/LibriSpeech/1089-134686-0000.wav"
data = urllib.request.urlopen(url, timeout=120).read()
open("speech/1089-134686-0000.wav", "wb").write(data)
print(hashlib.sha256(data).hexdigest())
PY
```

期望摘要就是上表的 SHA-256。

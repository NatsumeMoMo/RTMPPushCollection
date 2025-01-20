# RTMP推流

[流媒体协议之RTMP详解20230513 - yuweifeng - 博客园](https://www.cnblogs.com/yuweifeng/p/17444833.html)



## RTMP数据包格式

**了解FLV和RTMP包结构的相同点和不同点**: 

（以下的博客中有的会将 `packet type` 划分到了`Audio Tag Header` \  `Video Tag Header`  (body header)中，有的博客以及其博客中贴出的协议原文会把 `packet type` 归在 `Audio Data` \ `Video Data` （body data）中。即这个 `packet type` 要么在header的最后一个比特位，要么在data一开始的首个比特位。在这两种划分方式的，这个比特位的位置都没变，以方便自己理解的方式进行记忆。其中1、3为前者划分，2、4为后者划分）



1. [【流媒体协议】图解 FLV 协议 快速入门_flv协议-CSDN博客](https://blog.csdn.net/qq_33844311/article/details/122951819) 

2. [FLV协议5分钟入门浅析 - 程序猿小卡 - 博客园](https://www.cnblogs.com/chyingp/p/flv-getting-started.html)

3. [【Android音视频开发】【029】RTMP和FLV数据包格式图解_rtmp和flv关系-CSDN博客](https://blog.csdn.net/u013718730/article/details/119789187)

4. [RTMP与FLV协议(转载)_rtmp flv-CSDN博客](https://blog.csdn.net/water1209/article/details/128562747)

5. [RTMP和flv分析_rtmp flv-CSDN博客](https://blog.csdn.net/tanziliang98/article/details/132372012)



- 下面给出一个以**文本结构图**为核心、逐层拆解的 RTMP 数据包格式说明。整体分为**RTMP Packet**→**Packet Header**(→Basic Header、Message Header、Extended Timestamp)→**Packet Body**。在每一层拆解时，都会指出各自的长度(字节/比特)以及字段作用。对于**Packet Body**部分，不同 **Message Type**(例如音频、视频、数据等)会有不同的内容结构，也会继续拆解。

  ------

  ## 1. RTMP Packet

  ```scss
  ┌─────────────────────────────────────────────────────────┐
  │ RTMP Packet                                             │
  │  ┌────────────────────────────────────────────────────┐ │
  │  │ Packet Header                                      │ │
  │  │  ├─ Basic Header (1 ~ 3 bytes)                     │ │
  │  │  ├─ Message Header (0, 3, 7, or 11 bytes)          │ │
  │  │  └─ Extended Timestamp (0 or 4 bytes)              │ │
  │  └────────────────────────────────────────────────────┘ │
  │  ┌────────────────────────────────────────────────────┐ │
  │  │ Packet Body (长度由Message Header的message_length    │ │
  │  │              字段决定)                               │ │
  │  └────────────────────────────────────────────────────┘ │
  └─────────────────────────────────────────────────────────┘
  ```

  RTMP 数据流是由一个个 RTMP Packet 组成，每个 RTMP Packet 必定包含**Packet Header**和**Packet Body**两部分。**Packet Header**中包含了拆包所需的元数据(例如时间戳、类型、Stream ID 等)，**Packet Body**中则是实际的音视频数据或命令消息等。

  ------

  ## 2. Packet Header

  Packet Header 由以下三部分组成：

  1. **Basic Header** (1 ~ 3 字节)
  2. **Message Header** (0, 3, 7, or 11 字节)
  3. **Extended Timestamp** (可选 4 字节)

  ```scss
  ┌───────────────────────────────────────────────────────────────────┐
  │ Packet Header                                                     │
  │  ├─ Basic Header (1 ~ 3 bytes)                                    │
  │  │   (包含 chunk_type/format 以及 chunk_stream_id)                  │
  │  ├─ Message Header (可为 0/3/7/11 bytes, 取决于 Basic Header 中的
  │  │                chunk_type/format)                              │
  │  │   (包含 timestamp / message_length / message_type / stream_id)  │
  │  └─ Extended Timestamp (4 bytes, 当 timestamp >= 16777215 时出现)   │
  └───────────────────────────────────────────────────────────────────┘
  ```

  下面分别详细拆解这三部分的内部格式。

  ------

  ### 2.1 Basic Header

  Basic Header 的长度取决于 chunk_stream_id 的取值范围，最短 1 字节，最长 3 字节。其最重要的字段是 **2 bits** 的 `format (也称 chunk_type)` 和用于标识 Chunk Stream 的 `chunk_stream_id` (6 bits ~ 14 bits 或更多)。

  #### 2.1.1 Basic Header 结构图

  ```scss
  ┌─────────────────────────────────────────────────────────────────────┐
  │ Basic Header (1~3 bytes)                                            │
  │ ┌───────────────────────────────────────────────┐                   │
  │ │  format (2 bits)  │ chunk_stream_id (6 bits)  │  ==> (1 byte)     │
  │ └───────────────────────────────────────────────┘                   │
  │  ┌─────────────────────────────────────────────┐                    │
  │  │      chunk_stream_id ( > 63 时扩展 )         │  ==> (2 or 3 bytes)│
  │  └─────────────────────────────────────────────┘                    │
  └─────────────────────────────────────────────────────────────────────┘
  ```

  - **format (2 bits)**
    - 该字段也常被称为 **Chunk Type**，决定了后面 **Message Header** 的长度(0, 3, 7, 11 字节)。
      - 00：表示 **Message Header** 长度为 11 字节
      - 11：表示 **Message Header** 长度为 7 字节
      - 22：表示 **Message Header** 长度为 3 字节
      - 33：表示 **Message Header** 为 0 字节(与前一个 chunk 共享头部信息)
  - **chunk_stream_id**
    - 剩余的 bit 用于表示 Chunk Stream ID。
    - 当 chunk_stream_id ≤ 63 时，仅用 6 bits 即可(1 字节 Basic Header)。
    - 当 chunk_stream_id 为 64 ~ 319 时，需要再多出 1 个字节来表示(2 字节 Basic Header)。
    - 当 chunk_stream_id ≥ 320 时，需要再多出 2 个字节来表示(3 字节 Basic Header)。

  ------

  ### 2.2 Message Header

  Message Header 的长度由前面 Basic Header 的 `format/Chunk Type` 来决定：

  - **format = 0** → 11 字节
  - **format = 1** → 7 字节
  - **format = 2** → 3 字节
  - **format = 3** → 0 字节

  #### 2.2.1 Message Header 4 种长度概览

  1. **11 字节** (format = 0)

     ```scss
     ┌──────────────────────────────────────────────────────────┐
     │ Message Header (11 bytes)                                │
     │  ├─ timestamp (3 bytes)                                  │
     │  ├─ message_length (3 bytes)                             │
     │  ├─ message_type (1 byte)                                │
     │  └─ msg_stream_id (4 bytes, 小端序)                       │
     └──────────────────────────────────────────────────────────┘
     ```

  2. **7 字节** (format = 1)

     ```scss
     ┌──────────────────────────────────────────────────────────┐
     │ Message Header (7 bytes)                                 │
     │  ├─ timestamp_delta (3 bytes)                            │
     │  ├─ message_length (3 bytes)                             │
     │  └─ message_type (1 byte)                                │
     └──────────────────────────────────────────────────────────┘
     ```

     - 此时不再包含 msg_stream_id，因为默认与前一个 chunk 的相同。

  3. **3 字节** (format = 2)

     ```scss
     ┌──────────────────────────────────────────────────────────┐
     │ Message Header (3 bytes)                                 │
     │  └─ timestamp_delta (3 bytes)                            │
     └──────────────────────────────────────────────────────────┘
     ```

     - 此时不包含 message_length, message_type, msg_stream_id。均与前一个 chunk 相同。

  4. **0 字节** (format = 3)

     ```scss
     ┌──────────────────────────────────────────────────────────┐
     │ Message Header (0 bytes)                                 │
     └──────────────────────────────────────────────────────────┘
     ```

     - 不再包含任何字段，与上一个 chunk 完全相同的消息头信息(只可能是同一个 chunk_stream_id 上的后续 chunk)。

  #### 2.2.2 具体字段说明

  - **timestamp / timestamp_delta (3 bytes, 大端序)**
    - 当 `format=0` 时用作“绝对时间戳 (absolute timestamp)”。
    - 当 `format=1` 或 `format=2` 时用作“相对时间戳 (timestamp delta)”，相对于之前发送的 chunk 的时间戳。
    - 如果该 3 字节数值为 `0xFFFFFF` (即 16777215)，则需要使用 **Extended Timestamp** 字段(4 字节) 进行扩展。
  - **message_length (3 bytes, 大端序)**
    - 表示 **Packet Body** 的长度(即负载数据长度)，不包含 Packet Header。
    - 最大可表示 16777215(0xFFFFFF) 字节。
  - **message_type (1 byte)**
    - 标识后面 **Packet Body** 的数据类型。例如：
      - 8 = Audio message
      - 9 = Video message
      - 18 = Metadata / AMF0 Data message
      - 20 = AMF0 command message (如 connect、publish 等)
      - 17 = AMF3 command message
      - ...(其他类型可参见官方文档)
  - **msg_stream_id (4 bytes, 小端序)**
    - 表示该 RTMP 消息所在的流编号(如播放流、发布流等)。
    - 仅在 `format=0` 的 11 字节头中出现。

  ------

  ### 2.3 Extended Timestamp

  ```scss
  ┌─────────────────────────────────────────────────────────┐
  │ Extended Timestamp (4 bytes, 大端序)                     │
  │  - 仅在 timestamp 或 timestamp_delta = 0xFFFFFF 时出现    │
  └─────────────────────────────────────────────────────────┘
  ```

  - 当 **timestamp** 或 **timestamp_delta** 的 3 字节部分为 `0xFFFFFF(16777215)` 时，表示真正的时间戳或时间增量存放在这个 **4 字节** 的 Extended Timestamp 里(大端序)。
  - 如果 **timestamp** 或 **timestamp_delta** < 16777215，则不需要 Extended Timestamp，该字段不存在。

  ------

  ## 3. Packet Body

  `Packet Body` 的长度由 `Message Header` 中的 **message_length** 决定，具体内容由 `message_type` 决定。可以是音频数据、视频数据，也可能是命令消息、元数据等。

  ### 3.1 不同 message_type 的常见类型

  - **0x08 (Audio Message)**
  - **0x09 (Video Message)**
  - **0x12 (Metadata AMF0)** 或 **0x0F (Metadata AMF3)** 等
  - **0x14 (Command AMF0)** 或 **0x11 (Command AMF3)** 等

  下面列举最常见的**音频**与**视频**的具体结构。其他类型(如命令消息 AMF0/AMF3)也有各自的协议格式(AMF 编码结构)，其内部字段较多，会根据 RTMP 命令的种类而不同。

  ------

  ### 3.2 Audio Message (message_type = 8)

  如果 `message_type == 8`，**Packet Body** 是音频数据(通常承载与 FLV 格式一致的音频帧)。结构上一般如下(以 FLV Audio Tag 为参考)：

  ```scss
  ┌────────────────────────────────────────────────────────────────────────┐
  │ Audio Message Body (length = message_length)                           │
  │  ┌───────────────────────────────────────────────────────────────────┐ │
  │  │ SoundFormat (4 bits) | SoundRate (2 bits) | SoundSize (1 bit)     │ │
  │  │    | SoundType (1 bit)                                            │ │
  │  └───────────────────────────────────────────────────────────────────┘ │
  │  ┌───────────────────────────────────────────────────────────────────┐ │
  │  │ Audio Data (具体编码数据, 如 AAC Header + AAC Raw Data 等)           │ │
  │  └───────────────────────────────────────────────────────────────────┘ │
  └────────────────────────────────────────────────────────────────────────┘
  ```

  - SoundFormat (4 bits)
    - 指示音频编码类型。例如 10 = AAC, 2 = MP3 等。
  - SoundRate (2 bits)
    - 采样率。例如 0 = 5.5 kHz, 1 = 11 kHz, 2 = 22 kHz, 3 = 44 kHz。
  - SoundSize (1 bit)
    - 采样大小。例如 0 = 8-bit, 1 = 16-bit。
  - SoundType (1 bit)
    - 声道数。例如 0 = 单声道, 1 = 立体声。
  - Audio Data
    - 具体音频数据帧。例如 AAC 可能紧跟一个 AACPacketType(1 字节) + raw AAC 数据。

  > 注：具体 FLV Audio Tag 结构可根据实际编码而不同，AAC 会有额外的 AAC Header 等字段。
  >
  > 
  >
  > ## Audio tags
  >
  > 定义如下所示：
  >
  > | 字段        | 字段类型                | 字段含义                                                     |
  > | :---------- | :---------------------- | :----------------------------------------------------------- |
  > | SoundFormat | UB[4]                   | 音频格式，重点关注 **10 = AAC ** 0 = Linear PCM, platform endian 1 = ADPCM 2 = MP3 3 = Linear PCM, little endian 4 = Nellymoser 16-kHz mono 5 = Nellymoser 8-kHz mono 6 = Nellymoser 7 = G.711 A-law logarithmic PCM 8 = G.711 mu-law logarithmic PCM 9 = reserved 10 = AAC 11 = Speex 14 = MP3 8-Khz 15 = Device-specific sound |
  > | SoundRate   | UB[2]                   | 采样率，对AAC来说，永远等于3 0 = 5.5-kHz 1 = 11-kHz 2 = 22-kHz 3 = 44-kHz |
  > | SoundSize   | UB[1]                   | 采样精度，对于压缩过的音频，永远是16位 0 = snd8Bit 1 = snd16Bit |
  > | SoundType   | UB[1]                   | 声道类型，对Nellymoser来说，永远是单声道；对AAC来说，永远是双声道； 0 = sndMono 单声道 1 = sndStereo 双声道 |
  > | SoundData   | UI8[size of sound data] | 如果是AAC，则为 AACAUDIODATA； 其他请参考规范；              |
  >
  > 备注：
  >
  > > If the SoundFormat indicates AAC, the SoundType should be set to 1 (stereo) and the SoundRate should be set to 3 (44 kHz). However, this does not mean that AAC audio in FLV is always stereo, 44 kHz data. Instead, the Flash Player ignores these values and extracts the channel and sample rate data is encoded in the AAC bitstream.
  >
  > ### AACAUDIODATA
  >
  > 当 SoundFormat 为10时，表示音频采AAC进行编码，此时，SoundData的定义如下：
  >
  > | 字段          | 字段类型 | 字段含义                                                     |
  > | :------------ | :------- | :----------------------------------------------------------- |
  > | AACPacketType | UI8      | 0: AAC sequence header 1: AAC raw                            |
  > | Data          | UI8[n]   | 如果AACPacketType为0，则为AudioSpecificConfig 如果AACPacketType为1，则为AAC帧数据 |
  >
  > > The AudioSpecificConfig is explained in ISO 14496-3. Note that it is not the same as the contents of the esds box from an MP4/F4V file. This structure is more deeply embedded.
  >
  > ### 关于AudioSpecificConfig
  >
  > 伪代码如下：参考[这里](https://wiki.multimedia.cx/index.php/MPEG-4_Audio#Audio_Specific_Config)
  >
  > ```delphi
  > 5 bits: object type
  > if (object type == 31)
  >     6 bits + 32: object type
  > 4 bits: frequency index
  > if (frequency index == 15)
  >     24 bits: frequency
  > 4 bits: channel configuration
  > var bits: AOT Specific Config
  > ```
  >
  > 定义如下：
  >
  > | 字段                   | 字段类型 | 字段含义                                           |
  > | :--------------------- | :------- | :------------------------------------------------- |
  > | AudioObjectType        | UB[5]    | 编码器类型，比如2表示AAC-LC                        |
  > | SamplingFrequencyIndex | UB[4]    | 采样率索引值，比如4表示44100                       |
  > | SamplingFrequencyIndex | UB[4]    | 采样率索引值，比如4表示44100                       |
  > | ChannelConfiguration   | UB[4]    | 声道配置，比如2代表双声道，front-left, front-right |

  ------

  ### 3.3 Video Message (message_type = 9)

  如果 `message_type == 9`，**Packet Body** 是视频数据(通常与 FLV Video Tag 一致的格式)。其结构如下：

  ```scss
  ┌──────────────────────────────────────────────────────────────────────────┐
  │ Video Message Body (length = message_length)                             │
  │  ┌─────────────────────────────────────────────────────────────────────┐ │
  │  │ FrameType (4 bits) | CodecID (4 bits)                               │ │
  │  └─────────────────────────────────────────────────────────────────────┘ │
  │  ┌─────────────────────────────────────────────────────────────────────┐ │
  │  │ Video Data (编码后的视频帧数据, 例如 AVC/H.264 或 HEVC 等)               │ │
  │  └─────────────────────────────────────────────────────────────────────┘ │
  └─────────────────────────────────────────────────────────────────────────┘
  ```

  - FrameType (4 bits)
    - 表示帧类型，如 1 = keyframe, 2 = inter frame(非关键帧) 等。
  - CodecID (4 bits)
    - 视频编码类型，如 7 = AVC(H.264), 12 = HEVC(H.265) 等。
  - Video Data
    - 具体的视频帧数据。
    - 对于 H.264/AVC，通常紧跟 **AVCPacketType(1 字节)** 和 **CompositionTime(3 字节)**，然后才是原始的 H.264 数据(nal units)。

  > ## Video tags
  >
  > 定义如下：
  >
  > | 字段      | 字段类型      | 字段含义                                                     |
  > | :-------- | :------------ | :----------------------------------------------------------- |
  > | FrameType | UB[4]         | 重点关注1、2： 1: keyframe (for AVC, a seekable frame) —— 即H.264的IDR帧； 2: inter frame (for AVC, a non- seekable frame) —— H.264的普通I帧； 3: disposable inter frame (H.263 only) 4: generated keyframe (reserved for server use only) 5: video info/command frame |
  > | CodecID   | UB[4]         | 编解码器，主要关注 7（AVC） 1: JPEG (currently unused) 2: Sorenson H.263 3: Screen video 4: On2 VP6 5: On2 VP6 with alpha channel 6: Screen video version 2 **7: AVC** |
  > | VideoData | 取决于CodecID | 实际的媒体类型，主要关注 7:AVCVIDEOPACKE 2: H263VIDEOPACKET 3: SCREENVIDEOPACKET 4: VP6FLVVIDEOPACKET 5: VP6FLVALPHAVIDEOPACKET 6: SCREENV2VIDEOPACKET **7: AVCVIDEOPACKE** |
  >
  > ### AVCVIDEOPACKE
  >
  > 当 CodecID 为 7 时，VideoData 为 AVCVIDEOPACKE，也即 H.264媒体数据。
  >
  > AVCVIDEOPACKE 的定义如下：
  >
  > | 字段            | 字段类型 | 字段含义                                                     |
  > | :-------------- | :------- | :----------------------------------------------------------- |
  > | AVCPacketType   | UI8      | 0: AVC sequence header 1: AVC NALU 2: AVC end of sequence    |
  > | CompositionTime | SI24     | 如果AVCPacketType=1，则为时间cts偏移量；否则，为0            |
  > | Data            | UI8[n]   | 1、如果如果AVCPacketType=1，则为AVCDecoderConfigurationRecord 2、如果AVCPacketType=1=2，则为NALU（一个或多个） 3、如果AVCPacketType=2，则为空 |
  >
  > 这里有几点稍微解释下：
  >
  > 1. NALU：H.264中，将**数据**按照特定规则格式化后得到的抽象逻辑单元，称为NALU。这里的数据既包括了编码后的视频数据，也包括视频解码需要用到的参数集（PPS、SPS）。
  > 2. AVCDecoderConfigurationRecord：H.264 视频解码所需要的参数集（SPS、PPS）
  > 3. CTS：当B帧的存在时，视频解码呈现过程中，dts、pts可能不同，cts的计算公式为 pts - dts/90，单位为毫秒；如果B帧不存在，则cts固定为0；
  >
  > PPS、SPS这里先不展开。

  ------

  ### 3.4 其他类型 (简要说明)

  - **0x12 (Metadata AMF0) / 0x0F (Metadata AMF3)**
    - 通常包含一些元数据信息(如 onMetaData)。内部采用 AMF 编码。
  - **0x14 (Command AMF0) / 0x11 (Command AMF3)**
    - 常用于 RTMP 命令消息(如 connect、createStream、play、publish 等)，内部是 AMF 编码序列。

  不同的命令或元数据，都有其在 AMF 中对应的字段结构。

  ------

  ## 4. 综合示例（ASCII 结构总览）

  最后将最常见的一帧 RTMP Packet 结构以更直观的文本形式罗列：(假设 format=0，message_type=9 视频数据为例)

  ```scss
  ┌───────────────────────────────────────────────────────────────────────────────────────────┐
  │ RTMP Packet                                                                               │
  │  ┌─────────────────────────────────────────────────────────────────────────────────────┐  │
  │  │ Packet Header                                                                        │ │
  │  │  ├─ Basic Header (1 byte)                                                            │ │
  │  │  │   ┌─ format (2 bits) = 0                                                          │ │
  │  │  │   └─ chunk_stream_id (6 bits) (假设=6)                                             │ │
  │  │  ├─ Message Header (11 bytes, 因为 format=0)                                          │ │
  │  │  │   ┌─ timestamp (3 bytes) = 0x0000AA (示例)                                         │ │
  │  │  │   ├─ message_length (3 bytes) = 0x0100 (示例，256 字节)                             │ │
  │  │  │   ├─ message_type (1 byte) = 0x09 (视频消息)                                        │ │
  │  │  │   └─ msg_stream_id (4 bytes, 小端序) = 1                                           │ │
  │  │  └─ Extended Timestamp (0 or 4 bytes)                                                │ │
  │  │      (假设 timestamp < 0xFFFFFF, 所以没有该字段)                                        │ │
  │  └─────────────────────────────────────────────────────────────────────────────────────┘  │
  │  ┌─────────────────────────────────────────────────────────────────────────────────────┐  │
  │  │ Packet Body (长度=0x0100=256)                                                        │  │
  │  │  ┌─ FrameType(4 bits)=1, CodecID(4 bits)=7 (AVC/H.264)                              │  │
  │  │  ├─ AVC PacketType (1 byte)=1 (NALU)                                                │  │
  │  │  ├─ CompositionTime (3 bytes) = 0x000000                                            │  │
  │  │  └─ H.264 码流数据 (剩余部分)                                                          │  │
  │  └─────────────────────────────────────────────────────────────────────────────────────┘  │
  └───────────────────────────────────────────────────────────────────────────────────────────┘
  ```

  这样，我们通过**Basic Header**→**Message Header**→**(可选)Extended Timestamp**→**Packet Body**逐级拆解，就能够非常详细地理解 RTMP 协议中的消息格式。

  ------

  ## 5. 结语

  通过上述**分层结构图**与**逐字段说明**，我们可以清晰了解到：

  1. RTMP Packet 的最外层是 **Packet Header**(包含 Basic Header, Message Header, Extended Timestamp) 和 **Packet Body**。
  2. **Basic Header** 决定了 `format`(也称 chunk_type) 和 `chunk_stream_id`；同时 `format` 影响 **Message Header** 的大小。
  3. **Message Header** 里包含时间戳(或时间增量)、消息长度、消息类型、流 ID 等关键信息；若时间戳超过 3 字节上限(16777215)，则启用 **Extended Timestamp**。
  4. **Packet Body** 的具体结构依赖 **message_type**。常见音频(8)和视频(9)与 FLV Tag 头格式类似，而命令/元数据等则以 AMF 编码形式进行传输。

  以上内容即为 RTMP 每一级消息格式的详细解析及“文本结构图”示例，希望能帮助你彻底厘清 RTMP 数据包的层级关系和字段结构。









## 推流H.264

[H264视频通过RTMP直播_h.264 直播地址-CSDN博客](https://blog.csdn.net/firehood_/article/details/8783589)

[H264码流中SPS PPS详解 - 知乎](https://zhuanlan.zhihu.com/p/27896239)



### H.264文件推流Demo详解

- 下面的内容将分为三个部分，为你逐个解答所提出的问题。

  ---

  ## 一、结合 demo 代码，详细回答向 RTMP 服务器推流 H.264 数据的流程

  从 `main.cpp` 入手，代码结构和逻辑大致如下（可与示例代码一一对应）：

  1. **`main.cpp`** 中使用了 `CRTMPStream` 类来完成推流：

     ```cpp
     CRTMPStream rtmpSender;
     bool bRet = rtmpSender.Connect("rtmp://localhost/live/livestream");
     rtmpSender.SendH264File("push_dump.h264");
     rtmpSender.Close();
     ```

     - **`Connect()`**：建立到 RTMP 服务器的连接。  
     - **`SendH264File()`**：从本地读取 H.264 文件，并循环读取其中的 NALU（H.264 码流的重要单位），逐一发送到 RTMP 服务器。  
     - **`Close()`**：断开 RTMP 连接并释放资源。

  2. **`CRTMPStream` 类** 在 `RTMPStream.h`/`RTMPStream.cpp` 中实现了推流的主要逻辑：

     - **`Connect(const char* url)`**  
       1. 调用 `RTMP_SetupURL` 来设置推流目标 URL（例如 `rtmp://localhost/live/livestream`）。  
       2. `RTMP_EnableWrite` 设置为可写模式（推流模式）。  
       3. 调用 `RTMP_Connect` 建立与 RTMP 服务器的 TCP 网络连接。  
       4. 调用 `RTMP_ConnectStream` 与 RTMP 服务器开始交互，建立 RTMP 流连接。  
       5. 如果返回成功，则表明与服务器握手并建立了 RTMP 连接，可以进行数据发送。

     - **`SendH264File(const char* pFileName)`**  
       1. 打开本地 H.264 裸流文件，读取到内存缓冲区 `m_pFileBuf` 中。  
       2. 首先 **读取 SPS**（Sequence Parameter Set） 和 **PPS**（Picture Parameter Set） 两个特殊的 NALU，用来获取视频宽、高、Profile 等信息。  
          - 这里使用了 `ReadOneNaluFromBuf(NaluUnit &nalu)` 函数来从内存中解析 NALU。  
       3. 调用 `h264_decode_sps`（定义在 `SpsDecode.h` 中）对 SPS 进行解析，得到视频的宽度（width）和高度（height）等信息，并存储到 `RTMPMetadata` 结构体中。  
       4. 调用 `SendMetadata(&metaData)` 将元数据（视频分辨率、帧率、SPS/PPS 等）先发送给 RTMP 服务器。  
          - RTMP 协议中，视频流（H.264）通常需要先发送 AVC sequence header（也就是 SPS/PPS 头），这样播放端或者服务器端才能正确解码后续的 H.264 画面数据。  
       5. 后续循环读取其他 NALU，并通过 `SendH264Packet` 将每一个 NALU 封装成 RTMP Packet 发送出去。  
          - 如果 NALU 的类型是 0x05（IDR 帧），则认为是关键帧（I 帧）。否则就是普通 P 帧。  
       6. 发送完成后，关闭文件并返回。

     - **`ReadOneNaluFromBuf(NaluUnit &nalu)`**  
       1. 该函数用于从内存缓冲区 `m_pFileBuf` 中找到下一组以 `00 00 00 01` 开头的 H.264 NALU。  
       2. 每找到一组起始码（四字节的 `00 00 00 01`），就继续往后找下一组起始码。下一组起始码出现的位置就是当前 NALU 的结束位置。  
       3. 将 NALU 的类型（低 5 bit：`type = data[0] & 0x1F`）、大小以及数据指针保存到 `nalu` 结构中。更新 `m_nCurPos` 指向下一个 NALU 的开始位置。  
       4. 如果搜索到末尾，也就表示文件读完了。

     - **`SendMetadata(LPRTMPMetadata lpMetaData)`**  
       1. 先通过 AMF 协议打包一个 RTMP type 为 “info/metadata” 的消息，告诉 RTMP 服务器 “我有一些基础信息，如分辨率、帧率、编码类型等”。  
       2. 此外，这里面还构建了一个 AVC sequence header（SPS/PPS）。它对应 “VideoTagHeader + AVCDecoderConfigurationRecord”。  
       3. 先发送一个 RTMP Packet 类型 `RTMP_PACKET_TYPE_INFO`（标识其内容为metadata），再发送一个 RTMP Packet 类型 `RTMP_PACKET_TYPE_VIDEO`（标识其内容为配置头 SPS/PPS）。  
       4. 播放端或服务器收到这两个包后，就能知道后续的 H.264 数据如何解码。

     - **`SendH264Packet(unsigned char *data, unsigned int size, bool bIsKeyFrame, unsigned int nTimeStamp)`**  
       1. 构造 Video Tag，写入关键帧/非关键帧标志、NALU 数据的时间戳、数据大小等。  
       2. 将这些数据打包到 RTMP packet 中，通过 `RTMP_SendPacket(m_pRtmp, &packet, ...)` 发送给服务器。  
       3. RTMP 协议会在服务器或播放器端解析这些包并恢复成原始 H.264 码流。  

  3. **补充说明：**  

     - 在实际推流场景中，除了视频数据（H.264）外，通常还需要推音频数据（AAC 等），在这个 Demo 里并没有演示推音频的过程，但思路相似：**先发送 AAC sequence header（音频配置信息），再循环发送音频帧**。  
     - 另外在直播中，为了减少卡顿或延迟，还会对码率、关键帧间隔进行调优，也会涉及到网络抖动处理、RTMP 重连等逻辑。  
     - 这个 Demo 演示了最基础的 “读文件 - 找 NALU - 发送到 RTMP” 流程，比较适合初学者理解 H.264 码流推送到 RTMP 服务器的工作过程。

  ---

  ## 二、`put_byte()`、`put_be16()`、`put_be24()`、`put_be32()`、`put_be64()`、`put_amf_string()`、`put_amf_double()` 函数的逻辑、作用和意义

  在 `RTMPStream.cpp` 中，可以看到这些函数都属于 “往缓冲区中写入一些特定格式的数据” 的工具函数。它们的主要作用是按照 RTMP/FLV/AMF 等协议规范，将数据写到内存中，使得后面可以通过一次 `SendPacket` 调用发送到服务器。在 RTMP 中，一些字段要求大端（Big Endian）格式，还有一些字段需要按 AMF 规范组织。我们来一一说明：

  1. **`put_byte(char *output, uint8_t nVal)`**  
     - 往内存中写入 1 字节（一个 `uint8_t`）。  
     - RTMP/FLV 协议中有大量 1 字节的标志位或小字段需要写入，此函数就是最简单的写法。  
     - 写完之后返回下一个要写的位置指针。

  2. **`put_be16(char *output, uint16_t nVal)`**  
     - 写入一个 16 位无符号数，且是大端（Big Endian）格式。  
     - 例如：如果 `nVal = 0x1234`，那么写入的两个字节依次是 `0x12`、`0x34`。  
     - FLV、RTMP、AMF 等协议对多字节数据一般要求使用大端序。  

  3. **`put_be24(char *output, uint32_t nVal)`**  
     - 写入 24 位无符号整型，依旧是大端格式。  
     - 比如 `nVal = 0x00123456`，那就写入 `0x12, 0x34, 0x56` 三个字节（更高位 0x00 被抛弃）。  
     - FLV 中常见 “24 bits” 的一些长度字段都会使用这个函数写入。

  4. **`put_be32(char *output, uint32_t nVal)`**  
     - 写入 32 位无符号数，大端格式。  
     - RTMP 在发送包体大小、时间戳等场景也经常需要写 4 字节。

  5. **`put_be64(char *output, uint64_t nVal)`**  
     - 写入 64 位无符号数，大端格式。  
     - 如果一些 AMF/RTMP 需要写 double 或者时间戳扩展，可能要 8 字节。

  6. **`put_amf_string(char *c, const char *str)`**  
     - 先写入一个 16 位（大端）表示字符串长度，然后紧跟字符串本身。  
     - 在 AMF 编码中，字符串的格式往往是 `[2 字节 length][字符串字节序列]`。  
     - 写完后返回下一个可写指针。

  7. **`put_amf_double(char *c, double d)`**  
     - AMF 协议中标识一个 Number 类型时，需要先写一个字节表示 “我是 Number 类型”（`AMF_NUMBER` = 0x00）。  
     - 接下来写 8 个字节的 double。这里为了保持网络序或大端模式，需要进行一些字节反转（把 `double` 的高位字节移到低地址）。  
     - 这样 RTMP/FLV 解析端就能正确还原 double 数值。  

  **总结：** 这些函数的存在就是为了手动构建出符合 RTMP/FLV/AMF 编码格式的数据包。因为 `SendPacket()` 最终是向网络发送一个内存区域，而协议规定了哪些字节放在哪个位置、以大端或小端写入，所以就需要编写这些小工具函数来完成。

  ---

  ## 三、详细解释 `SpsDecode.h` 中每一个函数的逻辑

  `SpsDecode.h` 里最核心的函数是 `h264_decode_sps`，它需要解析 H.264 的 SPS（Sequence Parameter Set）。SPS 用于描述视频的基本编码信息，包括分辨率、级别、Profile 等。为了解析这些内容，需要一些辅助函数：`Ue`、`Se`、`u`。让我们先看看它们都做了什么。

  ### 1. `Ue(BYTE *pBuff, UINT nLen, UINT &nStartBit)`

  - **含义：** 解析 H.264 码流中的 “unsigned exponential Golomb” 编码（即 UE/Golomb 编码）。  
  - **原理：**  
    1. 先数连多少个 “0” bit（记为 nZeroNum），直到遇到一个 “1” bit 停止；  
    2. 跳过这个 “1” bit；  
    3. 再读取 nZeroNum 个比特，把它们当作一个无符号整数（记为 `dwRet`）；  
    4. 最终结果 = `(1 << nZeroNum) - 1 + dwRet`。  
  - **为什么这样？**  
    - H.264 的很多参数（如 pic_width_in_mbs_minus1、pic_height_in_map_units_minus1 等）都用 UE 码表示，它具有一定的熵编码压缩特性。  
    - 通过在码流中读 bit 的方式解码出对应的整型参数。  

  ### 2. `Se(BYTE *pBuff, UINT nLen, UINT &nStartBit)`

  - **含义：** 解析 H.264 码流中的 “signed exponential Golomb” 编码（即 SE/Golomb 编码）。  
  - **原理：**  
    1. 先读取一个 `Ue` 值（无符号）；  
    2. 假设得到的结果是 `UeVal`；  
    3. 计算 `nValue = ceil(UeVal / 2)`；  
    4. 如果 `UeVal` 是偶数，则结果取负号；若是奇数，则结果取正；  
    5. 这样就可以将无符号数映射成一个有符号数，适用于 H.264 中一些有符号参数（如某些偏移量）。  

  ### 3. `u(UINT BitCount, BYTE *buf, UINT &nStartBit)`

  - **含义：** 直接从码流中读取 `BitCount` 个比特位，把它们拼接成一个无符号整型返回。  
  - **原理：**  
    - 循环 BitCount 次，每次将结果左移 1 位，读出当前码流的下一个 bit（`(buf[nStartBit/8] & (0x80 >> (nStartBit % 8))) != 0`)），如果是 1 就加到结果的最低位上。  
    - 读完一个 bit 后，`nStartBit++`。  

  ### 4. `h264_decode_sps(BYTE * buf, unsigned int nLen, int &width, int &height)`

  - **功能：** 解析 SPS，最终得到视频的分辨率 `width`、`height` 等关键参数。  

  - **执行过程概览：**  

    1. 先解析 1 bit 的 `forbidden_zero_bit`、2 bit 的 `nal_ref_idc`、5 bit 的 `nal_unit_type`；如果 `nal_unit_type == 7`，才表示这是一个 SPS。  

    2. 接着读取 `profile_idc`、若干个 flag，以及 `level_idc`，再通过 `Ue` 解析出 `seq_parameter_set_id`。  

    3. 如果 `profile_idc` ∈ {100, 110, 122, 144}，说明是高端 Profile，需要再解析更多参数（如 `chroma_format_idc`、`bit_depth_luma_minus8` 等）。  

    4. 解析完后，得到 `log2_max_frame_num_minus4`、`pic_order_cnt_type` 等。  

    5. 最终重点解析 `pic_width_in_mbs_minus1` 和 `pic_height_in_map_units_minus1`，因为  

       ```
       width = (pic_width_in_mbs_minus1 + 1) * 16
       height = (pic_height_in_map_units_minus1 + 1) * 16
       ```

       H.264 里一个宏块（macroblock, MB）是 16×16 的基本编码单位，所以宽高都是要 *16 才是像素尺寸。  

    6. 返回 `true` 表示解析成功。  

  因为要发送 H.264 裸流，解析 SPS 以获取正确的分辨率等元数据是至关重要的，否则 RTMP 服务器或播放端就不会知道应该如何解码画面的大小。  

  ---

  ## 总结

  1. **向 RTMP 服务器推流 H.264 的主要流程** 是：  
     1) 与 RTMP 服务器建立连接 (`Connect()`)；  
     2) 读取本地 H.264 裸流文件并提取 NALU（`ReadOneNaluFromBuf()`）；  
     3) 解析并发送 SPS/PPS 作为 “AVC sequence header” (`SendMetadata()`)；  
     4) 逐帧发送后续的 P 帧/I 帧 (`SendH264Packet()`)；  
     5) 发送结束或退出时断开连接（`Close()`）。  

  2. **各种写入函数**（`put_byte()`、`put_be16()` 等）**用于拼装特定协议字段**，满足 RTMP/FLV/AMF 等协议对大端、小端、AMF 类型标识的要求。

  3. **SPS 解析**（`SpsDecode.h`）**是 H.264 编码必备**，通过 Golomb 编码函数（`Ue`、`Se`、`u`）可获取到视频的分辨率、Profile 等关键信息。  

  以上便是对整个 Demo 的详细说明与扩展，希望能帮助你更全面地理解从 H.264 裸流到推送 RTMP 服务器的流程，以及相关工具函数的作用和协议对应关系。





### YUV编码后再推流



#### Librtmp推流

- 下面给你提供一个**示例代码**，演示如何将**YUV420P**数据通过**H264Encoder**编码成 H.264，再使用我们先前的 **CRTMPStream** 类推送到 RTMP 服务器进行直播**（低延迟）**。在示例中，我对原有的代码做了适度改动与整合，并加上了较为详细的注释，供你参考和后续实践。

  > **说明**：以下代码仅作演示，实际项目中可能需要根据工程结构拆分到不同文件，并做异常处理、日志、重连、内存管理等更加完善的设计。

  ------

  ## 一、推流 YUV 的大致流程

  1. **打开并读取 YUV 文件**：循环从本地 `xxx.yuv` 文件中读取一帧（width × height × 1.5 字节）到内存缓冲区。
  2. **将 YUV 数据送入 `H264Encoder` 进行 H.264 编码**：调用 `Encode()` 或类似函数获取编码后的 H.264 码流（NALU）。
  3. 建立 RTMP 连接，准备推流：
     - 使用 `CRTMPStream` 的 `Connect()` 方法连接到 RTMP 服务器。
     - 读取（或从编码器 extradata 中获取）SPS、PPS 并通过 `SendMetadata()` 发送给服务器。
  4. 循环发送编码后的 H.264 数据：
     - 对每一帧编码得到的数据，判断是否关键帧（IDR），再调用 `SendH264Packet()` 推送到 RTMP 服务器。
     - 适当控制发送速度（帧率）或根据网络状况进行调度。
  5. **结束推流**：关闭文件、释放资源，调用 `Close()` 断开 RTMP 连接。

  ------

  ## 二、与直接推流 H.264 文件的差别

  - **数据来源**不同：以前的 Demo 直接从 H.264 文件（已编码）读取，找到 NALU 再发送；现在则是从原始 YUV 数据开始，要先调用 FFmpeg 编码器（`H264Encoder`）编码成 H.264 再发送。
  - **需要额外的编码配置**：对帧率、码率、GOP、大多数时还会设置 `preset` “ultrafast”+ `tune` “zerolatency”等参数，尽量减少编码延时。
  - **SPS/PPS 获取方式不同**：
    - 直接推流 H.264 文件时，SPS/PPS 通常是文件里的前两个 NALU。
    - 现在我们从 FFmpeg 编码器的 `extradata` 中获取，并手动剥离或提取出 SPS、PPS。
  - **低延迟优化**：由于是现编现推，在编码器配置里启用低延迟（如 `b_frames=0`, `tune=zerolatency`, `preset=ultrafast`），使编码后能尽快发送。

  ------

  ## 三、示例整合代码

  下面给出一个单文件示例 `main.cpp`，大致演示了从本地读取 YUV420P 数据，用 FFmpeg 进行实时编码，然后用 `CRTMPStream` 推流到 RTMP 服务器。

  > **请注意**：
  >
  > 1. 需要在你的项目中确保 FFmpeg 库和 librtmp 库都已正确链接。
  > 2. 需要把 `H264Encoder.*` 和 `RTMPStream.*` 这两个类正确包含进来。
  > 3. 如果你的 FFmpeg 版本较新，可能需要将废弃的函数替换成对应的新 API。

  复现之前需要把 rtmp_sys.h文件中的

  ```C++
  typedef __int64 off_t;
  ```

  改为:

  ```C++
  #ifndef WIN32
  typedef __int64 off_t;
  #endif
  ```

  不然会导致宏重定义的错误

  

  ```cpp
  /*************************************************************
   * File: main.cpp
   * Description:
   *   1) 从本地读取 YUV420P 文件
   *   2) 使用 H264Encoder 编码成 H.264
   *   3) 使用 CRTMPStream 推流到 RTMP 服务器
   *   4) 尽量优化低延迟
   *
   * Compile and run:
   *   g++ -std=c++11 main.cpp H264Encoder.cpp RTMPStream.cpp -lavcodec -lavutil -lavformat -lz ...
   *   ./a.out
   *************************************************************/
  
  #include <iostream>
  #include <cstdio>
  #include <cstdlib>
  #include <cstring>
  #include <string>
  #include <thread>
  #include <chrono>
  
  #include "H264Encoder.h"      // 你的H264Encoder类
  #include "RTMPStream.h"       // 你的CRTMPStream类
  
  // 这里假设都在同一目录，如果不在，需要自己修改包含路径
  // #include "Properties.h"     // 你需要的 Properties 类
  // #include "dlog.h"           // 如果有需要的日志库
  // ... 其他头文件
  
  int main(int argc, char* argv[])
  {
      // ==== 1. 读取参数及文件名 ====
      // 例如： ./a.out rtmp://localhost/live/livestream input_640x360.yuv 640 360
      if (argc < 5) {
          std::cerr << "Usage: " << argv[0] << " <rtmp_url> <yuv_file> <width> <height>\n";
          return -1;
      }
      std::string rtmpUrl = argv[1];
      std::string yuvFilePath = argv[2];
      int width  = std::stoi(argv[3]);
      int height = std::stoi(argv[4]);
  
      // ==== 2. 打开YUV文件 ====
      FILE* fp_yuv = fopen(yuvFilePath.c_str(), "rb");
      if (!fp_yuv) {
          std::cerr << "Failed to open YUV file: " << yuvFilePath << std::endl;
          return -1;
      }
      std::cout << "Opened YUV file: " << yuvFilePath << "\n";
  
      // ==== 3. 初始化H264编码器 ====
      // 这里使用Properties或自己手动设置参数都可以
      Properties props;
      props.SetProperty("width",  width);
      props.SetProperty("height", height);
      props.SetProperty("fps",    25);            // 目标帧率
      props.SetProperty("bitrate", 800 * 1024);   // 800kbps 仅示例
      props.SetProperty("b_frames", 0);           // 不使用B帧，减低延迟
      props.SetProperty("gop", 25);               // GOP大小=帧率
  
      H264Encoder encoder;
      if (encoder.Init(props) < 0) {
          std::cerr << "H264Encoder init failed.\n";
          fclose(fp_yuv);
          return -1;
      }
      std::cout << "H264Encoder init OK.\n";
  
      // ==== 4. 获取SPS/PPS 信息，用来发送MetaData ====
      // 由于H264Encoder中已经在Init阶段解析了 extradata，所以可以直接getSPS/getPPS
      uint8_t spsData[1024] = {0};
      uint8_t ppsData[1024] = {0};
      int spsLen = 0;
      int ppsLen = 0;
  
      // 如果你的H264Encoder里提供了 getSPS/ getPPS 函数
      // 也可以用 encoder.getSPSData(), getSPSSize() 直接获取
      spsLen = encoder.getSPSSize();
      ppsLen = encoder.getPPSSize();
      if (spsLen > 0) {
          memcpy(spsData, encoder.getSPSData(), spsLen);
      }
      if (ppsLen > 0) {
          memcpy(ppsData, encoder.getPPSData(), ppsLen);
      }
  
      // ==== 5. 初始化RTMP推流 ====
      CRTMPStream rtmpSender;
      bool bRet = rtmpSender.Connect(rtmpUrl.c_str());
      if (!bRet) {
          std::cerr << "Failed to connect RTMP server: " << rtmpUrl << "\n";
          fclose(fp_yuv);
          return -1;
      }
      std::cout << "RTMP Connect OK: " << rtmpUrl << "\n";
  
      // 构建 MetaData
      RTMPMetadata metaData;
      memset(&metaData, 0, sizeof(RTMPMetadata));
      metaData.nSpsLen = spsLen;
      memcpy(metaData.Sps, spsData, spsLen);
      metaData.nPpsLen = ppsLen;
      memcpy(metaData.Pps, ppsData, ppsLen);
      metaData.nWidth     = encoder.getWidth();
      metaData.nHeight    = encoder.getHeight();
      metaData.nFrameRate = 25;  // 与编码器一致
      // 如果有音频，可设置 metaData.bHasAudio = true 等，但此处仅演示视频
      // 发送MetaData(SPS/PPS)给RTMP服务器，让对端知晓解码参数
      rtmpSender.SendMetadata(&metaData);
      std::cout << "Sent MetaData (SPS/PPS) to RTMP server.\n";
  
      // ==== 6. 读YUV数据，编码并推流 ====
      // YUV420P一帧大小 = width * height * 3 / 2
      const int frameSize = width * height * 3 / 2;
      uint8_t* yuvBuffer = new uint8_t[frameSize];
  
      int frameIndex = 0;
      while (true) {
          // 6.1 读取YUV一帧
          int ret = fread(yuvBuffer, 1, frameSize, fp_yuv);
          if (ret < frameSize) {
              std::cout << "End of YUV file or read error.\n";
              break;
          }
  
          // 6.2 使用H264Encoder编码，获取H.264数据
          //     这里演示使用 encoder.Encode(yuv, pts) -> AVPacket*
          AVPacket* packet = encoder.Encode(yuvBuffer, 0 /*pts*/);
          if (!packet) {
              // 编码器可能需要更多数据或者出现了EAGAIN
              // 也可能直接返回空，此处简单处理
              std::cerr << "Encoder returned null packet. skip.\n";
              continue;
          }
  
          // 6.3 拆分AVPacket->data 中的 NALU，并逐个发送
          //     简化做法：一般情况下 FFmpeg 默认会在 packet->data 里放一个或多个NALU
          //     常见情况：NALU 前面带有 00 00 00 01 startcode
          //     我们可以直接调用 rtmpSender.SendH264Packet() 发送这一整块，但需判断关键帧
          //     这里用简单方法：若 nal_unit_type == 5 => keyframe
          bool bIsKeyFrame = false;
          // 大多数时候可以通过 (packet->flags & AV_PKT_FLAG_KEY) 判断
          if (packet->flags & AV_PKT_FLAG_KEY) {
              bIsKeyFrame = true;
          }
  
          // RTMP 推流时，需要去掉多余的 00 00 00 01 start code 头等
          // 在H264Encoder里我们可能已经跳过4字节startcode，也可能保留
          // 这里假定 FFmpeg输出的packet带 startcode，则简单跳过头
          // 具体视 encoder 里 avcodec_send_frame/ avcodec_receive_packet 的实现
          int skipBytes = 0;
          if (packet->size > 4 && packet->data[0] == 0x00 &&
                                  packet->data[1] == 0x00 &&
                                  packet->data[2] == 0x00 &&
                                  packet->data[3] == 0x01) {
              skipBytes = 4; // 跳过startcode
          }
  
          uint8_t* sendData = packet->data + skipBytes;
          int sendSize      = packet->size - skipBytes;
  
          // 6.4 送到 RTMP 流媒体服务器
          unsigned int timestamp = (unsigned int)(frameIndex * 40); // 假设25fps => 40ms 一帧
          
          /* 优化了FFmpeg推流Demo之后，把其中的动态sleep逻辑加入进来，原来是6.6步的机械sleep 40ms */
          auto expected_time = start_time + std::chrono::milliseconds(static_cast<int>(frameIndex * 1000.0 / 25));
      	auto now = std::chrono::steady_clock::now();
      	if (expected_time > now) {
      		std::this_thread::sleep_until(expected_time);
      	}
          
          
          // sendData里可能包含多个NALU(含SPS/PPS/SEI/IDR等)，RTMP可以一次发送
          rtmpSender.SendH264Packet(sendData, sendSize, bIsKeyFrame, timestamp);
  
          // 6.5 释放本次Packet
          av_packet_free(&packet);
  
          // 6.6 控制帧率(若需要做准实时推流)
          //     以25fps为例，sleep ~40ms。如果要超低延迟，可以考虑不sleep，
          //     但是网络+编码缓冲还是会有一定延迟
          // std::this_thread::sleep_for(std::chrono::milliseconds(40));
  
          frameIndex++;
      }
  
      // ==== 7. 结束推流, 释放资源 ====
      delete[] yuvBuffer;
      fclose(fp_yuv);
      rtmpSender.Close();
      std::cout << "RTMP Stream finished.\n";
  
      return 0;
  }
  ```

  ### 代码解读

  1. **读取 YUV**：在 `while(true)` 循环中，使用 `fread()` 从文件中按帧读取固定大小的 YUV420P 数据。大小为 `width * height * 3 / 2`。
  2. **编码**：使用 `encoder.Encode()`，将一帧的 YUV 数据编码为 H.264。这里使用了 `AVPacket* H264Encoder::Encode(uint8_t*, uint64_t pts, const int flush)` 方法，每次返回一个 `AVPacket`。
  3. 拆分/解析 H.264 Packet：在大部分情况下，FFmpeg 编码出来的 `packet->data` 内可能包含一个或多个 H.264 NALU，常见起始码是 `00 00 00 01` 。
     - 如果我们只想简化处理，可以整体发送给 RTMP 推流端，但 RTMP 协议一般也能识别并拆分，只需要注意**起始码**的处理。
  4. **判断关键帧**：常用 `(packet->flags & AV_PKT_FLAG_KEY)` 来判断。关键帧（IDR）对应 RTMP 协议中 `0x17` 标识；非关键帧对应 `0x27`。在 `SendH264Packet()` 内部正是通过布尔值 `bIsKeyFrame` 做此区分。
  5. MetaData：在 YUV->H.264 的场景下，我们要自己从编码器的 `extradata` 里获取 SPS/PPS 并发送，这和读取现有 H.264 文件的 Demo 不同：
     - 后者可以在文件中直接读到 SPS/PPS NALU；
     - 现在则要通过 `H264Encoder` 的 `codec_ctx_->extradata` 或内部函数 `getSPS()/getPPS()` 获取。
  6. **低延迟**：在编码器初始化时，设置 `b_frames=0`、`preset=ultrafast`、`tune=zerolatency` 等参数，可最大程度减少编码缓冲。发送端也要相对减少缓冲/不睡眠，只是这样会增大码率波动，需要平衡。

  ------

  ## 四、总结

  1. 推流 YUV 的流程：

     - 读 YUV → 编码 → 获取 SPS/PPS → RTMP 连接 → 发送 SPS/PPS → 循环发送编码帧 → 结束释放。

  2. 与推流 H.264 文件的区别：

     - 主要是数据源不同。直接推 H.264 文件时已有编码好的一长串 NALU；现在是先自己用 FFmpeg 编码，再推。
     - 需要在编码阶段配置低延迟参数（B 帧数、preset、tune 等）来减少编码缓冲。
     - SPS/PPS 的来源有所区别（文件中 vs. 编码器 extradata）。

  这套思路能够帮助你进一步深入理解从 **YUV → H.264 → RTMP** 的实际操作流程，也为后续加上 **音频**、**多路码率**、**真正的直播服务器端** 等打下良好基础。希望能对你有所帮助，祝你在后续的直播推流学习中一切顺利!







#### FFmpeg推流

[最简单的基于FFmpeg的推流器（以推送RTMP为例）_ffmpeg 推送mov格式视频-CSDN博客](https://blog.csdn.net/leixiaohua1020/article/details/39803457)

[ffmpeg 代码实现rtmp推流到服务器_ffmpeg.autogen 推拉流-CSDN博客](https://blog.csdn.net/zhangpengzp/article/details/89713422)



博客中的代码是推流flv或者H.264的, YUV推流是在GPT写的基础上借鉴了博客的一点思路再让GPT继续优化，使Demo可以以较好的效果运行



- 下面给出一个**示例性**的推流流程示例代码，展示如何使用您已有的 **H264Encoder** 进行编码，然后通过 FFmpeg **6.1** 的 API 进行 RTMP 推流。该示例会包含以下关键步骤：

  1. **读取本地 YUV 数据**
  2. **初始化和使用 H264Encoder 进行编码**
  3. **初始化 FFmpeg 输出（RTMP）**
  4. **循环编码并推流**
  5. **收尾操作（flush、释放资源）**

  > **注意**
  >
  > - 该示例仅作演示，很多健壮性处理（如错误处理、异常情况、边界判断等）在实际项目中还需完善。
  > - 推流时，为了降低延迟，通常会选用 `ultrafast + zerolatency` 等参数、减少/禁用 B 帧、使用较小的缓冲区等。您在 `H264Encoder::Init()` 里已经设置了这些参数；此外，推流端也可以做一些 buffer 调整（如 `fflags=nobuffer` 或 `-tune zerolatency` 等）。
  > - 请确保本示例中 `H264Encoder.h`、`H264Encoder.cpp` 在同一工程内，并且已正确链接 FFmpeg 6.1+ 库。

  ------

  ## 示例代码

  ```cpp
  /**************************************************************
   * File: main_rtmp_push.cpp
   * Description:
   *   1) 读取本地YUV文件（I420或YUV420P格式）
   *   2) 使用H264Encoder编码
   *   3) 使用FFmpeg 6.1 API 推流至RTMP服务器
   **************************************************************/
  
  #include <chrono>
  #include <thread>
  
  extern "C" {
  #include <libavformat/avformat.h>
  #include <libavcodec/avcodec.h>
  #include <libavutil/imgutils.h>
  #include <libavutil/time.h>
  }
  
  #include <iostream>
  #include <fstream>
  #include <string>
  #include <memory>
  #include "H264Encoder.h"
  
  
  /**
   * 读取一帧YUV420P数据
   * @param inFile   已打开的YUV文件输入流
   * @param buffer   存放读取结果的缓冲区指针
   * @param frameSize 一帧YUV所需的字节数： width*height*3/2
   * @return true: 成功读取一帧; false: 文件读取完毕或异常
   */
  bool readOneFrame(std::ifstream &inFile, uint8_t* buffer, int frameSize) {
      if (!inFile.good()) {
          return false;
      }
      inFile.read(reinterpret_cast<char*>(buffer), frameSize);
      if (inFile.gcount() < frameSize) {
          return false;
      }
      return true;
  }
  
  static double r2d(AVRational r)
  {
      return r.num == 0 || r.den == 0 ? 0. : (double)r.num / (double)r.den;
  }
  
  
  int main(int argc, char* argv[])
  {
  
      std::string yuvFile   = "720x480_25fps_420p.yuv";
      int width             = 720;
      int height            = 480;
      std::string rtmpUrl   = "rtmp://localhost/live/livestream";
  
      // 2) 打开YUV文件
      std::ifstream inFile(yuvFile, std::ios::binary);
      if (!inFile.is_open()) {
          std::cerr << "Failed to open input YUV file: " << yuvFile << std::endl;
          return -1;
      }
  
      // 3) 初始化H264编码器
      //    3.1) 先设置Properties
      Properties props;
      props.SetProperty("width",    width);
      props.SetProperty("height",   height);
      props.SetProperty("fps",      25);         // 可根据实际需求调整
      props.SetProperty("bitrate",  600000);     // 600 kbps
      props.SetProperty("gop",      25);         // gop大小
      props.SetProperty("b_frames", 0);          // 不使用B帧，降低延迟
  
      //    3.2) 创建并初始化H264Encoder
      H264Encoder encoder;
      if (encoder.Init(props) < 0) {
          std::cerr << "Encoder Init failed." << std::endl;
          return -1;
      }
  
      // 4) 初始化 FFmpeg RTMP 推流
      //    4.1) 注册所有组件(对于FFmpeg6.x 通常不用手动注册, 这里写着以防环境不同)
      // av_register_all();
      avformat_network_init();
  
      //    4.2) 为输出分配一个AVFormatContext
      //         使用FLV格式 (RTMP通常封装为FLV)
      AVFormatContext* pOutFormatCtx = nullptr;
      avformat_alloc_output_context2(&pOutFormatCtx, nullptr, "flv", rtmpUrl.c_str());
      if (!pOutFormatCtx) {
          std::cerr << "Could not create Output Context." << std::endl;
          return -1;
      }
  
      //    4.3) 新建一个视频流
      AVStream* outStream = avformat_new_stream(pOutFormatCtx, nullptr);
      if (!outStream) {
          std::cerr << "Failed to create new stream." << std::endl;
          avformat_free_context(pOutFormatCtx);
          return -1;
      }
      outStream->time_base = {1, 25}; // 跟编码器配置保持一致, 或者用 encoder.getCodecCtx()->time_base
  
      //    4.4) 复制编码器参数到输出流的codecpar (H264 extradata 同时要传过去)
      //         (如果后续有重新打开编码器的需求, 可以先avcodec_parameters_from_context)
      AVCodecContext* enc_ctx = encoder.getCodecCtx();
      int ret = avcodec_parameters_from_context(outStream->codecpar, enc_ctx);
      if (ret < 0) {
          std::cerr << "Failed to copy codec parameters from encoder context." << std::endl;
          avformat_free_context(pOutFormatCtx);
          return -1;
      }
      outStream->codecpar->codec_tag = 0; // some FLV encoders want this to be 0
  
      //    4.5) 打开输出的IO (如果是RTMP, 这里会进行网络连接)
      if (!(pOutFormatCtx->oformat->flags & AVFMT_NOFILE)) {
          ret = avio_open(&pOutFormatCtx->pb, rtmpUrl.c_str(), AVIO_FLAG_WRITE);
          if (ret < 0) {
              std::cerr << "Failed to open rtmp URL output." << std::endl;
              avformat_free_context(pOutFormatCtx);
              return -1;
          }
      }
  
      //    4.6) 写文件头（即向 RTMP 服务器发送Header）
      ret = avformat_write_header(pOutFormatCtx, nullptr);
      if (ret < 0) {
          std::cerr << "Error occurred when writing header to output URL." << std::endl;
          if (!(pOutFormatCtx->oformat->flags & AVFMT_NOFILE)) {
              avio_close(pOutFormatCtx->pb);
          }
          avformat_free_context(pOutFormatCtx);
          return -1;
      }
  
      // 5) 循环读取YUV数据 -> 编码 -> 推流
      //    一帧大小: YUV420P: width * height * 3/2
      int frameSize = width * height * 3 / 2;
      std::unique_ptr<uint8_t[]> yuvBuf(new uint8_t[frameSize]);
  
      int64_t frameIndex = 0;
      auto start_time = std::chrono::steady_clock::now();
      while (true) {
          // 5.1) 从文件读取一帧YUV
          if (!readOneFrame(inFile, yuvBuf.get(), frameSize)) {
              std::cout << "Reach end of YUV file or read error.\n";
              break;
          }
  
          // 5.2) 进行H264编码，返回AVPacket*
          //      这里调用的是: AVPacket* H264Encoder::Encode(uint8_t *yuv, uint64_t pts, const int flush)
          AVPacket* pkt = encoder.Encode(yuvBuf.get(), /*pts=*/frameIndex, /*flush=*/0);
          if (!pkt) {
              // 可能是编码器内部缓冲没产出或者错误
              // 继续读下一帧
              frameIndex++;
              continue;
          }
  
          // 5.3) 推流(把packet写到RTMP服务器)
          //      需要注意时间戳（dts/pts 基于 outStream->time_base）
          pkt->stream_index = outStream->index;
          pkt->pts = frameIndex;
          pkt->dts = frameIndex;
          pkt->duration = 1;  // 每帧1个时间单位
          frameIndex++;
  
          // 时间基转换: 如果编码器的time_base跟outStream->time_base不一致时，需要用av_rescale_q
          pkt->pts = av_rescale_q(pkt->pts, enc_ctx->time_base, outStream->time_base);
          pkt->dts = av_rescale_q(pkt->dts, enc_ctx->time_base, outStream->time_base);
  
  
          // 控制推流速率以匹配真实时间
          /* 借鉴博客的思路让GPT优化的结果。使用原博客的代码反而无法拉流，原因未知 */
          auto expected_time = start_time + std::chrono::milliseconds(static_cast<int>(frameIndex * 1000.0 / 25));
          auto now = std::chrono::steady_clock::now();
          if (expected_time > now) {
              std::this_thread::sleep_until(expected_time);
          }
  
          ret = av_interleaved_write_frame(pOutFormatCtx, pkt);
          if (ret < 0) {
              std::cerr << "Error muxing packet. ret=" << ret << std::endl;
              av_packet_free(&pkt);
              break;
          }
          av_packet_free(&pkt);
      }
  
      // 6) 文件读取完毕后, flush编码器(若编码器内部还有缓冲帧)
      //    6.1) 这里调一下Encode传参 flush=1, 看是否还剩余数据
      while (true) {
          AVPacket* pkt = encoder.Encode(nullptr, 0, /*flush=*/1);
          if (!pkt) {
              break; // 编码器缓存flush完毕
          }
          pkt->stream_index = outStream->index;
          pkt->pts = frameIndex;
          pkt->dts = frameIndex;
          pkt->duration = 1;
          frameIndex++;
  
          ret = av_interleaved_write_frame(pOutFormatCtx, pkt);
          if (ret < 0) {
              std::cerr << "Error muxing flush packet. ret=" << ret << std::endl;
              av_packet_free(&pkt);
              break;
          }
          av_packet_free(&pkt);
      }
  
      // 7) 写文件尾
      av_write_trailer(pOutFormatCtx);
  
      // 8) 资源释放
      if (!(pOutFormatCtx->oformat->flags & AVFMT_NOFILE)) {
          avio_close(pOutFormatCtx->pb);
      }
      avformat_free_context(pOutFormatCtx);
      inFile.close();
  
      std::cout << "RTMP push finished." << std::endl;
      return 0;
  }
  
  ```

  ------

  ### 主要流程解析

  1. **初始化编码器**
     - 先创建并填充 `Properties` 对象：设置分辨率、帧率、码率、GOP 大小、B 帧数量等。
     - 调用 `encoder.Init(props)` 完成内部初始化，如 `avcodec_find_encoder(AV_CODEC_ID_H264)`、`avcodec_open2` 等。
  2. **读取 YUV 文件**
     - 在循环中，从文件中读取一帧完整的 YUV420P 数据（大小为 `width * height * 3 / 2`），填充到缓冲区 `yuvBuf`。
     - 一旦无法成功读取满一帧（例如文件结尾），就停止循环。
  3. **编码**
     - 将读取到的YUV指针 `yuvBuf` 传给 `encoder.Encode(yuv, pts, flush=0)`。
     - 如果编码产生了输出 `AVPacket*`，说明成功编码了一帧压缩数据。若返回 `nullptr`，说明编码器目前无法输出（或有异常），此时直接读取下一帧。
     - 当文件读取完毕后，再通过多次调用 `Encode(nullptr, 0, flush=1)` 将编码器内部的缓冲帧（如B帧）全部释放出来。
  4. **RTMP 推流**
     - 通过 `avformat_alloc_output_context2` 创建输出 `AVFormatContext`，格式是 `flv`，URL 是 `rtmp://...`。
     - 为输出创建一个 `AVStream`，将编码器的参数 (`codecpar`) 拷贝进去。
     - 打开网络 IO (`avio_open`)，然后写文件头 (`avformat_write_header`)。
     - 对每个编码后的 `AVPacket`，调用 `av_interleaved_write_frame` 写到 RTMP 服务器。
     - 完成后，写文件尾 (`av_write_trailer`)，并关闭 `AVIOContext`、释放 `AVFormatContext`。

  ------

  ### 低延迟注意事项

  - **编码端**：
    - 您在 `H264Encoder::Init()` 里设置了 `ultrafast + zerolatency`，并且 `b_frames=0`，已经是较常见的超低延迟配置。
    - `encoder` 里把 `max_b_frames=0`，可以减少编码延时。
    - 如果需要可在 `Properties` 里自定义更多参数（如 `keyint=...` 等）来调节。
  - **推流端**：
    - 若 RTMP 服务器端也有缓冲设置，需要在服务端或 FFmpeg 推流时使用适当选项（如 `-fflags nobuffer` 等）减少缓冲积压。
    - 另外可以注意网络环境是否稳定，网络抖动也会导致一定延迟。
  - **帧率 & 分辨率**：
    - 如果希望更低的端到端延迟，可以考虑降低分辨率、提高编码效率，从而减小码率、减少网络发送的时延。

  ------

  以上即为一个**完整的参考示例**，可帮助您快速入门并搭建**YUV -> H.264 -> RTMP 推流**的基础流程。











## 推流AAC



[FFmpeg —— 14.示例程序（八）：音频编码器（PCM编码为AAC）_ffmpeg pcm一帧的长度-CSDN博客](https://blog.csdn.net/guoyunfei123/article/details/105615102)

### 概念

 

#### 音频PTS

[音视频录入的pts和dts问题_pkt.dts-CSDN博客](https://blog.csdn.net/zhouyongku/article/details/38510747)

- 在将PCM编码为AAC并进行RTMP推流的过程中，计算PCM的PTS（Presentation Time Stamp，显示时间戳）和时间戳（通常也指PTS或DTS，Decoding Time Stamp，解码时间戳）是至关重要的，它们决定了音频帧在播放器端的正确播放顺序和时间。

  由于音频编码的特性，通常情况下，**AAC帧的PTS和DTS是相等的**，因为音频帧的解码顺序和播放顺序通常是一致的。因此，我们主要关注如何计算AAC帧的PTS，而这个PTS是基于其对应的原始PCM数据的时间信息计算出来的。

  以下是计算PCM对应的AAC帧PTS的步骤和方法，基于你的PCM文件信息 "48000_2_s16le.pcm"：

  **理解PCM文件信息：**

  * **采样率 (Sample Rate):** 48000 Hz (每秒 48000 个采样点)
  * **声道数 (Channels):** 2 (立体声)
  * **采样格式 (Sample Format):** s16le (16-bit signed little-endian)

  **计算单个PCM采样点的时间间隔：**

  每个采样点的时间间隔为 `1 / 采样率` 秒。
  对于你的PCM文件，每个采样点的时间间隔为 `1 / 48000` 秒。

  **确定AAC编码器的帧大小（Frame Size）：**

  AAC编码器会将一定数量的PCM采样点编码成一个AAC帧。AAC的帧大小并不是固定的，取决于编码器的配置和使用的Profile。常见的AAC帧大小对应的PCM采样点数量有：

  * **1024 采样点/帧 (MPEG-4 AAC LC)：** 这是比较常见的配置。

  你需要了解你使用的AAC编码器的帧大小配置。

  **计算AAC帧的持续时间：**

  假设你的AAC编码器使用 1024 采样点/帧，那么每个AAC帧对应的PCM数据的持续时间为：

  `AAC帧持续时间 = 帧大小 (采样点) / 采样率 (采样点/秒)`
  `AAC帧持续时间 = 1024 / 48000 秒 ≈ 0.021333 秒 ≈ 21.333 毫秒`

  **计算AAC帧的PTS：**

  AAC帧的PTS代表了该帧所包含的音频数据的播放起始时间。在推流过程中，我们需要为每个编码后的AAC帧打上相应的PTS。

  **方法一：基于已处理的PCM采样点数量**

  1. **维护一个已处理的PCM采样点计数器。**  每次编码一个AAC帧，就将对应的PCM采样点数量加到计数器上。

  2. **计算当前AAC帧的PTS：**
     `PTS (秒) = 已处理的PCM采样点数量 / 采样率`
     `PTS (毫秒) = (已处理的PCM采样点数量 / 采样率) * 1000`

     例如，编码第一个AAC帧（假设对应 1024 个PCM采样点）：
     `已处理的PCM采样点数量 = 1024`
     `PTS (毫秒) = (1024 / 48000) * 1000 ≈ 21.333 毫秒`

     编码第二个AAC帧：
     `已处理的PCM采样点数量 = 1024 + 1024 = 2048`
     `PTS (毫秒) = (2048 / 48000) * 1000 ≈ 42.667 毫秒`

  **方法二：基于已编码的AAC帧数量**

  1. **维护一个已编码的AAC帧计数器。**

  2. **计算当前AAC帧的PTS：**
     `PTS (毫秒) = 已编码的AAC帧数量 * AAC帧持续时间 (毫秒)`

     例如，编码第一个AAC帧：
     `已编码的AAC帧数量 = 1`
     `PTS (毫秒) = 1 * 21.333 ≈ 21.333 毫秒`

     编码第二个AAC帧：
     `已编码的AAC帧数量 = 2`
     `PTS (毫秒) = 2 * 21.333 ≈ 42.667 毫秒`

  **RTMP时间戳：**

  在将AAC帧封装到RTMP消息（通常是AudioData消息）时，需要设置RTMP的时间戳。这个时间戳通常就是AAC帧的PTS，以毫秒为单位。

  **代码示例（概念性，使用伪代码）：**

  ```python
  sample_rate = 48000
  channels = 2
  sample_format = "s16le"
  
  # 假设 AAC 编码器帧大小为 1024 采样点
  aac_frame_size_samples = 1024
  aac_frame_duration_ms = (aac_frame_size_samples / sample_rate) * 1000
  
  processed_samples = 0
  encoded_aac_frames = 0
  
  # 循环读取 PCM 数据并进行编码
  while True:
      pcm_data = read_pcm_data(aac_frame_size_samples * channels * 2) # 读取一个 AAC 帧对应的 PCM 数据 (2 bytes/sample)
      if not pcm_data:
          break
  
      # 进行 AAC 编码
      aac_frame = encode_to_aac(pcm_data)
  
      # 计算 PTS
      # 方法一：基于已处理的 PCM 采样点
      pts_ms_method1 = (processed_samples / sample_rate) * 1000
  
      # 方法二：基于已编码的 AAC 帧数量
      pts_ms_method2 = encoded_aac_frames * aac_frame_duration_ms
  
      # 将 AAC 帧封装到 RTMP 消息并设置时间戳
      rtmp_timestamp = int(pts_ms_method2) # 通常使用整数毫秒
  
      send_rtmp_audio_message(aac_frame, rtmp_timestamp)
  
      processed_samples += aac_frame_size_samples
      encoded_aac_frames += 1
  ```

  **关键点和注意事项：**

  * **起始时间戳：** 第一个AAC帧的PTS通常从0开始，或者根据需要设置一个起始时间。
  * **时间戳的单调递增：** RTMP时间戳必须是单调递增的，否则播放器可能会出现问题。
  * **精度：** 时间戳的精度通常是毫秒级的。
  * **与视频同步：** 如果同时推流音频和视频，需要确保音视频的时间戳同步，以避免音画不同步的问题。这通常涉及到更复杂的同步机制。
  * **编码器配置：** 不同的AAC编码器可能具有不同的帧大小和配置，需要根据实际情况进行调整。
  * **读取PCM数据的速率：**  你需要按照一定的速率读取PCM数据并进行编码，以保证推流的实时性。

  **总结：**

  计算PCM到AAC的PTS的关键在于理解AAC帧与原始PCM数据之间的对应关系。通过计算每个AAC帧所代表的PCM数据的起始时间，并将其转换为毫秒级的时间戳，就可以正确地设置RTMP的时间戳，从而保证音频在播放器端的正确播放。选择哪种计算方法取决于你的具体实现和偏好，但核心思想是一致的：追踪已处理的PCM数据量或已编码的AAC帧数量，并结合采样率和帧大小来计算时间戳。





### AAC编码

[FFmpeg —— 14.示例程序（八）：音频编码器（PCM编码为AAC）_ffmpeg pcm一帧的长度-CSDN博客](https://blog.csdn.net/guoyunfei123/article/details/105615102)





### AAC推流

AAC推流代码没有找到合适的博客和开源代码。由GPT-o1根据YUV、H264推流代码修改而来。PCM推流和AAC推流流程基本相同，多了编码这一步，因此不再额外整理。

- ### **AAC推流的整体流程**

  1. **初始化和连接RTMP服务器**
  2. **发送音视频元数据（onMetaData）**
  3. **发送音频序列头（Audio Sequence Header）**
  4. **解析AAC文件并发送音频数据包**
  5. **断开RTMP连接**

  下面逐步详细介绍每个步骤，并结合代码进行说明。

  ------

  ### **1. 初始化和连接RTMP服务器**

  **协议步骤说明：**

  在推流开始前，需要建立与RTMP服务器的连接。这包括初始化RTMP库、设置RTMP URL、启用写入功能，并连接到服务器的指定流。

  **相关代码实现：**

  ```cpp
  bool CRTMPStream::Connect(const char* url)
  {
      if(RTMP_SetupURL(m_pRtmp, (char*)url) < 0)
      {
          return FALSE;
      }
      RTMP_EnableWrite(m_pRtmp);
      if(RTMP_Connect(m_pRtmp, NULL) < 0)
      {
          return FALSE;
      }
      if(RTMP_ConnectStream(m_pRtmp, 0) < 0)
      {
          return FALSE;
      }
      return TRUE;
  }
  ```

  **代码说明：**

  - **RTMP_SetupURL**：配置RTMP连接的URL地址。
  - **RTMP_EnableWrite**：启用写入模式，允许推流。
  - **RTMP_Connect**：建立与RTMP服务器的连接。
  - **RTMP_ConnectStream**：连接到指定的流（stream）。

  ------

  ### **2. 发送音视频元数据（onMetaData）**

  **协议步骤说明：**

  在推流开始时，需要发送包含音视频相关参数的元数据（onMetaData），以便接收端了解音视频的基本配置。这包括音频的采样率、声道数、编码格式等信息。

  **相关代码实现：**

  ```cpp
  bool CRTMPStream::SendMetadata(LPRTMPMetadata lpMetaData)
  {
      if(lpMetaData == NULL)
      {
          return false;
      }
      char body[2048] = {0}; // 增加缓冲区大小以适应音视频元数据
      char * p = (char *)body;
      p = put_byte(p, AMF_STRING );
      p = put_amf_string(p , "@setDataFrame" );
  
      p = put_byte( p, AMF_STRING );
      p = put_amf_string( p, "onMetaData" );
  
      p = put_byte(p, AMF_OBJECT );
  
      // Audio MetaData
      if(lpMetaData->bHasAudio)
      {
          p = put_amf_string(p, "hasAudio");
          p = put_byte(p, AMF_BOOLEAN);
          *p++ = lpMetaData->bHasAudio ? 0x01 : 0x00;
  
          p = put_amf_string(p, "audiosamplerate");
          p = put_amf_double(p, lpMetaData->nAudioSampleRate);
  
          p = put_amf_string(p, "audiosamplesize");
          p = put_amf_double(p, lpMetaData->nAudioSampleSize);
  
          p = put_amf_string(p, "audiochannels");
          p = put_amf_double(p, lpMetaData->nAudioChannels);
  
          p = put_amf_string(p, "audiocodecid");
          p = put_amf_double(p, FLV_CODECID_AAC);
      }
  
      p = put_amf_string(p, "" );
      p = put_byte( p, AMF_OBJECT_END  );
  
      // 发送MetaData包
      SendPacket(RTMP_PACKET_TYPE_INFO, (unsigned char*)body, p - body, 0);
  
      // 发送音频MetaData的AudioSpecificConfig
      if(lpMetaData->bHasAudio)
      {
          int i = 0;
          char audioBody[4] = {0};
  
          /* AAC sequence header */
          // body header
          audioBody[i++] = 0xAF; // 1010 (AAC) 11 (44kHz) 1 (16bit) 1 (stereo) - 实际根据AudioSpecificConfig配置
          audioBody[i++] = 0x00; // AACPacketType: 0 (sequence header)
          // body data
          audioBody[i++] = lpMetaData->AudioSpecCfg[0];
          audioBody[i++] = lpMetaData->AudioSpecCfg[1];
  
          // 发送音频MetaData
          SendPacket(RTMP_PACKET_TYPE_AUDIO, (unsigned char*)audioBody, i, 0);
      }
  
      return true;
  }
  ```

  **代码说明：**

  - **构建AMF编码的onMetaData数据包**：

    - 使用AMF（Action Message Format）编码格式，构建包含音频参数的对象。
    - 包含字段如`hasAudio`、`audiosamplerate`、`audiosamplesize`、`audiochannels`、`audiocodecid`等。

  - **发送MetaData包**：

    - 使用`SendPacket`函数发送构建好的MetaData数据包到RTMP服务器。

  - **发送音频序列头（Audio Sequence Header）**：

    - 构建包含`AudioSpecificConfig`的音频序列头。
    - `0xAF`表示AAC音频数据，`0x00`表示这是序列头。
    - 后续两个字节为`AudioSpecificConfig`，包含音频的编码配置。

    `AudioSpecificConfig` 的信息是在 `CRTMPStream::SendAACFile()` 函数中获取的

    ```c++
    ADTSHeader adtsHeader;
    if(!ParseADTSHeader(m_pFileBuf, m_nFileBufSize, adtsHeader))
    {
        printf("ERROR: Parse ADTS Header fail!\n");
        return FALSE;
    }
    
    // 设置音频元数据
    metaData.bHasAudio = true;
    metaData.nAudioSampleRate = GetSamplingFrequency(adtsHeader.samplingFrequencyIndex);
    metaData.nAudioSampleSize = 16; // 假设16位
    metaData.nAudioChannels = adtsHeader.channelConfiguration;
    metaData.AudioSpecCfg[0] = adtsHeader.AudioSpecificConfig[0];
    metaData.AudioSpecCfg[1] = adtsHeader.AudioSpecificConfig[1];
    ```

    而在 `ParseADTSHeader` 函数中

    ```c++
    // 解析ADTS头部字段
    header.profile = ((data[2] & 0xC0) >> 6) + 1; // profile: 1~4
    header.samplingFrequencyIndex = (data[2] & 0x3C) >> 2; // 采样率索引
    header.channelConfiguration = ((data[2] & 0x01) << 2) | ((data[3] & 0xC0) >> 6); // 声道数
    
    // 提取AudioSpecificConfig（前两个字节）
    // AudioSpecificConfig = profile (5 bits) + samplingFrequencyIndex (4 bits) + channelConfiguration (4 bits)
    header.AudioSpecificConfig[0] = ((header.profile & 0x07) << 3) | ((header.samplingFrequencyIndex & 0x0F) >> 1);
    header.AudioSpecificConfig[1] = ((header.samplingFrequencyIndex & 0x01) << 7) | ((header.channelConfiguration & 0x0F) << 3);
    ```

    由这上面的代码从AAC文件中的ADTS头部中获取


  ------

  ### **3. 发送音频序列头（Audio Sequence Header）**

  **协议步骤说明：**

  音频序列头用于传递音频的编码配置信息（`AudioSpecificConfig`），接收端根据这些信息进行音频解码。

  **相关代码实现：**

  ```cpp
  // 在 SendMetadata 函数中已经实现发送音频序列头的逻辑
  if(lpMetaData->bHasAudio)
  {
      int i = 0;
      char audioBody[4] = {0};
  
      /* AAC sequence header */
      // body header
      audioBody[i++] = 0xAF; // 1010 (AAC) 11 (44kHz) 1 (16bit) 1 (stereo) - 实际根据AudioSpecificConfig配置
      audioBody[i++] = 0x00; // AACPacketType: 0 (sequence header)
      // body data
      audioBody[i++] = lpMetaData->AudioSpecCfg[0];
      audioBody[i++] = lpMetaData->AudioSpecCfg[1];
  
      // 发送音频MetaData
      SendPacket(RTMP_PACKET_TYPE_AUDIO, (unsigned char*)audioBody, i, 0);
  }
  ```

  **代码说明：**

  - **构建音频序列头**：
    - `0xAF`：表示AAC音频数据，采样率、声道数等由`AudioSpecificConfig`确定。
    - `0x00`：表示这是序列头而非实际音频帧。
    - 后续两个字节为`AudioSpecificConfig`，由ADTS头解析得到。
  - **发送音频序列头**：
    - 使用`SendPacket`函数将构建好的音频序列头发送到RTMP服务器。

  ------

  ### **4. 解析AAC文件并发送音频数据包**

  **协议步骤说明：**

  AAC文件通常包含ADTS头部，用于描述每帧音频数据的配置和长度。在推流过程中，需要解析每个ADTS帧，去除ADTS头部，提取AAC帧数据，并按照RTMP协议发送音频数据包。

  **相关代码实现：**

  #### **4.1 解析ADTS头部**

  ```cpp
  struct ADTSHeader
  {
      bool valid;
      int profile;
      int samplingFrequencyIndex;
      int channelConfiguration;
      int frameLength;
      unsigned char AudioSpecificConfig[2];
  };
  
  // 辅助函数：解析ADTS头部
  bool ParseADTSHeader(const unsigned char* data, int dataLen, ADTSHeader& header)
  {
      if (dataLen < 7) {
          header.valid = false;
          return false;
      }
  
      // 检查同步字. 检查前12个比特位是否都为1, 若不是则不是有效的ADTS头部
      if (data[0] != 0xFF || (data[1] & 0xF0) != 0xF0) {
          header.valid = false;
          return false;
      }
  
      // 解析ADTS头部字段
      /*  Profile（音频配置文件）: 第3个字节的高2位（bits 16-17）
      	0xC0 : 1100 0000
      	data[2] & 0xC0 : xx00 0000
      	>> 6 : 0000 00xx
      	+1 : DTS中的profile值范围为0-3，实际对应1-4（例如，0表示Main，1表示LC等）
      */
      header.profile = ((data[2] & 0xC0) >> 6) + 1; // profile: 1~4
      /*  Sampling Frequency Index（采样率索引）: 第3个字节的中间4位（bits 18-21）
      	0x3C : 0011 1100
      	data[2] & 0x3C : 00xx xx00
      	>> 2 : 0000 xxxx
      	采样率索引对应具体的采样率（如索引4对应44100Hz）
      */
      header.samplingFrequencyIndex = (data[2] & 0x3C) >> 2; // 采样率索引
      /*  Channel Configuration（声道配置）: 第3个字节的最低1位（bit 22）和第4个字节的高2位（bits 23-24）
      	0x01 : 0000 0001
      	data[2] & 0x01 : 0000 000x
      	<< 2 : 0000 0x00
      	data[3] & 0xC0 : xx00 0000
      	>> 6 : 0000 00xx
      	0000 0x00 | 0000 00xx : 0000 0xxx
      	将两部分通过|操作符组合，得到3位的声道配置（通常0-7，表示不同的声道数）
      */
      header.channelConfiguration = ((data[2] & 0x01) << 2) | ((data[3] & 0xC0) >> 6); // 声道数
  
      // 解析帧长度
      /*	第4-6个字节的特定位
      	data[3] & 0x03 : 0000 00xx
      	<< 11 : xx00 0000 0000 0
      	data[4] << 3: yyyy yyyy -> yyyy yyyy 000 (yy yyyy yy00 0)
      	data[5] & 0xE0 : zzz0 0000
      	>> 5 : 0000 0zzz
      	xx00 0000 0000 0 | yyyy y000 | 0000 0zzz : xxyy yyyy yyzz z
      	三部分通过|操作符组合，得到13位的帧长度，表示整个ADTS帧的长度（包括头部和数据）
      */
      header.frameLength = ((data[3] & 0x03) << 11) |
                           (data[4] << 3) |
                           ((data[5] & 0xE0) >> 5);
  
      // 提取AudioSpecificConfig（前两个字节）
      /*	(header.profile & 0x07) << 3 : 00xx x000
      	(header.samplingFrequencyIndex & 0x0F) >> 1 : 0000 0yyy
      	A | B : 00xx xyyy
      */
      header.AudioSpecificConfig[0] = ((header.profile & 0x07) << 3) | ((header.samplingFrequencyIndex & 0x0F) >> 1);
      /*	(header.samplingFrequencyIndex & 0x01) << 7 : y000 0000
      	(header.channelConfiguration & 0x0F) << 3) : 0zzz z000
      	C | D : yzzz z000
      */
      header.AudioSpecificConfig[1] = ((header.samplingFrequencyIndex & 0x01) << 7) | ((header.channelConfiguration & 0x0F) << 3);
      /*	AudioSpecificConfig[2] : 
      	00xx xyyy yzzz z000
      	00xx x : audioObjectType
      	yyy y : sampleFrequencyIndex
      	zzz z : channelConfiguration
      	0 : frameLengthFlag
      	0 : dependsOnCoreCoder
      	0 : extensionFlag*/
  
      header.valid = true;
      return true;
  }
  
  // 辅助函数：获取采样率
  int GetSamplingFrequency(int samplingFrequencyIndex)
  {
      static const int samplingFrequencies[] = {
          96000, // 0
          88200, // 1
          64000, // 2
          48000, // 3
          44100, // 4
          32000, // 5
          24000, // 6
          22050, // 7
          16000, // 8
          12000, // 9
          11025, // 10
          8000,  // 11
          7350,  // 12
          0,     // 13
          0,     // 14
          0      // 15
      };
  
      if (samplingFrequencyIndex < 0 || samplingFrequencyIndex > 12)
          return 44100; // 默认采样率
  
      return samplingFrequencies[samplingFrequencyIndex];
  }
  ```

  **代码说明：**

  - **ADTSHeader结构体**：用于存储解析后的ADTS头部信息，包括`profile`、`samplingFrequencyIndex`、`channelConfiguration`、`frameLength`和`AudioSpecificConfig`。

  - **ParseADTSHeader函数**：

    - **同步字检测**：检查前两个字节是否为`0xFFF`，确保是有效的ADTS帧。

    - **字段解析**：提取`profile`、`samplingFrequencyIndex`、`channelConfiguration`和`frameLength`。

      > | 字段                     | 比特数 | 说明                                                |
      > | ------------------------ | ------ | --------------------------------------------------- |
      > | syncword                 | 12     | 所有位必须为1，即0xFFF                              |
      > | ID                       | 1      | 0代表MPEG-4，1代表MPEG-2                            |
      > | layer                    | 2      | 所有位必须为0                                       |
      > | protection_absent        | 1      | 1代表没有CRC，0代表有                               |
      > | profile                  | 2      | 配置级别                                            |
      > | sampling_frequency_index | 4      | 标识使用的采样频率，具体见下表                      |
      > | private_bit              | 1      | see ISO/IEC 11172-3, subclause 2.4.2.3 (Table 8)    |
      > | channel_configuration    | 3      | 取值为0时，通过inband的PCE设置channel configuration |
      > | original/copy            | 1      | 编码时设置为0，解码时忽略                           |
      > | home                     | 1      | 编码时设置为0，解码时忽略                           |
      >
      > [深入解析AAC音频编码技术：SBR与PS提升压缩效率,-CSDN博客](https://blog.csdn.net/ProgramNovice/article/details/137208177)

    - **AudioSpecificConfig生成**：根据解析出的字段计算`AudioSpecificConfig`，用于音频序列头的构建。

      ![](D:/OneDrive/Work_and_learn/Code/CPP/CPPLearn/AudioandVideo/15-RTMP流媒体实战/pics/AAC sequence header.png)

      在`ParseADTSHeader()` 中:

      ```c++
      	// AudioSpecificConfig = profile (5 bits) + samplingFrequencyIndex (4 bits) + channelConfiguration (4 bits)
      	header.AudioSpecificConfig[0] = ((header.profile & 0x07) << 3) | ((header.samplingFrequencyIndex & 0x0F) >> 1);
      
      	header.AudioSpecificConfig[1] = ((header.samplingFrequencyIndex & 0x01) << 7) | ((header.channelConfiguration & 0x0F) << 3);
      ```

      这两行用于解析 `AudioSpecificConfig` 所需要的信息

  - **GetSamplingFrequency函数**：根据`samplingFrequencyIndex`返回实际的采样率值。

  #### **4.2 发送AAC数据包**

  ```cpp
  bool CRTMPStream::SendAACPacket(unsigned char *data, unsigned int size, unsigned int nTimeStamp)
  {
      if(data == NULL || size == 0)
      {
          return false;
      }
  
      unsigned char *body = new unsigned char[size + 2];
      int i = 0;
  
      // body header
      body[i++] = 0xAF; // 1010 (AAC) 11 (采样率索引) 1 (16bit) 1 (声道数)
      body[i++] = 0x01; // AACPacketType: 1 (raw AAC frame)
  
      // body data
      memcpy(&body[i], data, size);
  
      bool bRet = SendPacket(RTMP_PACKET_TYPE_AUDIO, body, i + size, nTimeStamp);
  
      delete[] body;
  
      return bRet;
  }
  ```

  **代码说明：**

  - **构建音频数据包**：
    - `0xAF`：表示AAC音频数据，具体的采样率索引、位深和声道数由`AudioSpecificConfig`确定。
    - `0x01`：表示这是一个raw AAC帧（非序列头）。
    - 后续数据为实际的AAC音频帧数据。
  - **发送音频数据包**：
    - 使用`SendPacket`函数将构建好的音频数据包发送到RTMP服务器。

  #### **4.3 解析并发送AAC文件**

  ```cpp
  bool CRTMPStream::SendAACFile(const char *pFileName)
  {
      if(pFileName == NULL)
      {
          printf("ERROR: 文件路径为空!\n");
          return FALSE;
      }
      FILE *fp = fopen(pFileName, "rb");
      if(!fp)
      {
          printf("ERROR: 打开文件 %s 失败!\n", pFileName);
          return FALSE;
      }
      fseek(fp, 0, SEEK_SET);
      m_nFileBufSize = fread(m_pFileBuf, sizeof(unsigned char), FILEBUFSIZE, fp);
      if(m_nFileBufSize >= FILEBUFSIZE)
      {
          printf("警告: 文件大小超过 BUFSIZE\n");
      }
      fclose(fp);
  
      RTMPMetadata metaData;
      memset(&metaData, 0, sizeof(RTMPMetadata));
  
      // 解析第一个ADTS头部，获取AudioSpecificConfig
      ADTSHeader adtsHeader;
      if(!ParseADTSHeader(m_pFileBuf, m_nFileBufSize, adtsHeader))
      {
          printf("ERROR: 解析ADTS头部失败!\n");
          return FALSE;
      }
  
      // 设置音频元数据
      metaData.bHasAudio = true;
      metaData.nAudioSampleRate = GetSamplingFrequency(adtsHeader.samplingFrequencyIndex);
      metaData.nAudioSampleSize = 16; // 假设16位
      metaData.nAudioChannels = adtsHeader.channelConfiguration;
      metaData.AudioSpecCfg[0] = adtsHeader.AudioSpecificConfig[0];
      metaData.AudioSpecCfg[1] = adtsHeader.AudioSpecificConfig[1];
      metaData.nAudioSpecCfgLen = 2;
  
      // 发送音频元数据
      SendMetadata(&metaData);
      printf("发送音频MetaData完成.\n");
  
      unsigned int tick = 0;
      unsigned int pos = 0;
      while(pos + 7 <= m_nFileBufSize) // 至少需要7字节的ADTS头部
      {
          ADTSHeader currentHeader;
          if(!ParseADTSHeader(&m_pFileBuf[pos], m_nFileBufSize - pos, currentHeader))
          {
              printf("WARNING: 未找到有效的ADTS头部，跳过当前字节.\n");
              pos += 1;
              continue;
          }
  
          // AAC帧数据开始的位置
          int aacFrameStart = pos + 7; // ADTS头部通常为7字节
          if(aacFrameStart + (currentHeader.frameLength - 7) > m_nFileBufSize)
          {
              printf("WARNING: AAC帧数据超出文件大小，停止处理.\n");
              break;
          }
  
          // 提取AAC帧数据
          unsigned char* aacData = &m_pFileBuf[aacFrameStart];
          int aacFrameSize = currentHeader.frameLength - 7;
  
          // 发送AAC帧
          SendAACPacket(aacData, aacFrameSize, tick);
          printf("发送AAC帧: %d 字节, 时间戳: %u ms\n", aacFrameSize, tick);
  
          // 更新时间戳
          tick += 1024 * 1000 / metaData.nAudioSampleRate; // 假设每帧1024个采样点
  
          // 移动到下一个帧
          pos += currentHeader.frameLength;
      }
  
      printf("AAC文件推流完成.\n");
      return TRUE;
  }
  ```

  **代码说明：**

  1. **文件读取和缓冲**：
     - 打开指定的AAC文件，并读取到`m_pFileBuf`缓冲区中。
     - 检查文件大小是否超过缓冲区限制。
  2. **解析第一个ADTS头部**：
     - 使用`ParseADTSHeader`函数解析文件开始的ADTS头部，提取`AudioSpecificConfig`等音频参数。
     - 设置`RTMPMetadata`结构体中的音频相关参数，如采样率、位深、声道数等。
  3. **发送音频元数据**：
     - 调用`SendMetadata`函数发送音频元数据和音频序列头到RTMP服务器。
  4. **循环解析并发送AAC帧**：
     - 使用`while`循环，逐个解析AAC帧的ADTS头部和音频数据。
     - 对于每个AAC帧：
       - 解析ADTS头部，获取帧长度。
       - 提取AAC帧数据（去除ADTS头部）。
       - 调用`SendAACPacket`发送AAC帧数据到RTMP服务器。
       - 更新时间戳`tick`，根据采样率和每帧的采样点数计算。
       - 移动文件读取位置到下一个AAC帧。
  5. **结束推流**：
     - 推流完成后，打印日志信息。

  ------

  ### **5. 断开RTMP连接**

  **协议步骤说明：**

  推流完成后，需要断开与RTMP服务器的连接，释放相关资源。

  **相关代码实现：**

  ```cpp
  void CRTMPStream::Close()
  {
      if(m_pRtmp)
      {
          RTMP_Close(m_pRtmp);
          RTMP_Free(m_pRtmp);
          m_pRtmp = NULL;
      }
  }
  ```

  **代码说明：**

  - **RTMP_Close**：关闭与RTMP服务器的连接。
  - **RTMP_Free**：释放RTMP对象的内存。
  - **设置`m_pRtmp`为`NULL`**：防止重复释放。

  ------

  ### **整体流程结合协议步骤和代码实现**

  以下是AAC推流的完整流程，结合RTMP协议步骤和代码实现：

  1. **初始化和连接RTMP服务器**

     - **协议**：建立RTMP连接，准备推流。

     - 代码：

       ```cpp
       CRTMPStream rtmpStream;
       if(!rtmpStream.Connect("rtmp://localhost/live/livestream"))
       {
           // 连接失败处理
       }
       ```

  2. **发送音视频元数据（onMetaData）**

     - **协议**：发送包含音频参数的元数据，通知接收端音频流的配置。

     - 代码：

       ```cpp
       RTMPMetadata metaData;
       // 填充metaData的音频相关字段
       rtmpStream.SendMetadata(&metaData);
       ```

  3. **发送音频序列头（Audio Sequence Header）**

     - **协议**：发送`AudioSpecificConfig`，用于音频解码配置。

     - 代码：

       - 在`SendMetadata`函数中已实现发送音频序列头的逻辑。

       ```cpp
       // 已在 SendMetadata 函数中
       ```

  4. **解析AAC文件并发送音频数据包**

     - **协议**：

       - 解析每个ADTS帧，去除ADTS头部，提取AAC帧数据。
       - 按照RTMP音频数据包格式发送音频帧。

     - **代码**：

       ```cpp
       rtmpStream.SendAACFile("output.aac");
       ```

       - `SendAACFile`函数内部实现了ADTS头部解析、音频元数据设置、音频序列头发送以及AAC帧数据的循环发送。

  5. **断开RTMP连接**

     - **协议**：完成推流后，断开与RTMP服务器的连接。

     - 代码：

       ```cpp
       rtmpStream.Close();
       ```

  ------

  ### **完整的推流流程示意图**

  ```
  +-------------------+       RTMP协议       +-------------------+
  |   CRTMPStream     | <------------------> |    RTMP服务器     |
  +-------------------+                       +-------------------+
          |                                       |
          | 1. 初始化RTMP连接                       |
          |-------------------------------------->|
          |                                       |
          | 2. 发送onMetaData(音频元数据)            |
          |-------------------------------------->|
          |                                       |
          | 3. 发送音频序列头(Audio Specific Config) |
          |-------------------------------------->|
          |                                       |
          | 4. 发送AAC音频数据包                     |
          |-------------------------------------->|
          |                                       |
          | 5. 断开RTMP连接                         |
          |-------------------------------------->|
          |                                       |
  ```

  ------

  ### **关键代码详解**

  #### **4.3.1 解析ADTS头部并发送AAC帧**

  ```cpp
  while(pos + 7 <= m_nFileBufSize) // 至少需要7字节的ADTS头部
  {
      ADTSHeader currentHeader;
      if(!ParseADTSHeader(&m_pFileBuf[pos], m_nFileBufSize - pos, currentHeader))
      {
          printf("WARNING: 未找到有效的ADTS头部，跳过当前字节.\n");
          pos += 1;
          continue;
      }
  
      // AAC帧数据开始的位置
      int aacFrameStart = pos + 7; // ADTS头部通常为7字节
      if(aacFrameStart + (currentHeader.frameLength - 7) > m_nFileBufSize)
      {
          printf("WARNING: AAC帧数据超出文件大小，停止处理.\n");
          break;
      }
  
      // 提取AAC帧数据
      unsigned char* aacData = &m_pFileBuf[aacFrameStart];
      int aacFrameSize = currentHeader.frameLength - 7;
  
      // 发送AAC帧
      SendAACPacket(aacData, aacFrameSize, tick);
      printf("发送AAC帧: %d 字节, 时间戳: %u ms\n", aacFrameSize, tick);
  
      // 更新时间戳
      tick += 1024 * 1000 / metaData.nAudioSampleRate; // 假设每帧1024个采样点
  
      // 移动到下一个帧
      pos += currentHeader.frameLength;
  }
  ```

  **代码说明：**

  - **循环条件**：确保剩余数据至少包含一个ADTS头部（7字节）。
  - ADTS头部解析：
    - 使用`ParseADTSHeader`解析当前帧的ADTS头部。
    - 如果解析失败，跳过当前字节继续解析，避免死循环。
  - AAC帧数据提取：
    - 计算AAC帧数据在缓冲区中的起始位置和大小。
    - `frameLength`包括ADTS头部和AAC帧数据，因此实际AAC帧数据大小为`frameLength - 7`。
  - 发送AAC帧：
    - 调用`SendAACPacket`发送提取的AAC帧数据，并附带当前时间戳。
  - 时间戳更新：
    - 根据音频采样率和每帧采样点数（通常为1024）计算下一个帧的时间戳。
  - 位置更新：
    - 更新读取位置`pos`到下一个帧的起始位置。

  ------

  ### **5. Demo程序结合推流流程**

  **Demo程序代码：**

  ```cpp
  /********************************************************************
  filename:   Demo_AACStream.cpp
  created:    2025-01-14
  author:     firehood
  purpose:    使用CRTMPStream类仅推流本地AAC文件到RTMP服务器
  *********************************************************************/
  
  #include "RTMPStream.h"
  #include <iostream>
  
  int main()
  {
      // 创建CRTMPStream实例
      CRTMPStream rtmpStream;
  
      // 连接到RTMP服务器
      const char* rtmpUrl = "rtmp://localhost/live/livestream";
      if(!rtmpStream.Connect(rtmpUrl))
      {
          std::cerr << "无法连接到RTMP服务器: " << rtmpUrl << std::endl;
          return -1;
      }
      std::cout << "成功连接到RTMP服务器: " << rtmpUrl << std::endl;
  
      // 发送AAC文件
      const char* aacFilePath = "output.aac";
      if(!rtmpStream.SendAACFile(aacFilePath))
      {
          std::cerr << "推流AAC文件失败: " << aacFilePath << std::endl;
          rtmpStream.Close();
          return -1;
      }
      std::cout << "成功推流AAC文件: " << aacFilePath << std::endl;
  
      // 断开连接
      rtmpStream.Close();
      std::cout << "断开RTMP连接." << std::endl;
  
      return 0;
  }
  ```

  **流程结合说明：**

  1. **创建`CRTMPStream`实例**：

     ```cpp
     CRTMPStream rtmpStream;
     ```

     - 初始化RTMP相关资源，分配缓冲区，初始化套接字等。

  2. **连接到RTMP服务器**：

     ```cpp
     if(!rtmpStream.Connect(rtmpUrl))
     {
         // 处理连接失败
     }
     ```

     - 调用`Connect`函数，建立与RTMP服务器的连接。

  3. **发送AAC文件**：

     ```cpp
     if(!rtmpStream.SendAACFile(aacFilePath))
     {
         // 处理发送失败
     }
     ```

     - 调用`SendAACFile`函数，读取AAC文件，解析ADTS头部，发送音频元数据和AAC帧数据。

  4. **断开连接**：

     ```cpp
     rtmpStream.Close();
     ```

     - 调用`Close`函数，断开与RTMP服务器的连接，释放资源。

  ------

  ### **总结**

  通过以上步骤，我们详细总结了使用`CRTMPStream`类进行AAC推流的整个流程，主要基于RTMP协议的音频推流步骤。每个协议步骤都对应了代码中的具体实现，确保推流过程符合RTMP协议规范。

  **关键点回顾：**

  1. **初始化和连接**：建立与RTMP服务器的连接，确保可以发送数据。
  2. **发送元数据**：通过`onMetaData`通知接收端音频的基本配置，包括采样率、声道数等。
  3. **发送音频序列头**：传递`AudioSpecificConfig`，帮助接收端正确解码音频。
  4. **发送音频数据包**：解析AAC文件中的ADTS帧，去除ADTS头部，按帧发送音频数据。
  5. **断开连接**：完成推流后，及时断开连接，释放资源。

  **注意事项：**

  - **AAC文件格式**：确保AAC文件采用ADTS格式，且ADTS头部正确。
  - **音频参数匹配**：`AudioSpecificConfig`的配置必须与AAC文件的实际编码参数一致。
  - **时间戳计算**：根据音频采样率和每帧采样点数正确计算时间戳，以保证音频播放的同步性。
  - **错误处理**：实际应用中应增加更完善的错误处理机制，确保推流过程的稳定性。
  - **性能优化**：对于大文件或实时音频，考虑使用多线程或更高效的缓冲机制以提升推流性能。

  通过严格遵循RTMP协议的音频推流步骤，并结合正确的代码实现，可以实现稳定且兼容性强的AAC音频推流功能。


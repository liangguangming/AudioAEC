# 音频AEC（回声消除）项目

基于macOS的音频回声消除（Acoustic Echo Cancellation）项目，使用VoiceProcessingIO音频单元实现高质量的音频录制和回声消除功能。

## VoiceProcessingIO的局限
只要你用VoiceProcessingIO做AEC，系统就会自动“保护”麦克风输入，抑制扬声器音量，防止回声。
这是Apple的安全策略，防止回声泄露到麦克风，无法通过AudioUnit参数关闭。

## 项目概述

本项目实现了在macOS系统上使用VoiceProcessingIO音频单元进行音频录制，具备以下特性：
- 实时音频采集
- 自动回声消除（AEC）
- 自动增益控制（AGC）
- 高质量音频输出（16位PCM WAV格式）

## 核心架构

### 文件结构
```
audioAEC/
├── src/
│   ├── AudioAECImpl.h      # 音频AEC实现头文件
│   ├── AudioAECImpl.mm     # 音频AEC核心实现（Objective-C++）
│   ├── AudioAECWrapper.h   # C++包装器头文件
│   ├── AudioAECWrapper.cpp # C++包装器实现
│   └── main.cpp           # 主程序入口
├── CMakeLists.txt         # CMake构建配置
└── README.md             # 项目文档
```

## 核心技术步骤

### 1. 音频单元初始化

#### 1.1 创建VoiceProcessingIO音频单元
```cpp
AudioComponentDescription desc = {
    kAudioUnitType_Output,
    kAudioUnitSubType_VoiceProcessingIO,  // 关键：使用VoiceProcessingIO
    kAudioUnitManufacturer_Apple,
    0, 0
};
```

**技术要点：**
- VoiceProcessingIO是macOS专用于语音处理的音频单元
- 内置AEC、AGC、降噪等功能
- 相比RemoteIO，更适合语音通话场景

#### 1.2 配置音频格式
```cpp
AudioStreamBasicDescription format = { 0 };
format.mSampleRate = 48000;                    // 48kHz采样率
format.mFormatID = kAudioFormatLinearPCM;      // PCM格式
format.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
format.mChannelsPerFrame = 1;                  // 单声道
format.mBitsPerChannel = 32;                   // 32位浮点
```

**技术要点：**
- 使用32位浮点格式保证音频质量
- 单声道减少计算复杂度
- 48kHz采样率提供良好的音频质量

### 2. 回调机制设置

#### 2.1 输入回调（关键步骤）
```cpp
// 设置输入回调获取麦克风数据
AURenderCallbackStruct inputCallbackStruct = { 0 };
inputCallbackStruct.inputProc = InputRenderCallback;
inputCallbackStruct.inputProcRefCon = this;

AudioUnitSetProperty(audioUnit_,
                     kAudioOutputUnitProperty_SetInputCallback,  // 关键属性
                     kAudioUnitScope_Global,
                     0,
                     &inputCallbackStruct,
                     sizeof(inputCallbackStruct));
```

**技术要点：**
- VoiceProcessingIO需要**输入回调**来获取麦克风数据
- 不能只依赖渲染回调
- 这是解决音频数据为零的关键

#### 2.2 输出渲染回调
```cpp
// 设置输出渲染回调提供静音输出
AURenderCallbackStruct outputCallbackStruct = { 0 };
outputCallbackStruct.inputProc = RenderCallback;
outputCallbackStruct.inputProcRefCon = this;

AudioUnitSetProperty(audioUnit_,
                     kAudioUnitProperty_SetRenderCallback,
                     kAudioUnitScope_Input,
                     0,
                     &outputCallbackStruct,
                     sizeof(outputCallbackStruct));
```

**技术要点：**
- 提供静音输出避免扬声器播放
- 确保AEC功能正常工作

### 3. 音频数据获取

#### 3.1 输入回调实现
```cpp
OSStatus AudioAECImpl::InputRenderCallback(...) {
    // 动态分配缓冲区
    std::vector<float> data(inNumberFrames);
    
    AudioBufferList bufferList;
    bufferList.mNumberBuffers = 1;
    bufferList.mBuffers[0].mData = data.data();
    bufferList.mBuffers[0].mDataByteSize = sizeof(float) * inNumberFrames;
    bufferList.mBuffers[0].mNumberChannels = 1;

    // 获取输入数据（经过AEC处理）
    OSStatus status = AudioUnitRender(self->audioUnit_,
                                      ioActionFlags,
                                      inTimeStamp,
                                      1,  // input bus
                                      inNumberFrames,
                                      &bufferList);
    
    // 调用用户回调函数
    if (status == noErr && self->callback_) {
        self->callback_(data.data(), inNumberFrames);
    }
}
```

**技术要点：**
- 使用`AudioUnitRender`从音频单元获取数据
- 数据已经过AEC处理，回声被消除
- 实时回调确保低延迟

### 4. 音频格式转换与WAV写入

#### 4.1 Float到Int16转换
```cpp
// 将float数据转换为16位整型PCM
std::vector<int16_t> pcmBuffer(audioBuffer.size());
for (size_t i = 0; i < audioBuffer.size(); ++i) {
    // 限制在[-1.0, 1.0]范围内，然后缩放到16位整型范围
    float sample = std::max(-1.0f, std::min(1.0f, audioBuffer[i]));
    pcmBuffer[i] = static_cast<int16_t>(sample * 32767.0f);
}
```

**技术要点：**
- 32位浮点转换为16位整型提高兼容性
- 数据范围限制防止溢出
- 32767.0f是16位有符号整型的最大值

#### 4.2 WAV文件头配置
```cpp
struct WAVHeader {
    // ... 其他字段
    uint16_t audioFormat = 1;      // PCM格式
    uint16_t numChannels = 1;      // 单声道
    uint32_t sampleRate = 48000;   // 采样率
    uint16_t bitsPerSample = 16;   // 16位整型
    // ...
};
```

**技术要点：**
- 使用16位PCM格式确保最大兼容性
- 文件头信息必须与实际数据一致

## 关键技术难点与解决方案

### 1. 音频数据为零问题
**问题：** 回调触发但音频数据全为零
**原因：** VoiceProcessingIO的回调机制配置错误
**解决：** 使用输入回调（`kAudioOutputUnitProperty_SetInputCallback`）而不是仅依赖渲染回调

### 2. 音质问题
**问题：** 32位浮点WAV格式兼容性差
**原因：** 大部分播放器对32位float WAV支持不好
**解决：** 转换为16位整型PCM格式

### 3. AEC功能配置
**问题：** 回声消除效果不明显
**原因：** VoiceProcessingIO属性配置不完整
**解决：** 启用AGC、设置合适的缓冲区大小

## 构建与运行

### 环境要求
- macOS 10.15+
- Xcode Command Line Tools
- CMake 3.10+

### 构建步骤
```bash
mkdir build
cd build
cmake ..
cmake --build .
```

### 运行
```bash
./bin/audioAEC
```

## 性能优化建议

1. **缓冲区大小调优**
   - 默认512帧，可根据延迟要求调整
   - 较小缓冲区：低延迟，较高CPU占用
   - 较大缓冲区：高延迟，较低CPU占用

2. **采样率选择**
   - 48kHz：高质量，适合音乐录制
   - 44.1kHz：标准CD质量，兼容性好
   - 16kHz：语音质量，节省资源

3. **AGC控制**
   - 启用AGC：自动音量调节，适合语音
   - 禁用AGC：保持原始音量，适合音乐

## 扩展功能

1. **多声道支持**：修改`mChannelsPerFrame`和回调处理
2. **实时音频处理**：在回调中添加音频效果
3. **网络传输**：集成WebRTC或其他网络协议
4. **音频可视化**：添加频谱分析功能

## 故障排除

### 常见问题
1. **权限问题**：确保麦克风权限已授予
2. **设备占用**：检查其他应用是否正在使用麦克风
3. **音频格式不匹配**：确认WAV头部与实际数据一致
4. **回调未触发**：检查音频单元配置和回调设置

### 调试技巧
1. 启用详细日志输出
2. 检查音频数据统计信息
3. 使用音频分析工具验证WAV文件
4. 监控系统音频设备状态

## 技术参考

- [Core Audio Programming Guide](https://developer.apple.com/library/archive/documentation/MusicAudio/Conceptual/CoreAudioOverview/Introduction/Introduction.html)
- [Audio Unit Programming Guide](https://developer.apple.com/library/archive/documentation/MusicAudio/Conceptual/AudioUnitProgrammingGuide/Introduction/Introduction.html)
- [VoiceProcessingIO Documentation](https://developer.apple.com/documentation/audiotoolbox/kaudiounitsubtype_voiceprocessingio)

## 许可证

本项目采用MIT许可证，详见LICENSE文件。 
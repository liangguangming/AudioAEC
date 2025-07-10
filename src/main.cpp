#include "AudioAECWrapper.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#include <thread>
#include <iomanip> // Required for std::fixed and std::setprecision
#include <atomic>
#include <cmath>

// WAV文件头结构
struct WAVHeader {
    char riff[4] = {'R', 'I', 'F', 'F'};
    uint32_t chunkSize;
    char wave[4] = {'W', 'A', 'V', 'E'};
    char fmt[4] = {'f', 'm', 't', ' '};
    uint32_t fmtChunkSize = 16;
    uint16_t audioFormat = 1; // PCM
    uint16_t numChannels = 1; // 单声道
    uint32_t sampleRate = 48000;
    uint32_t byteRate = 48000 * 2; // sampleRate * numChannels * bitsPerSample/8 (16位)
    uint16_t blockAlign = 2; // numChannels * bitsPerSample/8 (16位)
    uint16_t bitsPerSample = 16; // 改为16位整型
    char data[4] = {'d', 'a', 't', 'a'};
    uint32_t dataChunkSize;
};

// 麦克风测试函数
bool testMicrophone(AudioAECWrapper& aec, int testDuration = 3) {
    std::cout << "开始麦克风测试，持续" << testDuration << "秒..." << std::endl;
    std::cout << "请对着麦克风说话或发出声音..." << std::endl;
    
    std::vector<float> testBuffer;
    std::atomic<bool> testing{true};
    std::atomic<int> testCallbackCount{0};
    std::atomic<float> testMaxAmplitude{0.0f};
    
    auto testStartTime = std::chrono::steady_clock::now();
    
    bool startSuccess = aec.start([&](const float* data, size_t frames) {
        testCallbackCount.fetch_add(1);
        
        if (testing.load()) {
            // 计算最大振幅
            float localMax = 0.0f;
            for (size_t i = 0; i < frames; i++) {
                localMax = std::max(localMax, std::abs(data[i]));
            }
            
            float currentMax = testMaxAmplitude.load();
            while (localMax > currentMax && !testMaxAmplitude.compare_exchange_weak(currentMax, localMax));
            
            testBuffer.insert(testBuffer.end(), data, data + frames);
            
            // 显示测试进度
            auto elapsed = std::chrono::steady_clock::now() - testStartTime;
            auto elapsedSeconds = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() / 1000.0f;
            float progress = (elapsedSeconds / testDuration) * 100.0f;
            
            if (progress <= 100.0f) {
                std::cout << "\r测试进度: " << std::fixed << std::setprecision(1) 
                         << progress << "% 最大振幅: " << std::setprecision(4) << testMaxAmplitude.load() << std::flush;
            }
        }
    });
    
    if (!startSuccess) {
        std::cerr << "麦克风测试启动失败！" << std::endl;
        return false;
    }
    
    // 等待测试完成
    auto testTargetTime = testStartTime + std::chrono::seconds(testDuration);
    while (std::chrono::steady_clock::now() < testTargetTime) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    testing.store(false);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    aec.stop();
    
    std::cout << "\n麦克风测试完成！" << std::endl;
    std::cout << "- 回调次数: " << testCallbackCount.load() << std::endl;
    std::cout << "- 最大振幅: " << testMaxAmplitude.load() << std::endl;
    std::cout << "- 采样点数: " << testBuffer.size() << std::endl;
    
    // 检查是否有音频信号
    bool hasAudioSignal = testMaxAmplitude.load() > 0.001f; // 阈值可调整
    
    if (hasAudioSignal) {
        std::cout << "✓ 麦克风工作正常，检测到音频信号" << std::endl;
        return true;
    } else {
        std::cout << "✗ 麦克风测试失败，未检测到音频信号" << std::endl;
        std::cout << "请检查：" << std::endl;
        std::cout << "1. 麦克风权限是否已授予" << std::endl;
        std::cout << "2. 麦克风是否正常工作" << std::endl;
        std::cout << "3. 是否有其他应用正在使用麦克风" << std::endl;
        std::cout << "4. 系统音量设置是否合适" << std::endl;
        return false;
    }
}

int main() {
    AudioAECWrapper aec;
    std::vector<float> audioBuffer;
    const int sampleRate = 48000;
    const int recordDuration = 10; // 录制10秒
    
    std::cout << "=== 音频AEC录制程序 ===" << std::endl;
    std::cout << "使用VoiceProcessingIO进行回声消除" << std::endl;
    std::cout << "采样率: " << sampleRate << " Hz" << std::endl;
    
    std::cout << "\n开始录制音频，持续" << recordDuration << "秒..." << std::endl;
    std::cout << "AEC功能已启用，将自动消除回声" << std::endl;
    
    auto startTime = std::chrono::steady_clock::now();
    std::atomic<bool> recording{true};
    std::atomic<int> callbackCount{0};
    std::atomic<size_t> totalFrames{0};
    std::atomic<float> maxAmplitude{0.0f};
    std::atomic<float> rmsLevel{0.0f};

    bool startSuccess = aec.start([&](const float* data, size_t frames) {
        callbackCount.fetch_add(1);
        totalFrames.fetch_add(frames);
        
        if (recording.load()) {
            // 计算音频统计信息
            float localMax = 0.0f;
            float localRms = 0.0f;
            
            for (size_t i = 0; i < frames; i++) {
                float sample = data[i];
                localMax = std::max(localMax, std::abs(sample));
                localRms += sample * sample;
            }
            localRms = std::sqrt(localRms / frames);
            
            // 更新全局统计
            float currentMax = maxAmplitude.load();
            while (localMax > currentMax && !maxAmplitude.compare_exchange_weak(currentMax, localMax));
            
            float currentRms = rmsLevel.load();
            while (localRms > currentRms && !rmsLevel.compare_exchange_weak(currentRms, localRms));
            
            // 将音频数据添加到缓冲区
            audioBuffer.insert(audioBuffer.end(), data, data + frames);
            
            // 显示录制进度
            auto elapsed = std::chrono::steady_clock::now() - startTime;
            auto elapsedSeconds = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() / 1000.0f;
            float progress = (elapsedSeconds / recordDuration) * 100.0f;
            
            if (progress <= 100.0f) {
                std::cout << "\r录制进度: " << std::fixed << std::setprecision(1) 
                         << progress << "% (" << std::setprecision(1) << elapsedSeconds << "s/" << recordDuration << "s) "
                         << "回调次数: " << callbackCount.load() << " 总帧数: " << totalFrames.load() 
                         << " 最大振幅: " << std::setprecision(4) << maxAmplitude.load()
                         << " RMS: " << std::setprecision(4) << rmsLevel.load() << std::flush;
            }
            
            // 每100次回调显示一次详细数据
            if (callbackCount.load() % 100 == 0) {
                std::cout << "\n[AEC调试] 回调#" << callbackCount.load() 
                         << " 帧数:" << frames 
                         << " 前5个样本:" << data[0] << "," << data[1] << "," << data[2] << "," << data[3] << "," << data[4] << std::endl;
            }
        }
    });

    if (!startSuccess) {
        std::cerr << "启动音频录制失败！请检查麦克风权限和设备状态。" << std::endl;
        return -1;
    }

    std::cout << "音频录制已启动，等待回调..." << std::endl;

    // 等待录制完成（基于时间）
    auto targetTime = startTime + std::chrono::seconds(recordDuration);
    while (std::chrono::steady_clock::now() < targetTime) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // 停止录制
    recording.store(false);
    std::cout << "\n录制完成！" << std::endl;

    // 等待一小段时间确保最后的回调完成
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    aec.stop();

    std::cout << "最终统计:" << std::endl;
    std::cout << "- 回调次数: " << callbackCount.load() << std::endl;
    std::cout << "- 总帧数: " << totalFrames.load() << std::endl;
    std::cout << "- 缓冲区大小: " << audioBuffer.size() << " 采样点" << std::endl;
    std::cout << "- 最大振幅: " << maxAmplitude.load() << std::endl;
    std::cout << "- RMS电平: " << rmsLevel.load() << std::endl;

    if (audioBuffer.empty()) {
        std::cerr << "错误：没有收集到任何音频数据！" << std::endl;
        std::cerr << "可能的原因：" << std::endl;
        std::cerr << "1. 麦克风权限被拒绝" << std::endl;
        std::cerr << "2. 没有可用的音频输入设备" << std::endl;
        std::cerr << "3. 音频设备配置问题" << std::endl;
        return -1;
    }

    // 检查音频数据是否全为零
    bool allZero = true;
    float maxVal = 0.0f;
    for (float sample : audioBuffer) {
        if (std::abs(sample) > 1e-6) {
            allZero = false;
        }
        maxVal = std::max(maxVal, std::abs(sample));
    }
    
    if (allZero) {
        std::cerr << "警告：所有音频数据都是零！" << std::endl;
        std::cerr << "可能的原因：" << std::endl;
        std::cerr << "1. 麦克风没有检测到声音" << std::endl;
        std::cerr << "2. 音频设备配置错误" << std::endl;
        std::cerr << "3. 音量设置过低" << std::endl;
    } else {
        std::cout << "音频数据有效，最大绝对值: " << maxVal << std::endl;
    }

    // 将float数据转换为16位整型PCM
    std::vector<int16_t> pcmBuffer(audioBuffer.size());
    for (size_t i = 0; i < audioBuffer.size(); ++i) {
        // 限制在[-1.0, 1.0]范围内，然后缩放到16位整型范围
        float sample = std::max(-1.0f, std::min(1.0f, audioBuffer[i]));
        pcmBuffer[i] = static_cast<int16_t>(sample * 32767.0f);
    }

    // 写入WAV文件
    std::string filename = "recorded_audio_aec.wav";
    std::ofstream file(filename, std::ios::binary);
    
    if (!file.is_open()) {
        std::cerr << "无法创建音频文件: " << filename << std::endl;
        return -1;
    }

    // 准备WAV文件头
    WAVHeader header;
    header.dataChunkSize = pcmBuffer.size() * sizeof(int16_t);
    header.chunkSize = 36 + header.dataChunkSize;

    // 写入文件头
    file.write(reinterpret_cast<const char*>(&header), sizeof(WAVHeader));

    // 写入音频数据（16位整型PCM）
    file.write(reinterpret_cast<const char*>(pcmBuffer.data()), 
               pcmBuffer.size() * sizeof(int16_t));

    file.close();

    std::cout << "AEC音频已保存到: " << filename << std::endl;
    std::cout << "录制时长: " << (float)audioBuffer.size() / sampleRate << " 秒" << std::endl;
    std::cout << "采样点数: " << audioBuffer.size() << std::endl;
    std::cout << "文件大小: " << (36 + pcmBuffer.size() * sizeof(int16_t)) << " 字节" << std::endl;
    std::cout << "格式: 16位整型PCM (兼容性更好)" << std::endl;

    return 0;
}

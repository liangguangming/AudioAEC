#import "AudioAECImpl.h"
#include <iostream>
#include <vector>

AudioAECImpl::AudioAECImpl() : audioUnit_(nullptr) {}

AudioAECImpl::~AudioAECImpl() {
    stop();
}

bool AudioAECImpl::start(AudioCallback callback) {
    callback_ = callback;

    AudioComponentDescription desc = {
        kAudioUnitType_Output,
        kAudioUnitSubType_VoiceProcessingIO,  // 使用VoiceProcessingIO用于AEC
        kAudioUnitManufacturer_Apple,
        0,
        0
    };

    AudioComponent comp = AudioComponentFindNext(nullptr, &desc);
    if (!comp) {
        std::cerr << "无法找到VoiceProcessingIO音频组件" << std::endl;
        return false;
    }

    OSStatus status = AudioComponentInstanceNew(comp, &audioUnit_);
    if (status != noErr) {
        std::cerr << "创建音频单元失败，错误码: " << status << std::endl;
        return false;
    }

    // 设置音频格式 - 使用标准配置
    AudioStreamBasicDescription format = { 0 };
    format.mSampleRate = 48000;
    format.mFormatID = kAudioFormatLinearPCM;
    format.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
    format.mChannelsPerFrame = 1;
    format.mFramesPerPacket = 1;
    format.mBytesPerFrame = 4;
    format.mBytesPerPacket = 4;
    format.mBitsPerChannel = 32;

    std::cout << "音频格式配置:" << std::endl;
    std::cout << "- 采样率: " << format.mSampleRate << " Hz" << std::endl;
    std::cout << "- 声道数: " << format.mChannelsPerFrame << std::endl;
    std::cout << "- 位深度: " << format.mBitsPerChannel << " bits" << std::endl;

    // 设置输入格式
    status = AudioUnitSetProperty(audioUnit_,
                                 kAudioUnitProperty_StreamFormat,
                                 kAudioUnitScope_Output,
                                 1,  // input bus
                                 &format,
                                 sizeof(format));
    if (status != noErr) {
        std::cerr << "设置输入格式失败，错误码: " << status << std::endl;
        return false;
    }

    // 设置输出格式
    status = AudioUnitSetProperty(audioUnit_,
                                 kAudioUnitProperty_StreamFormat,
                                 kAudioUnitScope_Input,
                                 0,  // output bus
                                 &format,
                                 sizeof(format));
    if (status != noErr) {
        std::cerr << "设置输出格式失败，错误码: " << status << std::endl;
        return false;
    }

    // 启用输入
    UInt32 enableIO = 1;
    status = AudioUnitSetProperty(audioUnit_,
                                 kAudioOutputUnitProperty_EnableIO,
                                 kAudioUnitScope_Input,
                                 1, // enable input
                                 &enableIO,
                                 sizeof(enableIO));
    if (status != noErr) {
        std::cerr << "启用输入失败，错误码: " << status << std::endl;
        return false;
    }

    // 启用输出（VoiceProcessingIO需要输出才能正常工作）
    status = AudioUnitSetProperty(audioUnit_,
                                 kAudioOutputUnitProperty_EnableIO,
                                 kAudioUnitScope_Output,
                                 0, // enable output
                                 &enableIO,
                                 sizeof(enableIO));
    if (status != noErr) {
        std::cerr << "启用输出失败，错误码: " << status << std::endl;
        return false;
    }

    // 设置渲染回调（这是关键）
    AURenderCallbackStruct callbackStruct = { 0 };
    callbackStruct.inputProc = RenderCallback;
    callbackStruct.inputProcRefCon = this;

    status = AudioUnitSetProperty(audioUnit_,
                                 kAudioUnitProperty_SetRenderCallback,
                                 kAudioUnitScope_Input,
                                 0,
                                 &callbackStruct,
                                 sizeof(callbackStruct));
    if (status != noErr) {
        std::cerr << "设置渲染回调失败，错误码: " << status << std::endl;
        return false;
    }

    // 初始化音频单元
    status = AudioUnitInitialize(audioUnit_);
    if (status != noErr) {
        std::cerr << "初始化音频单元失败，错误码: " << status << std::endl;
        return false;
    }

    // 启动音频单元
    status = AudioOutputUnitStart(audioUnit_);
    if (status != noErr) {
        std::cerr << "启动音频单元失败，错误码: " << status << std::endl;
        return false;
    }

    std::cout << "VoiceProcessingIO音频单元启动成功，AEC功能已启用" << std::endl;
    return true;
}

void AudioAECImpl::stop() {
    if (audioUnit_) {
        AudioOutputUnitStop(audioUnit_);
        AudioUnitUninitialize(audioUnit_);
        AudioComponentInstanceDispose(audioUnit_);
        audioUnit_ = nullptr;
        std::cout << "音频单元已停止" << std::endl;
    }
}

// 渲染回调函数 - 这是唯一需要的回调
OSStatus AudioAECImpl::RenderCallback(void* inRefCon,
                                      AudioUnitRenderActionFlags* ioActionFlags,
                                      const AudioTimeStamp* inTimeStamp,
                                      UInt32 inBusNumber,
                                      UInt32 inNumberFrames,
                                      AudioBufferList* ioData) {
    auto* self = static_cast<AudioAECImpl*>(inRefCon);

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

    if (status == noErr && self->callback_) {
        // 简单的音频处理：限制音量范围
        // for (UInt32 i = 0; i < inNumberFrames; i++) {
        //     // 限制音频范围在 -1.0 到 1.0 之间
        //     if (data[i] > 1.0f) data[i] = 1.0f;
        //     if (data[i] < -1.0f) data[i] = -1.0f;
        // }
        
        self->callback_(data.data(), inNumberFrames);
    } else if (status != noErr) {
        std::cerr << "AudioUnitRender失败，错误码: " << status << std::endl;
    }

    // 为输出提供静音数据
    if (ioData && ioData->mNumberBuffers > 0) {
        for (UInt32 i = 0; i < ioData->mNumberBuffers; i++) {
            memset(ioData->mBuffers[i].mData, 0, ioData->mBuffers[i].mDataByteSize);
        }
    }

    return noErr;
}

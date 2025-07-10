#pragma once

#import <AudioToolbox/AudioToolbox.h>
#include <functional>

class AudioAECImpl {
public:
    using AudioCallback = std::function<void(const float* data, size_t numFrames)>;

    AudioAECImpl();
    ~AudioAECImpl();

    bool start(AudioCallback callback);
    void stop();

private:
    AudioUnit audioUnit_;
    AudioCallback callback_;

    static OSStatus InputRenderCallback(void* inRefCon,
                                        AudioUnitRenderActionFlags* ioActionFlags,
                                        const AudioTimeStamp* inTimeStamp,
                                        UInt32 inBusNumber,
                                        UInt32 inNumberFrames,
                                        AudioBufferList* ioData);

    static OSStatus RenderCallback(void* inRefCon,
                                   AudioUnitRenderActionFlags* ioActionFlags,
                                   const AudioTimeStamp* inTimeStamp,
                                   UInt32 inBusNumber,
                                   UInt32 inNumberFrames,
                                   AudioBufferList* ioData);
};

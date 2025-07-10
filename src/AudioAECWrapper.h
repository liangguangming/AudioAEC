#pragma once

#include <cstdint>
#include <functional>

class AudioAECWrapper {
public:
    using AudioCallback = std::function<void(const float* data, size_t numFrames)>;

    AudioAECWrapper();
    ~AudioAECWrapper();

    bool start(AudioCallback callback);
    void stop();

private:
    class Impl;
    Impl* impl_;
};

#include "AudioAECWrapper.h"
#include "AudioAECImpl.h"

class AudioAECWrapper::Impl {
public:
    AudioAECImpl impl;
};

AudioAECWrapper::AudioAECWrapper() {
    impl_ = new Impl();
}

AudioAECWrapper::~AudioAECWrapper() {
    delete impl_;
}

bool AudioAECWrapper::start(AudioCallback callback) {
    return impl_->impl.start(callback);
}

void AudioAECWrapper::stop() {
    impl_->impl.stop();
}

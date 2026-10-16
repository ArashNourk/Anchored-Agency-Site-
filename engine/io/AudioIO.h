// Audio I/O abstraction. The engine never talks to an audio API directly; a
// host (standalone device backend, AU/VST3/CLAP wrapper, offline renderer)
// owns the callback and drives Synth::render plus MidiController::message
// with sample-accurate event splitting.
#pragma once

#include <functional>

namespace p10 {

struct AudioCallbackInfo {
    double sampleRate;
    int blockSize;
};

class AudioDevice {
public:
    using Callback = std::function<void(float* left, float* right, int frames)>;
    virtual ~AudioDevice() = default;
    virtual bool open(const AudioCallbackInfo& info, Callback cb) = 0;
    virtual void close() = 0;
};

}  // namespace p10

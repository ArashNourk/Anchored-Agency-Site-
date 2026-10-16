#include "Synth.h"

#include <algorithm>
#include <cmath>

namespace p10 {

Synth::Synth(double sampleRate, int oversample, uint64_t serial)
    : fs_(sampleRate), os_(std::clamp(oversample, 1, 16)), vintage_(&cal_) {
    vintage_.build(serial);
    for (auto& f : frames_) f.resize(kBlock);
    noise_.resize(kBlock * os_);
    acc_.resize(kBlock * os_);
    block_.resize(kBlock);
    calibrationChanged();
    setProgram(Program::init());
    for (auto& m : mod_) m.snapToTarget();
}

void Synth::calibrationChanged() {
    for (int i = 0; i < kNumVoices; ++i) voices_[i].configure(i, fs_, os_, &cal_);
    alloc_.configure(&voices_, &cal_);
    for (auto& m : mod_) m.configure(fs_, &cal_);
    dec_.configure(os_, fs_);
    outDc_.setCutoff(cal_.outputDcHz, fs_);
    outDc_.reset();
    applyProgramToEngine();
}

void Synth::setProgram(const Program& p) {
    alloc_.allNotesOff();
    program_ = p;
    applyProgramToEngine();
}

void Synth::applyProgramToEngine() {
    const auto& s = program_.settings;
    const int mode = static_cast<int>(s[kLayerMode] + 0.5);
    alloc_.setLayerMode(mode, static_cast<int>(s[kSplitPoint] + 0.5), s[kSplitUpperB] > 0.5);
    const double bal = s[kLayerABalance];
    const double gA = mode == 0 ? 1.0 : std::min(1.0, 2.0 * (1.0 - bal));
    const double gB = mode == 0 ? 0.0 : std::min(1.0, 2.0 * bal);
    mod_[0].setTarget(program_.layer[0], gA);
    mod_[1].setTarget(program_.layer[1], gB);
    alloc_.setLayerConfig(0, program_.layer[0]);
    alloc_.setLayerConfig(1, program_.layer[1]);
}

void Synth::setLayerParam(int layer, int id, double value) {
    if (layer < 0 || layer > 1 || id < 0 || id >= kNumLayerParams) return;
    const auto& info = layerParamInfo(id);
    value = clamp(value, info.minValue, info.maxValue);
    if (info.kind != ParamKind::Continuous) value = std::round(value);
    program_.layer[layer][id] = value;
    applyProgramToEngine();
}

void Synth::setProgramParam(int id, double value) {
    if (id < 0 || id >= kNumProgramParams) return;
    const auto& info = programParamInfo(id);
    value = clamp(value, info.minValue, info.maxValue);
    if (info.kind != ParamKind::Continuous) value = std::round(value);
    program_.settings[id] = value;
    applyProgramToEngine();
}

void Synth::noteOn(int note, double velocity01) {
    if (velocity01 <= 0.0) {
        noteOff(note);
        return;
    }
    alloc_.noteOn(note, velocity01);
}

void Synth::noteOff(int note) { alloc_.noteOff(note); }

void Synth::setSustain(bool on) {
    perf_.sustain = on;
    alloc_.setSustain(on);
}

void Synth::allSoundOff() {
    alloc_.allNotesOff();
    for (auto& v : voices_) v.forceSilence();
}

double Synth::latencySamples() const {
    // BLEP linear-phase delay (oversampled) + decimator group delay.
    return static_cast<double>(BlepTables::forOversample(os_, fs_).halfWidth()) / os_ + dec_.latencyOutputSamples();
}

void Synth::renderBlock(double* out, int n) {
    const int mode = static_cast<int>(program_.settings[kLayerMode] + 0.5);
    // Temporal drift and analog state at control-block rate.
    vintage_.stepDrift(static_cast<double>(n) / fs_);
    for (auto& v : voices_) v.setAnalogState(vintage_.state(v.index(), program_.layer[v.layer()][kVintage]));

    mod_[0].process(n, perf_, frames_[0].data());
    if (mode != 0) mod_[1].process(n, perf_, frames_[1].data());

    // One white-noise generator shared by all voices.
    const int m = n * os_;
    for (int i = 0; i < m; ++i) noise_[i] = noiseRng_.bipolar() * 1.7320508075688772;
    std::fill(acc_.begin(), acc_.begin() + m, 0.0);
    for (auto& v : voices_) {
        const int layer = (mode == 0) ? 0 : v.layer();
        v.render(frames_[layer].data(), n, noise_.data(), acc_.data());
    }
    dec_.process(acc_.data(), out, n);
    for (int i = 0; i < n; ++i) out[i] = outDc_.process(out[i]) * cal_.outputGain;
}

void Synth::render(double* mono, int n) {
    int done = 0;
    while (done < n) {
        const int chunk = std::min(kBlock, n - done);
        renderBlock(mono + done, chunk);
        done += chunk;
    }
}

void Synth::render(float* left, float* right, int n) {
    int done = 0;
    while (done < n) {
        const int chunk = std::min(kBlock, n - done);
        renderBlock(block_.data(), chunk);
        for (int i = 0; i < chunk; ++i) {
            const float s = static_cast<float>(block_[i]);
            left[done + i] = s;
            if (right) right[done + i] = s;
        }
        done += chunk;
    }
}

}  // namespace p10

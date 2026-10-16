// Top-level instrument: ten persistent voices, two layers (programs A/B for
// split/stack), the vintage/calibration engine, the allocator and the
// oversampled render path. UI- and host-independent: a standalone app or a
// plugin wrapper only needs this class, MidiController and PresetStore.
#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "dsp/Decimator.h"
#include "model/Calibration.h"
#include "model/Params.h"
#include "model/Vintage.h"
#include "voice/LayerModulator.h"
#include "voice/Voice.h"
#include "voice/VoiceAllocator.h"

namespace p10 {

class Synth {
public:
    // oversample: 2, 4 (default) or 8. serial: seeds the voices' component
    // personalities (two instances with the same serial are the same
    // "physical" instrument).
    explicit Synth(double sampleRate = 48000.0, int oversample = 4, uint64_t serial = 1);

    // Calibration constants. Call calibrationChanged() after editing.
    CalibrationModel& calibration() { return cal_; }
    void calibrationChanged();

    // Program / parameters.
    void setProgram(const Program& p);
    const Program& program() const { return program_; }
    void setLayerParam(int layer, int id, double value);
    void setProgramParam(int id, double value);
    double layerParam(int layer, int id) const { return program_.layer[layer][id]; }

    // Performance.
    void noteOn(int note, double velocity01);
    void noteOff(int note);
    void setModWheel(double v) { perf_.modWheel = v; }
    void setPitchBend(double v) { perf_.pitchBend = v; }
    void setAftertouch(double v) { perf_.aftertouch = v; }
    void setSustain(bool on);
    void allNotesOff() { alloc_.allNotesOff(); }
    void allSoundOff();

    // Analog maintenance.
    void tune() { vintage_.tune(); }
    void age(double hours) { vintage_.age(hours); }

    // Audio.
    void render(float* left, float* right, int n);
    void render(double* mono, int n);

    double sampleRate() const { return fs_; }
    int oversample() const { return os_; }
    double latencySamples() const;

    // Introspection (tests, metering, future UI).
    const Voice& voice(int i) const { return voices_[i]; }
    const VoiceAllocator& allocator() const { return alloc_; }
    VintageEngine& vintage() { return vintage_; }
    const LayerFrame& lastFrame(int layer) const { return frames_[layer].back(); }

private:
    void applyProgramToEngine();
    void renderBlock(double* out, int n);

    static constexpr int kBlock = 64;
    double fs_;
    int os_;
    CalibrationModel cal_;
    VintageEngine vintage_;
    Program program_;
    std::array<Voice, kNumVoices> voices_;
    VoiceAllocator alloc_;
    LayerModulator mod_[2];
    PerformanceState perf_;
    std::vector<LayerFrame> frames_[2];
    std::vector<double> noise_, acc_, block_;
    Decimator dec_;
    DcBlocker outDc_;
    Rng noiseRng_{0x5EEDull};
};

}  // namespace p10

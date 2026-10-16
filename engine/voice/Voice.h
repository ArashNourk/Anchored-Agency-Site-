// One persistent Prophet voice:
//   Osc A (CEM3340) + Osc B (CEM3340) + noise -> nonlinear mixer
//   -> SSM2040 or CEM3320 4-pole LPF -> VCA, with filter/amp ADSRs and the
//   per-voice Poly Mod path. All audio-path processing (oscillators, Poly Mod,
//   filter, VCA) runs per sample at the oversampled rate, so Osc B -> Freq A /
//   PW A / Filter is genuine audio-rate modulation. Envelopes, glide and
//   drift run at the output rate and are interpolated.
//
// The voice is "physical": it keeps its oscillator phases, filter state,
// envelope voltages, last pitch (for glide) and its component personality
// across notes. Which voice a note lands on therefore matters.
#pragma once

#include <cstdint>

#include "../dsp/DspMath.h"
#include "../dsp/Envelope.h"
#include "../dsp/Filters.h"
#include "../dsp/Oscillator.h"
#include "../model/Calibration.h"
#include "../model/Vintage.h"
#include "LayerModulator.h"

namespace p10 {

class Voice {
public:
    void configure(int index, double sampleRate, int oversample, const CalibrationModel* cal);
    void setAnalogState(const VoiceAnalogState& s);

    // Note control (called by the allocator).
    void start(int note, double velocity, int layer, uint64_t stamp, bool retrigger);
    void legatoTo(int note);  // change pitch without retriggering envelopes
    void release(uint64_t stamp);
    void setDetuneCents(double c) { detuneCents_ = c; }
    void forceSilence();

    // Render n output-rate samples into the oversampled accumulator
    // (n * oversample samples). `noise` is the shared white-noise source at the
    // oversampled rate (the Prophet has one noise generator for all voices).
    void render(const LayerFrame* frames, int n, const double* noise, double* acc);

    int index() const { return index_; }
    int note() const { return note_; }
    int layer() const { return layer_; }
    bool gated() const { return gate_; }
    bool sounding() const { return ampEnv_.active(); }
    double ampLevel() const { return ampEnv_.value(); }
    uint64_t startStamp() const { return startStamp_; }
    uint64_t releaseStamp() const { return releaseStamp_; }
    double currentPitch() const { return pitch_; }
    double filterEnvValue() const { return filtEnv_.value(); }

private:
    int index_ = 0;
    double fs_ = 48000.0, fsOs_ = 192000.0;
    int os_ = 4;
    const CalibrationModel* cal_ = nullptr;
    VoiceAnalogState an_{};

    Cem3340 oscA_, oscB_;
    Ssm2040Filter ssm_;
    Cem3320Filter cem_;
    AnalogEnvelope filtEnv_, ampEnv_;
    DcBlocker mixDc_;
    double lpA_ = 0.0, lpB_ = 0.0, lpCoef_ = 1.0;
    double prevFe_ = 0.0, prevAe_ = 0.0;
    double prevG_ = -1.0, prevK_ = 0.0;
    int activeFilter_ = 0;

    // Note state.
    int note_ = -1;
    int layer_ = 0;
    bool gate_ = false;
    double velocity_ = 1.0;
    double pitch_ = 60.0, pitchTarget_ = 60.0;
    bool hasPlayed_ = false;
    double detuneCents_ = 0.0;
    uint64_t startStamp_ = 0, releaseStamp_ = 0;

    // Fast frequency noise (component thermal noise), one per oscillator.
    Rng rng_;
    double jitA_ = 0.0, jitB_ = 0.0, jitCoef_ = 0.0, jitNorm_ = 0.0;
};

}  // namespace p10

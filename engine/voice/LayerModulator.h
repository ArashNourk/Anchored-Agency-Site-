// Per-layer (per-timbre) global modulation: knob smoothing, the Prophet LFO,
// the wheel-mod source mixer (LFO <-> noise), initial amount + wheel amount,
// aftertouch extensions and pitch bend. Produces one LayerFrame per output
// sample, which every voice assigned to the layer reads.
//
// Routing is fixed, as on the panel: one modulation signal, five destination
// switches (Freq A, Freq B, PW A, PW B, Filter). No matrix.
#pragma once

#include "../dsp/DspMath.h"
#include "../model/Calibration.h"
#include "../model/Params.h"

namespace p10 {

struct PerformanceState {
    double modWheel = 0.0;    // 0..1
    double pitchBend = 0.0;   // -1..1
    double aftertouch = 0.0;  // 0..1
    bool sustain = false;
};

// Panel positions converted to physical quantities once per sample per layer
// (shared by all voices of the layer; per-voice tolerances are applied in
// the voice).
struct DerivedControls {
    double fAtk, fDec, fSus, fRel;  // filter envelope (seconds, level)
    double aAtk, aDec, aSus, aRel;  // amp envelope
    double levA, levB, levN;        // mixer gains
    double pmEnvAmt, pmBAmt;        // Poly Mod source amounts
    double oscASemis, oscBSemis;    // oscillator frequency knob offsets
    double dutyA, dutyB;            // pulse widths incl. wheel modulation
    double cutoffSemis;             // log2(Hz)*12 from knob + wheel mod + aftertouch
    double envSemis;                // filter env amount (before velocity)
    double resK;                    // nominal loop gain of the selected model
    double glideCoef;               // per-sample one-pole coefficient (1 = off)
};

struct LayerFrame {
    double p[kNumLayerParams];  // smoothed panel positions
    DerivedControls d;
    double mod;                 // wheel-mod signal after amount, bipolar
    double lfo;                 // raw combined LFO waveform (for metering/tests)
    double bendSemis;
    double atFilterSemis;
    double gain;                // layer volume * balance
};

class LayerModulator {
public:
    void configure(double sampleRate, const CalibrationModel* cal);
    void setTarget(const LayerPatch& patch, double balanceGain);
    void snapToTarget();
    void process(int n, const PerformanceState& perf, LayerFrame* out);
    double lfoPhase() const { return lfoPhase_; }

private:
    void derive(const double* p, LayerFrame& f);
    // pow() is the dominant cost of panel->physical mapping; knobs are usually
    // static, so each mapping caches its last result.
    struct PowCache {
        double base = -1.0, expo = -1.0, value = 0.0;
    };
    double cachedPow(int slot, double base, double expo);
    PowCache pow_[32];
    double resKnobCache_ = -1.0, resKCache_ = 0.0;
    const FilterModelCal* resModelCache_ = nullptr;

    const CalibrationModel* cal_ = nullptr;
    double fs_ = 48000.0;
    LayerPatch target_{};
    double balanceGain_ = 1.0;
    double smoothed_[kNumLayerParams]{};
    double smoothCoef_ = 1.0;
    bool continuous_[kNumLayerParams]{};
    double lfoPhase_ = 0.0;
    double squareSlewed_ = 0.0;
    double noiseLp_ = 0.0;
    double wheelSmoothed_ = 0.0, atSmoothed_ = 0.0, bendSmoothed_ = 0.0;
    Rng rng_{0xC0FFEEull};
};

}  // namespace p10

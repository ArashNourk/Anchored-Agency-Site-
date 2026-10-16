// Vintage / component-variation engine and the analog tuning (calibration)
// model.
//
// Each of the ten physical voices owns a persistent component set:
//
//   raw      manufacturing error of the untrimmed circuit (large)
//   cal      correction written by the tune routine (cancels raw + aging,
//            limited by correction resolution and measurement noise)
//   aging    slow random walk accumulated since the last tune
//   z        the voice's normalised "personality" draws. The Vintage knob
//            scales them by per-component sigmas, so the SAME voice always
//            deviates in the SAME direction; turning Vintage up makes each
//            voice more itself, not more random. Tuning never removes it.
//   drift    temporal Ornstein-Uhlenbeck processes (per voice, plus one
//            correlated "thermal" process shared by all voices), also scaled
//            by Vintage. Short-term drift is separate from static offsets.
//
// Effective static error = raw + cal + aging + z * sigma(vintage).
#pragma once

#include <array>
#include <cstdint>

#include "../dsp/DspMath.h"
#include "Calibration.h"

namespace p10 {

constexpr int kNumVoices = 10;

// Snapshot consumed by a voice (physical units).
struct VoiceAnalogState {
    double tuneCentsA = 0, tuneCentsB = 0;        // static offset at the reference note
    double scaleCentsPerOctA = 0, scaleCentsPerOctB = 0;
    double hfResidualSecA = 0, hfResidualSecB = 0;  // uncompensated ramp reset time
    double pwOffsetA = 0, pwOffsetB = 0;
    double sawCurvA = 0, sawCurvB = 0;
    double cutoffOffsetSemis = 0, cutoffScale = 1.0, trackError = 0, resScale = 1.0;
    double filterEnvTimeScale = 1.0, ampEnvTimeScale = 1.0, sustainOffset = 0;
    double vcaGain = 1.0, feedthroughMul = 1.0;
    double stageMismatch[4] = {0, 0, 0, 0};
    double driftCentsA = 0, driftCentsB = 0, filterDriftSemis = 0;
    double jitterCents = 0;  // RMS of fast frequency noise to apply
};

struct VoiceComponents {
    // Personality (z-scores).
    double zTuneA, zTuneB, zScaleA, zScaleB, zHfA, zHfB, zPwA, zPwB, zCurvA, zCurvB;
    double zCutoff, zTrack, zRes, zFEnv, zAEnv, zSus, zVca, zFeed, zStage[4];
    // Untrimmed circuit error.
    double rawTuneA, rawTuneB, rawScaleA, rawScaleB, rawCutoff, rawCutoffScale;
    // Correction table from the last tune.
    double calTuneA = 0, calTuneB = 0, calScaleA = 0, calScaleB = 0, calCutoff = 0, calCutoffScale = 0;
    // Accumulated aging since last tune.
    double ageTuneA = 0, ageTuneB = 0, ageCutoff = 0;
    // Temporal drift processes (unit variance; scaled by Vintage on read).
    OuProcess driftA, driftB, driftF;
};

class VintageEngine {
public:
    explicit VintageEngine(CalibrationModel* cal) : cal_(cal) {}

    // Build the persistent component sets for an instrument "serial number"
    // and run an initial tune (the factory calibration).
    void build(uint64_t serial);

    // Hardware-style tune routine: measures each voice through the same pitch
    // and cutoff functions the voices use, then writes the correction tables.
    // Leaves Vintage personality and short-term drift untouched.
    void tune();

    // Long-term aging (random walk) since the last tune.
    void age(double hours);

    // Advance temporal drift (call at control rate).
    void stepDrift(double seconds);

    // Analog state of a voice for a given Vintage knob position (Vintage is a
    // program parameter, so the two layers may use different settings).
    VoiceAnalogState state(int voice, double vintageKnob) const;

    // Static (non-drift) pitch error of a voice's oscillator at a note, in
    // cents, as a tuning measurement would see it with Vintage at zero.
    double measuredPitchErrorCents(int voice, int osc, double note) const;
    double measuredCutoffErrorSemis(int voice, double note) const;

    const VoiceComponents& components(int v) const { return comp_[v]; }

private:
    CalibrationModel* cal_;
    std::array<VoiceComponents, kNumVoices> comp_{};
    OuProcess globalDrift_;
    Rng rng_;
};

}  // namespace p10

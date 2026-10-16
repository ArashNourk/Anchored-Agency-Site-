#include "LayerModulator.h"

#include <cmath>

#include "../dsp/Filters.h"

namespace p10 {

namespace {
// Knob scan + smoothing time of the panel/MIDI controls. Long enough to remove
// zipper noise from 7-bit CC steps, short enough to feel immediate.
constexpr double kKnobSmoothSec = 0.006;
constexpr double kPerfSmoothSec = 0.004;
}  // namespace

void LayerModulator::configure(double sampleRate, const CalibrationModel* cal) {
    fs_ = sampleRate;
    cal_ = cal;
    smoothCoef_ = 1.0 - std::exp(-1.0 / (kKnobSmoothSec * fs_));
    for (int k = 0; k < kNumLayerParams; ++k) continuous_[k] = layerParamInfo(k).kind == ParamKind::Continuous;
    for (auto& pc : pow_) pc = PowCache{};
    resKnobCache_ = -1.0;
}

void LayerModulator::setTarget(const LayerPatch& patch, double balanceGain) {
    target_ = patch;
    balanceGain_ = balanceGain;
}

void LayerModulator::snapToTarget() {
    for (int i = 0; i < kNumLayerParams; ++i) smoothed_[i] = target_[i];
}

void LayerModulator::process(int n, const PerformanceState& perf, LayerFrame* out) {
    const auto& c = *cal_;
    const double perfCoef = 1.0 - std::exp(-1.0 / (kPerfSmoothSec * fs_));
    const double slewCoef = 1.0 - std::exp(-1.0 / (std::max(c.lfoSquareSlewSec, 1e-6) * fs_));
    const double noiseCoef = 1.0 - std::exp(-kTwoPi * c.wheelNoiseLpHz / fs_);
    // Unit-variance scaling of the low-passed white noise.
    const double noiseNorm = std::sqrt((2.0 - noiseCoef) / noiseCoef) * 0.5;

    for (int i = 0; i < n; ++i) {
        for (int k = 0; k < kNumLayerParams; ++k) {
            if (continuous_[k])
                smoothed_[k] += (target_[k] - smoothed_[k]) * smoothCoef_;
            else
                smoothed_[k] = target_[k];
        }
        wheelSmoothed_ += (perf.modWheel - wheelSmoothed_) * perfCoef;
        atSmoothed_ += (perf.aftertouch - atSmoothed_) * perfCoef;
        bendSmoothed_ += (perf.pitchBend - bendSmoothed_) * perfCoef;
        const double* p = smoothed_;

        // LFO: exponential rate taper, free running.
        const double hz = c.lfoMinHz * cachedPow(30, c.lfoMaxHz / c.lfoMinHz, p[kLfoFreq]);
        lfoPhase_ += hz / fs_;
        lfoPhase_ -= std::floor(lfoPhase_);
        const double saw = 2.0 * lfoPhase_ - 1.0;
        const double tri = 1.0 - 4.0 * std::fabs(lfoPhase_ - 0.5);
        const double sq = lfoPhase_ < 0.5 ? 1.0 : -1.0;
        squareSlewed_ += (sq - squareSlewed_) * slewCoef;
        double lfo = 0.0;
        if (p[kLfoSaw] > 0.5) lfo += saw;
        if (p[kLfoTri] > 0.5) lfo += tri;
        if (p[kLfoSquare] > 0.5) lfo += squareSlewed_;
        lfo *= c.lfoMultiWaveGain;

        // Wheel-mod source mix: LFO <-> noise.
        noiseLp_ += (rng_.bipolar() * 1.7320508 - noiseLp_) * noiseCoef;
        const double noise = clamp(noiseLp_ * noiseNorm, -2.0, 2.0);
        const double src = (1.0 - p[kWmSourceMix]) * lfo + p[kWmSourceMix] * noise;

        // Amount: initial amount + mod wheel (+ aftertouch when routed).
        const double initial = p[kLfoInitialAmount];
        double wheel = wheelSmoothed_;
        if (p[kAtLfoAmount] > 0.5) wheel += atSmoothed_ * c.atLfoDepth;
        double amt = c.wheelCombineMax > 0.5 ? std::max(initial, wheel) : initial + wheel;
        amt = clamp(amt, 0.0, 1.0);
        amt = cachedPow(31, amt, c.modAmountTaper);

        LayerFrame& f = out[i];
        for (int k = 0; k < kNumLayerParams; ++k) f.p[k] = p[k];
        f.lfo = lfo;
        f.mod = src * amt;
        f.bendSemis = bendSmoothed_ * target_[kBendRange];
        f.atFilterSemis = p[kAtFilter] > 0.5 ? atSmoothed_ * c.atFilterRangeSemis : 0.0;
        f.gain = p[kVolume] * p[kVolume] * balanceGain_;
        derive(p, f);
    }
}

double LayerModulator::cachedPow(int slot, double base, double expo) {
    PowCache& c = pow_[slot];
    if (base != c.base || expo != c.expo) {
        c.base = base;
        c.expo = expo;
        c.value = std::pow(base, expo);
    }
    return c.value;
}

void LayerModulator::derive(const double* p, LayerFrame& f) {
    const auto& c = *cal_;
    DerivedControls& d = f.d;
    int slot = 0;
    auto envTime = [&](double knob, double minS, double maxS) {
        const double k = cachedPow(slot++, clamp(knob, 0.0, 1.0), c.envTimeTaper);
        return minS * cachedPow(slot++, maxS / minS, k);
    };
    d.fAtk = envTime(p[kFEnvA], c.envAttackMinSec, c.envAttackMaxSec);
    d.fDec = envTime(p[kFEnvD], c.envDecayMinSec, c.envDecayMaxSec);
    d.fRel = envTime(p[kFEnvR], c.envReleaseMinSec, c.envReleaseMaxSec);
    d.aAtk = envTime(p[kAEnvA], c.envAttackMinSec, c.envAttackMaxSec);
    d.aDec = envTime(p[kAEnvD], c.envDecayMinSec, c.envDecayMaxSec);
    d.aRel = envTime(p[kAEnvR], c.envReleaseMinSec, c.envReleaseMaxSec);
    d.fSus = p[kFEnvS];
    d.aSus = p[kAEnvS];

    d.levA = cachedPow(slot++, p[kMixOscA], c.mixLevelTaper);
    d.levB = cachedPow(slot++, p[kMixOscB], c.mixLevelTaper);
    d.levN = cachedPow(slot++, p[kMixNoise], c.mixLevelTaper) * c.noiseGain;

    if (c.pmEnvBipolar > 0.5) {
        const double b = 2.0 * p[kPmFilterEnvAmt] - 1.0;
        d.pmEnvAmt = (b < 0 ? -1.0 : 1.0) * cachedPow(slot++, std::fabs(b), c.pmAmountTaper);
    } else {
        d.pmEnvAmt = cachedPow(slot++, p[kPmFilterEnvAmt], c.pmAmountTaper);
    }
    d.pmBAmt = cachedPow(slot++, p[kPmOscBAmt], c.pmAmountTaper) * c.pmOscBSourceGain;

    double sa = (p[kOscAFreq] - 0.5) * c.oscAFreqRangeSemis;
    if (c.oscAFreqQuantize > 0.5) sa = std::round(sa);
    double sb = (p[kOscBFreq] - 0.5) * c.oscBFreqRangeSemis;
    if (c.oscBFreqQuantize > 0.5) sb = std::round(sb);
    d.oscASemis = sa;
    d.oscBSemis = sb + (p[kOscBFine] - 0.5) * 2.0 * c.oscBFineRangeSemis;

    const double pwSpan = c.pwMaxDuty - c.pwMinDuty;
    d.dutyA = c.pwMinDuty + p[kOscAPw] * pwSpan + (p[kWmPwA] > 0.5 ? f.mod * c.modPwRange : 0.0);
    d.dutyB = c.pwMinDuty + p[kOscBPw] * pwSpan + (p[kWmPwB] > 0.5 ? f.mod * c.modPwRange : 0.0);

    d.cutoffSemis = 12.0 * std::log2(c.cutoffMinHz) + p[kFltCutoff] * 12.0 * std::log2(c.cutoffMaxHz / c.cutoffMinHz) +
                    f.atFilterSemis + (p[kWmFilter] > 0.5 ? f.mod * c.modFilterRangeSemis : 0.0);
    d.envSemis = p[kFltEnvAmt] * c.filterEnvRangeSemis;
    const FilterModelCal& fc = p[kFltRev] > 0.5 ? c.cem : c.ssm;
    if (p[kFltResonance] != resKnobCache_ || &fc != resModelCache_) {
        resKnobCache_ = p[kFltResonance];
        resModelCache_ = &fc;
        resKCache_ = resonanceLoopGain(p[kFltResonance], fc, 0.0);
    }
    d.resK = resKCache_;

    const double g = p[kGlide];
    if (g < 1e-3) {
        d.glideCoef = 1.0;
    } else {
        const double tau = c.glideMinSec * cachedPow(slot + 1, c.glideMaxSec / c.glideMinSec,
                                                     cachedPow(slot, g, c.glideTaper));
        d.glideCoef = 1.0 - std::exp(-1.0 / (tau * fs_));
    }
}

}  // namespace p10

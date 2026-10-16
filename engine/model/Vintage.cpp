#include "Vintage.h"

#include <cmath>

namespace p10 {

void VintageEngine::build(uint64_t serial) {
    rng_.reseed(serial * 0x2545F4914F6CDD1Dull + 0x1234567ull);
    const auto& c = *cal_;
    for (int v = 0; v < kNumVoices; ++v) {
        auto& k = comp_[v];
        k.zTuneA = rng_.gauss();
        k.zTuneB = rng_.gauss();
        k.zScaleA = rng_.gauss();
        k.zScaleB = rng_.gauss();
        k.zHfA = std::fabs(rng_.gauss());  // residual reset time can only add delay
        k.zHfB = std::fabs(rng_.gauss());
        k.zPwA = rng_.gauss();
        k.zPwB = rng_.gauss();
        k.zCurvA = rng_.gauss();
        k.zCurvB = rng_.gauss();
        k.zCutoff = rng_.gauss();
        k.zTrack = rng_.gauss();
        k.zRes = rng_.gauss();
        k.zFEnv = rng_.gauss();
        k.zAEnv = rng_.gauss();
        k.zSus = rng_.gauss();
        k.zVca = rng_.gauss();
        k.zFeed = std::fabs(rng_.gauss());
        for (double& z : k.zStage) z = rng_.gauss();
        k.rawTuneA = c.calRawTuneCents * rng_.gauss();
        k.rawTuneB = c.calRawTuneCents * rng_.gauss();
        k.rawScaleA = c.calRawScaleCentsPerOct * rng_.gauss();
        k.rawScaleB = c.calRawScaleCentsPerOct * rng_.gauss();
        k.rawCutoff = c.calRawCutoffSemis * rng_.gauss();
        k.rawCutoffScale = c.calRawCutoffScale * rng_.gauss();
        k.ageTuneA = k.ageTuneB = k.ageCutoff = 0.0;
        k.driftA.configure(c.vDriftTauSec, 1.0, 0.01);
        k.driftB.configure(c.vDriftTauSec, 1.0, 0.01);
        k.driftF.configure(c.vDriftTauSec, 1.0, 0.01);
        k.driftA.randomise(rng_);
        k.driftB.randomise(rng_);
        k.driftF.randomise(rng_);
    }
    globalDrift_.configure(c.vGlobalDriftTauSec, 1.0, 0.01);
    globalDrift_.randomise(rng_);
    tune();
}

double VintageEngine::measuredPitchErrorCents(int voice, int osc, double note) const {
    const auto& k = comp_[voice];
    const double oct = (note - 60.0) / 12.0;
    if (osc == 0)
        return (k.rawTuneA + k.calTuneA + k.ageTuneA) + (k.rawScaleA + k.calScaleA) * oct;
    return (k.rawTuneB + k.calTuneB + k.ageTuneB) + (k.rawScaleB + k.calScaleB) * oct;
}

double VintageEngine::measuredCutoffErrorSemis(int voice, double note) const {
    const auto& k = comp_[voice];
    const double oct = (note - 60.0) / 12.0;
    return (k.rawCutoff + k.calCutoff + k.ageCutoff) + (k.rawCutoffScale + k.calCutoffScale) * oct * 12.0;
}

void VintageEngine::tune() {
    const auto& c = *cal_;
    auto quant = [&](double cents) {
        const double q = std::max(c.calDacStepCents, 1e-9);
        return std::round(cents / q) * q;
    };
    // Two-point measurement per oscillator (as an autotune measures period at
    // two control voltages), then offset/scale correction.
    const double lo = 36.0, hi = 84.0;
    for (int v = 0; v < kNumVoices; ++v) {
        auto& k = comp_[v];
        for (int osc = 0; osc < 2; ++osc) {
            const double eLo = measuredPitchErrorCents(v, osc, lo) + c.calMeasureNoiseCents * rng_.gauss();
            const double eHi = measuredPitchErrorCents(v, osc, hi) + c.calMeasureNoiseCents * rng_.gauss();
            const double scale = (eHi - eLo) / ((hi - lo) / 12.0);
            const double offset = eLo - scale * ((lo - 60.0) / 12.0);
            double& calTune = osc == 0 ? k.calTuneA : k.calTuneB;
            double& calScale = osc == 0 ? k.calScaleA : k.calScaleB;
            calTune = quant(calTune - offset);
            calScale = quant(calScale - scale);
        }
        const double fLo = measuredCutoffErrorSemis(v, lo) + 0.01 * c.calMeasureNoiseCents * rng_.gauss();
        const double fHi = measuredCutoffErrorSemis(v, hi) + 0.01 * c.calMeasureNoiseCents * rng_.gauss();
        const double fScale = (fHi - fLo) / (hi - lo);
        const double fOffset = fLo - fScale * (lo - 60.0);
        k.calCutoff -= fOffset;
        k.calCutoffScale -= fScale;
    }
}

void VintageEngine::age(double hours) {
    const double s = cal_->calAgingCentsPerHour * std::sqrt(std::max(0.0, hours));
    for (auto& k : comp_) {
        k.ageTuneA += s * rng_.gauss();
        k.ageTuneB += s * rng_.gauss();
        k.ageCutoff += 0.01 * s * rng_.gauss();
    }
}

void VintageEngine::stepDrift(double seconds) {
    const auto& c = *cal_;
    globalDrift_.configure(c.vGlobalDriftTauSec, 1.0, seconds);
    globalDrift_.step(rng_);
    for (auto& k : comp_) {
        k.driftA.configure(c.vDriftTauSec, 1.0, seconds);
        k.driftB.configure(c.vDriftTauSec, 1.0, seconds);
        k.driftF.configure(c.vDriftTauSec, 1.0, seconds);
        k.driftA.step(rng_);
        k.driftB.step(rng_);
        k.driftF.step(rng_);
    }
}

VoiceAnalogState VintageEngine::state(int voice, double vintageKnob) const {
    const auto& c = *cal_;
    const auto& k = comp_[voice];
    const double v = clamp(vintageKnob, 0.0, 1.0);
    VoiceAnalogState s;
    // Static pitch error: residual after calibration + personality.
    s.tuneCentsA = (k.rawTuneA + k.calTuneA + k.ageTuneA) + k.zTuneA * c.vOscTuneCents.at(v);
    s.tuneCentsB = (k.rawTuneB + k.calTuneB + k.ageTuneB) + k.zTuneB * c.vOscTuneCents.at(v);
    s.scaleCentsPerOctA = (k.rawScaleA + k.calScaleA) + k.zScaleA * c.vOscScaleCentsPerOct.at(v);
    s.scaleCentsPerOctB = (k.rawScaleB + k.calScaleB) + k.zScaleB * c.vOscScaleCentsPerOct.at(v);
    const double hfBase = c.oscResetTimeSec * (1.0 - c.calHfTrimEfficiency);
    s.hfResidualSecA = std::max(0.0, hfBase + c.oscResetTimeSec * k.zHfA * c.vOscHfResidual.at(v));
    s.hfResidualSecB = std::max(0.0, hfBase + c.oscResetTimeSec * k.zHfB * c.vOscHfResidual.at(v));
    s.pwOffsetA = k.zPwA * c.vPwOffset.at(v);
    s.pwOffsetB = k.zPwB * c.vPwOffset.at(v);
    s.sawCurvA = c.sawCurvature + k.zCurvA * c.vSawCurvature.at(v);
    s.sawCurvB = c.sawCurvature + k.zCurvB * c.vSawCurvature.at(v);
    s.cutoffOffsetSemis = (k.rawCutoff + k.calCutoff + k.ageCutoff) + k.zCutoff * c.vCutoffSemis.at(v);
    s.cutoffScale = 1.0 + (k.rawCutoffScale + k.calCutoffScale);
    s.trackError = k.zTrack * c.vFilterTrack.at(v);
    s.resScale = std::max(0.0, 1.0 + k.zRes * c.vResScale.at(v));
    s.filterEnvTimeScale = std::max(0.2, 1.0 + k.zFEnv * c.vFilterEnvTime.at(v));
    s.ampEnvTimeScale = std::max(0.2, 1.0 + k.zAEnv * c.vAmpEnvTime.at(v));
    s.sustainOffset = k.zSus * c.vEnvSustain.at(v);
    s.vcaGain = std::pow(10.0, k.zVca * c.vVcaGainDb.at(v) / 20.0);
    s.feedthroughMul = k.zFeed * c.vVcaFeedthrough.at(v);
    for (int i = 0; i < 4; ++i) s.stageMismatch[i] = k.zStage[i] * c.vStageMismatch.at(v);
    const double g = globalDrift_.value() * c.vGlobalDriftCents.at(v);
    s.driftCentsA = k.driftA.value() * c.vDriftCents.at(v) + g;
    s.driftCentsB = k.driftB.value() * c.vDriftCents.at(v) + g;
    s.filterDriftSemis = k.driftF.value() * c.vFilterDriftSemis.at(v) + 0.01 * g;
    s.jitterCents = c.vJitterCents.at(v);
    // Measured unit profile (zero unless a fitted calibration supplies one).
    s.tuneCentsA += c.unitTuneCentsA[voice];
    s.tuneCentsB += c.unitTuneCentsB[voice];
    s.cutoffOffsetSemis += c.unitCutoffSemis[voice];
    s.filterEnvTimeScale *= std::exp(c.unitFilterEnvTimeLn[voice]);
    s.ampEnvTimeScale *= std::exp(c.unitAmpEnvTimeLn[voice]);
    s.vcaGain *= std::pow(10.0, c.unitVcaGainDb[voice] / 20.0);
    return s;
}

}  // namespace p10

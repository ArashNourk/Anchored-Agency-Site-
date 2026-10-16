#include "Voice.h"

#include <cmath>

namespace p10 {

namespace {
const double kLog2Middle = std::log2(midiNoteToHz(60.0));  // filter CV reference
}

void Voice::configure(int index, double sampleRate, int oversample, const CalibrationModel* cal) {
    index_ = index;
    fs_ = sampleRate;
    os_ = oversample;
    fsOs_ = sampleRate * oversample;
    cal_ = cal;
    ssm_.setSampleRate(fsOs_);
    cem_.setSampleRate(fsOs_);
    ssm_.reset();
    cem_.reset();
    filtEnv_.setSampleRate(fs_);
    ampEnv_.setSampleRate(fs_);
    mixDc_.setCutoff(cal->mixDcBlockHz, fsOs_);
    mixDc_.reset();
    lpCoef_ = 1.0 - std::exp(-kTwoPi * std::min(cal->oscOutputBandwidthHz, 0.45 * fsOs_) / fsOs_);
    rng_.reseed(0xA5A5A5A5ull + static_cast<uint64_t>(index) * 7919ull);
    oscA_.setOversample(oversample, sampleRate);
    oscB_.setOversample(oversample, sampleRate);
    // Free-running oscillators: arbitrary, voice-specific start phases.
    oscA_.reset(rng_.uniform());
    oscB_.reset(rng_.uniform());
    filtEnv_.hardReset();
    ampEnv_.hardReset();
    gate_ = false;
    note_ = -1;
    hasPlayed_ = false;
}

void Voice::setAnalogState(const VoiceAnalogState& s) {
    an_ = s;
    ssm_.setStageMismatch(s.stageMismatch);
    cem_.setStageMismatch(s.stageMismatch);
    oscA_.setCurvature(s.sawCurvA);
    oscB_.setCurvature(s.sawCurvB);
    const double bw = std::max(1e-3, cal_->vJitterBandwidthHz);
    jitCoef_ = 1.0 - std::exp(-kTwoPi * bw / fs_);
    jitNorm_ = std::sqrt((2.0 - jitCoef_) / jitCoef_);
}

void Voice::start(int note, double velocity, int layer, uint64_t stamp, bool retrigger) {
    note_ = note;
    layer_ = layer;
    velocity_ = clamp(velocity, 0.0, 1.0);
    pitchTarget_ = note;
    if (!hasPlayed_) {
        pitch_ = note;  // a never-used voice has no previous pitch to glide from
        hasPlayed_ = true;
    }
    gate_ = true;
    startStamp_ = stamp;
    if (retrigger || !ampEnv_.active()) {
        filtEnv_.gateOn();
        ampEnv_.gateOn();
    }
}

void Voice::legatoTo(int note) {
    note_ = note;
    pitchTarget_ = note;
}

void Voice::release(uint64_t stamp) {
    gate_ = false;
    releaseStamp_ = stamp;
    filtEnv_.gateOff();
    ampEnv_.gateOff();
}

void Voice::forceSilence() {
    gate_ = false;
    filtEnv_.hardReset();
    ampEnv_.hardReset();
    ssm_.reset();
    cem_.reset();
}

void Voice::render(const LayerFrame* frames, int n, const double* noise, double* acc) {
    const CalibrationModel& c = *cal_;
    const bool polarity = c.pulseHighAtRampEnd > 0.5;
    oscA_.setPulsePolarity(polarity);
    oscB_.setPulsePolarity(polarity);
    filtEnv_.setShape(c.envAttackTarget, c.envDecayTauDivisor);
    ampEnv_.setShape(c.envAttackTarget, c.envDecayTauDivisor);

    // Silent voice: keep the free-running oscillators and the filter envelope
    // capacitor moving, skip the audio path.
    if (!gate_ && !ampEnv_.active()) {
        const DerivedControls& d = frames[n - 1].d;
        const EnvTimes ft{1.0, 1.0, 0.0, d.fRel * an_.filterEnvTimeScale};
        for (int i = 0; i < n; ++i) filtEnv_.process(ft);
        pitch_ = pitchTarget_;
        const double cycles = midiNoteToHz(pitch_) * n / fs_;
        oscA_.advanceSilently(cycles);
        oscB_.advanceSilently(cycles * 1.0007);
        prevFe_ = filtEnv_.value();
        prevAe_ = 0.0;
        prevG_ = -1.0;
        return;
    }

    const double invOs = 1.0 / os_;
    const double velCurve = std::pow(velocity_, c.velCurve);
    const double fsOsInv = 1.0 / fsOs_;
    const double piOverFs = kPi * fsOsInv;
    const double maxHz = 0.45 * fsOs_;
    const double ref = 12.0 * kLog2Middle;
    const double satY0 = fastTanh(c.mixAsym);
    const double invDrive = 1.0 / c.mixDrive;
    const double mixMax = c.waveCombineMax;
    const double feScale = an_.filterEnvTimeScale, aeScale = an_.ampEnvTimeScale;

    for (int i = 0; i < n; ++i) {
        const LayerFrame& f = frames[i];
        const double* p = f.p;
        const DerivedControls& d = f.d;

        // ---- Control-rate section (output sample rate) --------------------
        pitch_ += (pitchTarget_ - pitch_) * d.glideCoef;
        // Unit-variance uniform noise; after the low-pass it is Gaussian-like.
        jitA_ += (rng_.bipolar() * 1.7320508075688772 - jitA_) * jitCoef_;
        jitB_ += (rng_.bipolar() * 1.7320508075688772 - jitB_) * jitCoef_;

        const EnvTimes ft{d.fAtk * feScale, d.fDec * feScale, clamp(d.fSus + an_.sustainOffset, 0.0, 1.0),
                          d.fRel * feScale};
        const EnvTimes at{d.aAtk * aeScale, d.aDec * aeScale, clamp(d.aSus + an_.sustainOffset, 0.0, 1.0),
                          d.aRel * aeScale};
        const double fe = filtEnv_.process(ft);
        const double ae = ampEnv_.process(at);

        // Oscillator pitches (MIDI note units) including tolerances and drift.
        const double modFreq = f.mod * c.modFreqRangeSemis;
        double semisA = pitch_ + d.oscASemis + f.bendSemis + detuneCents_ * 0.01;
        if (p[kWmFreqA] > 0.5) semisA += modFreq;
        semisA += 0.01 * (an_.tuneCentsA + an_.scaleCentsPerOctA * (semisA - 60.0) * (1.0 / 12.0) +
                          an_.driftCentsA + an_.jitterCents * jitA_ * jitNorm_);
        const double keyB = p[kOscBKbd] > 0.5 ? pitch_ : c.oscBKbdOffRefNote;
        double semisB = keyB + d.oscBSemis + f.bendSemis + detuneCents_ * 0.01;
        if (p[kOscBLoFreq] > 0.5) semisB += c.oscBLoFreqShiftSemis;
        if (p[kWmFreqB] > 0.5) semisB += modFreq;
        semisB += 0.01 * (an_.tuneCentsB + an_.scaleCentsPerOctB * (semisB - 60.0) * (1.0 / 12.0) +
                          an_.driftCentsB + an_.jitterCents * jitB_ * jitNorm_);
        const double hzA0 = midiNoteToHz(semisA);
        const double hzB = Cem3340::effectiveFrequency(midiNoteToHz(semisB), an_.hfResidualSecB);
        const double dtB = clamp(hzB * fsOsInv, 1e-9, 0.45);
        const double dutyA = d.dutyA + an_.pwOffsetA;
        const double dutyB = clamp(d.dutyB + an_.pwOffsetB, 0.0, 1.0);

        const bool pmToFreqA = p[kPmFreqA] > 0.5, pmToPwA = p[kPmPwA] > 0.5, pmToFilter = p[kPmFilter] > 0.5;
        const double pmEnvAmt = d.pmEnvAmt, pmBAmt = d.pmBAmt;

        // Filter control voltage (semitones of log frequency), static part.
        const int kbdMode = static_cast<int>(p[kFltKbd] + 0.5);
        const double track = (kbdMode == 0 ? 0.0 : kbdMode == 1 ? c.filterKbdHalf : 1.0) * (1.0 + an_.trackError);
        double envSemis = d.envSemis;
        if (p[kFltVel] > 0.5) envSemis *= 1.0 - c.velFilterDepth + c.velFilterDepth * velCurve;
        const double cvStatic = d.cutoffSemis + track * (pitch_ - c.filterKbdRefNote) + an_.cutoffOffsetSemis +
                                an_.filterDriftSemis;

        const int filterSel = p[kFltRev] > 0.5 ? 1 : 0;
        if (filterSel != activeFilter_) {
            activeFilter_ = filterSel;
            if (filterSel) cem_.reset(); else ssm_.reset();
        }
        const FilterModelCal& fc = filterSel ? c.cem : c.ssm;
        const double cvScale = an_.cutoffScale * fc.cutoffScale;
        auto cvToHz = [&](double cv) { return clamp(std::exp2((ref + (cv - ref) * cvScale) * (1.0 / 12.0)), 5.0, maxHz); };
        FilterControl ctl{};
        ctl.inputGain = fc.inputGain;
        ctl.outputGain = fc.outputGain;
        ctl.passbandComp = fc.passbandComp;
        ctl.fbDrive = fc.fbDrive;
        ctl.stageDrive = fc.stageDrive;
        const double kNominal = d.resK * an_.resScale;

        // Without audio-rate filter modulation the cutoff is computed once per
        // output sample and the integrator gain interpolated across the
        // oversampled sub-steps.
        double gEnd = 0.0, kEnd = 0.0;
        if (!pmToFilter) {
            const double hz = cvToHz(cvStatic + envSemis * fe);
            gEnd = std::tan(piOverFs * hz);
            const double r = hz * (1.0 / 20000.0);
            kEnd = kNominal / (1.0 + fc.hfResDamp * r * r);
            if (prevG_ < 0.0) {
                prevG_ = gEnd;
                prevK_ = kEnd;
            }
        }

        double ampVel = 1.0;
        if (p[kAEnvVel] > 0.5) ampVel = 1.0 - c.velAmpDepth + c.velAmpDepth * velCurve;
        const double vcaGain = ampVel * an_.vcaGain;
        const double feed = c.vcaFeedthrough * an_.feedthroughMul;
        const double outGain = f.gain;
        const double levA = d.levA, levB = d.levB, levN = d.levN;

        const bool aSaw = p[kOscASaw] > 0.5, aPulse = p[kOscAPulse] > 0.5;
        const bool bSaw = p[kOscBSaw] > 0.5, bTri = p[kOscBTri] > 0.5, bPulse = p[kOscBPulse] > 0.5;
        const bool sync = p[kOscASync] > 0.5;
        auto combine = [&](const Cem3340::Out& o, bool s, bool pu, bool t) {
            double sum = 0.0, mx = -1e30;
            int cnt = 0;
            if (s) { const double v = o.saw * c.sawLevel; sum += v; mx = std::max(mx, v); ++cnt; }
            if (pu) { const double v = o.pulse * c.pulseLevel; sum += v; mx = std::max(mx, v); ++cnt; }
            if (t) { const double v = o.tri * c.triLevel; sum += v; mx = std::max(mx, v); ++cnt; }
            if (cnt == 0) return 0.0;
            return mixMax == 0.0 ? sum : (1.0 - mixMax) * sum + mixMax * mx;
        };

        // ---- Audio-rate section (oversampled) -------------------------------
        for (int j = 0; j < os_; ++j) {
            const double frac = (j + 1) * invOs;
            const double feI = prevFe_ + (fe - prevFe_) * frac;
            const double aeI = prevAe_ + (ae - prevAe_) * frac;

            Cem3340::Out ob, oa;
            const double wrapB = oscB_.process(dtB, dutyB, -1.0, ob);
            const double bOut = combine(ob, bSaw, bPulse, bTri);

            // Poly Mod: per voice, per oversampled sample.
            const double pm = pmEnvAmt * feI + pmBAmt * bOut;
            double hzA = hzA0;
            if (pmToFreqA) hzA *= std::exp2(pm * c.pmFreqARangeSemis * (1.0 / 12.0));
            hzA = Cem3340::effectiveFrequency(hzA, an_.hfResidualSecA);
            const double dtA = clamp(hzA * fsOsInv, 1e-9, 0.45);
            double dA = dutyA;
            if (pmToPwA) dA += pm * c.pmPwARange;
            oscA_.process(dtA, clamp(dA, 0.0, 1.0), sync ? wrapB : -1.0, oa);
            const double aOut = combine(oa, aSaw, aPulse, false);

            lpA_ += (aOut - lpA_) * lpCoef_;
            lpB_ += (bOut - lpB_) * lpCoef_;

            // Nonlinear mixer (summing amp into the filter input).
            double mix = levA * lpA_ + levB * lpB_ + levN * noise[i * os_ + j];
            mix = (fastTanh(mix * c.mixDrive + c.mixAsym) - satY0) * invDrive;
            mix = mixDc_.process(mix) + c.filterNoiseFloor * rng_.bipolar();

            if (pmToFilter) {
                const double hz = cvToHz(cvStatic + envSemis * feI + pm * c.pmFilterRangeSemis);
                ctl.g = std::tan(piOverFs * hz);
                const double r = hz * (1.0 / 20000.0);
                ctl.k = kNominal / (1.0 + fc.hfResDamp * r * r);
            } else {
                ctl.g = prevG_ + (gEnd - prevG_) * frac;
                ctl.k = prevK_ + (kEnd - prevK_) * frac;
            }
            const double y = filterSel ? cem_.process(mix, ctl) : ssm_.process(mix, ctl);

            // VCA: linear control, soft output stage, control feed-through.
            const double g = aeI * vcaGain;
            double out = y * g;
            if (c.vcaSat > 0.0) out = fastTanh(out * c.vcaSat) / c.vcaSat;
            out += feed * aeI;
            acc[i * os_ + j] += out * outGain;
        }
        prevFe_ = fe;
        prevAe_ = ae;
        if (pmToFilter) {
            prevG_ = ctl.g;
            prevK_ = ctl.k;
        } else {
            prevG_ = gEnd;
            prevK_ = kEnd;
        }
    }
}

}  // namespace p10

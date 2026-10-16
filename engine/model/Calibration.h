// CalibrationModel: every constant whose true Prophet-10 Rev4 value is not
// known from documentation lives here, never inline in DSP code.
//
// Each entry carries an assumption ID (A-xxx-nn). docs/ASSUMPTIONS.md describes
// for every ID what is assumed, why, and the measurement procedure that
// determines the real value from reference hardware. Values can be overridden
// at runtime from a key=value calibration file produced by the fitting tools
// in tools/analysis.
#pragma once

#include <string>
#include <vector>

namespace p10 {

// Locale-independent-enough, exception-free number parsing (whole string).
bool parseNumber(const std::string& text, double& out);

// Standard deviations of a component tolerance at the four Vintage anchor
// positions (knob 0.0 = Rev4-calibrated ... knob 1.0 = Rev1-like).
struct VintageSigma {
    double rev4 = 0.0, rev3 = 0.0, rev2 = 0.0, rev1 = 0.0;
    double at(double vintageKnob) const;
};

// Per-filter-model constants (the two models share the structure of the
// calibration set but have very different topologies, see Filters.h).
struct FilterModelCal {
    double inputGain;       // mixer level -> filter input (drive)
    double outputGain;      // filter output make-up gain
    double resOnsetKnob;    // resonance knob position where linear loop gain reaches 4
    double resCurve;        // exponent of the resonance taper below onset
    double kMax;            // loop gain at resonance knob = 1.0
    double passbandComp;    // fraction of the resonance-induced passband loss compensated
    double fbDrive;         // drive of the feedback-path saturator
    double stageDrive;      // drive of the per-stage nonlinearity
    double hfResDamp;       // loop-gain loss proportional to cutoff/fs_os (parasitics)
    double cutoffScale;     // control-voltage to cutoff scale correction (1.0 = ideal)
};

struct CalibrationModel {
    // ---- Oscillators -------------------------------------------------------
    double oscAFreqRangeSemis = 48.0;     // A-OSC-01
    double oscAFreqQuantize = 1.0;        // A-OSC-02 (1 = semitone steps)
    double oscBFreqRangeSemis = 48.0;     // A-OSC-01
    double oscBFreqQuantize = 1.0;        // A-OSC-02
    double oscBFineRangeSemis = 1.0;      // A-OSC-03 (+/- semitones)
    double oscBLoFreqShiftSemis = -84.0;  // A-OSC-04
    double oscBKbdOffRefNote = 60.0;      // A-OSC-05
    double sawLevel = 1.0;                // A-OSC-06
    double pulseLevel = 1.0;              // A-OSC-06
    double triLevel = 1.0;                // A-OSC-06
    double waveCombineMax = 0.0;          // A-OSC-07 (0 = summed, 1 = diode-OR max)
    double pwMinDuty = 0.0;               // A-OSC-08
    double pwMaxDuty = 1.0;               // A-OSC-08
    double oscResetTimeSec = 1.0e-6;      // A-OSC-09 (raw 3340 reset/retrace time)
    double sawCurvature = 0.015;          // A-OSC-10 (nominal ramp bow)
    double oscOutputBandwidthHz = 60000;  // A-OSC-12 (output buffer slew)
    double pulseHighAtRampEnd = 1.0;      // A-OSC-13 (comparator phase vs. ramp)

    // ---- Mixer / noise ----------------------------------------------------
    double mixLevelTaper = 1.5;           // A-MIX-01 (gain = knob^taper)
    double noiseGain = 0.7;               // A-MIX-02
    double mixDrive = 0.55;               // A-MIX-03 (sum -> saturator scale)
    double mixAsym = 0.06;                // A-MIX-04
    double mixDcBlockHz = 4.0;            // A-MIX-05

    // ---- Filter (shared) --------------------------------------------------
    double cutoffMinHz = 10.0;            // A-FLT-01
    double cutoffMaxHz = 24000.0;         // A-FLT-01
    double filterEnvRangeSemis = 120.0;   // A-FLT-02
    double filterKbdRefNote = 60.0;       // A-FLT-03
    double filterKbdHalf = 0.5;           // A-FLT-04
    double filterNoiseFloor = 1.0e-5;     // A-FLT-05

    // ---- Filter models (A-FLT-1x: SSM2040/SSI2140, A-FLT-2x: CEM3320) -----
    FilterModelCal ssm{1.15, 1.45, 0.86, 1.35, 4.6, 0.55, 0.85, 1.0, 0.55, 1.0};
    FilterModelCal cem{0.95, 1.85, 0.90, 1.6, 4.4, 0.0, 1.3, 1.0, 0.35, 1.0};

    // ---- Envelopes --------------------------------------------------------
    double envAttackMinSec = 0.0010;      // A-ENV-01
    double envAttackMaxSec = 10.0;        // A-ENV-01
    double envDecayMinSec = 0.0020;       // A-ENV-02
    double envDecayMaxSec = 15.0;         // A-ENV-02
    double envReleaseMinSec = 0.0020;     // A-ENV-03
    double envReleaseMaxSec = 15.0;       // A-ENV-03
    double envTimeTaper = 1.0;            // A-ENV-04
    double envAttackTarget = 1.30;        // A-ENV-05 (RC charge target, 1.0 = trip point)
    double envDecayTauDivisor = 4.6;      // A-ENV-06 (knob time / tau; 4.6 ~ -40 dB)
    double velFilterDepth = 1.0;          // A-ENV-07
    double velAmpDepth = 0.7;             // A-ENV-08
    double velCurve = 1.0;                // A-ENV-09

    // ---- VCA -------------------------------------------------------------
    double vcaFeedthrough = 0.0015;       // A-VCA-01
    double vcaSat = 0.25;                 // A-VCA-02

    // ---- Poly Mod -----------------------------------------------------------
    double pmFreqARangeSemis = 48.0;      // A-PM-01
    double pmPwARange = 0.5;              // A-PM-02
    double pmFilterRangeSemis = 96.0;     // A-PM-03
    double pmEnvBipolar = 0.0;            // A-PM-04
    double pmOscBSourceGain = 1.0;        // A-PM-05
    double pmAmountTaper = 2.0;           // A-PM-06

    // ---- LFO / wheel / aftertouch ----------------------------------------
    double lfoMinHz = 0.03;               // A-LFO-01
    double lfoMaxHz = 25.0;               // A-LFO-01
    double lfoMultiWaveGain = 1.0;        // A-LFO-02
    double lfoSquareSlewSec = 0.0008;     // A-LFO-03
    double wheelNoiseLpHz = 80.0;         // A-LFO-04
    double wheelCombineMax = 0.0;         // A-LFO-05 (0 = initial+wheel summed, 1 = max)
    double modFreqRangeSemis = 12.0;      // A-LFO-06
    double modPwRange = 0.45;             // A-LFO-06
    double modFilterRangeSemis = 60.0;    // A-LFO-06
    double modAmountTaper = 2.0;          // A-LFO-07
    double atFilterRangeSemis = 36.0;     // A-AT-01
    double atLfoDepth = 1.0;              // A-AT-02

    // ---- Glide / unison ---------------------------------------------------
    double glideMinSec = 0.0005;          // A-GLD-01 (RC time constant at knob 0+)
    double glideMaxSec = 3.0;             // A-GLD-01
    double glideTaper = 1.0;              // A-GLD-02
    double unisonDetuneMaxCents = 50.0;   // A-UNI-01

    // ---- Vintage (per-voice persistent tolerances, std dev at anchors) ------
    VintageSigma vOscTuneCents{0.3, 1.5, 3.0, 5.0};          // A-VIN-01
    VintageSigma vOscScaleCentsPerOct{0.1, 0.6, 1.5, 3.0};    // A-VIN-02
    VintageSigma vOscHfResidual{0.03, 0.12, 0.22, 0.35};      // A-VIN-03
    VintageSigma vPwOffset{0.002, 0.01, 0.02, 0.03};          // A-VIN-04
    VintageSigma vSawCurvature{0.004, 0.01, 0.02, 0.03};      // A-VIN-05
    VintageSigma vCutoffSemis{0.2, 1.0, 2.0, 3.0};            // A-VIN-06
    VintageSigma vFilterTrack{0.005, 0.02, 0.04, 0.06};       // A-VIN-07
    VintageSigma vResScale{0.01, 0.03, 0.05, 0.08};           // A-VIN-08
    VintageSigma vFilterEnvTime{0.01, 0.05, 0.10, 0.15};      // A-VIN-09
    VintageSigma vAmpEnvTime{0.01, 0.05, 0.10, 0.15};         // A-VIN-09
    VintageSigma vEnvSustain{0.002, 0.01, 0.02, 0.03};        // A-VIN-10
    VintageSigma vVcaGainDb{0.1, 0.3, 0.6, 1.0};              // A-VIN-11
    VintageSigma vVcaFeedthrough{0.2, 0.5, 0.8, 1.0};         // A-VIN-12 (multiplier)
    VintageSigma vStageMismatch{0.002, 0.008, 0.015, 0.025};  // A-VIN-13
    VintageSigma vDriftCents{0.05, 0.5, 1.5, 3.0};            // A-VIN-14 (per-voice OU)
    VintageSigma vGlobalDriftCents{0.02, 0.3, 1.0, 2.0};      // A-VIN-15 (correlated OU)
    VintageSigma vJitterCents{0.02, 0.1, 0.25, 0.5};          // A-VIN-16 (fast FM noise)
    VintageSigma vFilterDriftSemis{0.01, 0.1, 0.3, 0.5};      // A-VIN-17
    double vDriftTauSec = 25.0;                               // A-VIN-14
    double vGlobalDriftTauSec = 140.0;                        // A-VIN-15
    double vJitterBandwidthHz = 40.0;                         // A-VIN-16

    // ---- Calibration / tuning model --------------------------------------
    double calRawTuneCents = 40.0;        // A-CAL-01 (untrimmed component spread)
    double calRawScaleCentsPerOct = 25.0; // A-CAL-01
    double calRawCutoffSemis = 6.0;       // A-CAL-01
    double calRawCutoffScale = 0.05;      // A-CAL-01
    double calDacStepCents = 0.12;        // A-CAL-02 (correction resolution)
    double calMeasureNoiseCents = 0.05;   // A-CAL-03
    double calAgingCentsPerHour = 0.4;    // A-CAL-04 (random-walk rate since last tune)
    double calHfTrimEfficiency = 0.9;     // A-CAL-05 (fraction of reset-time error trimmed)

    // ---- Voice allocation -------------------------------------------------
    double allocMode = 0.0;               // A-VAL-01 (0 rotate, 1 oldest released, 2 lowest)
    double allocReuseSameNote = 1.0;      // A-VAL-02
    double allocStealQuietest = 0.0;      // A-VAL-03 (0 = steal oldest)
    double allocStackFixedHalves = 1.0;   // A-VAL-04
    double allocSplitFixedHalves = 1.0;   // A-VAL-05

    // ---- Output -------------------------------------------------------------
    double outputGain = 0.25;             // A-OUT-01
    double outputDcHz = 3.0;              // A-OUT-02

    // ---- Unit voice profile (A-UNIT-01) ---------------------------------
    // Measured persistent offsets of each physical voice of ONE reference
    // instrument, fitted by tools/validate from the voice-fingerprint test.
    // Zero = no unit profile. Applied on top of the Vintage model.
    static constexpr int kUnitVoices = 10;
    double unitTuneCentsA[kUnitVoices]{};
    double unitTuneCentsB[kUnitVoices]{};
    double unitCutoffSemis[kUnitVoices]{};
    double unitFilterEnvTimeLn[kUnitVoices]{};  // ln of time scale
    double unitAmpEnvTimeLn[kUnitVoices]{};
    double unitVcaGainDb[kUnitVoices]{};

    // Registry used for file I/O and the fitting tools.
    struct Entry {
        std::string key;
        double* value;
        std::string assumption;
    };
    std::vector<Entry> entries();

    // key=value text. Unknown keys are reported in `errors`, not fatal.
    bool loadFromText(const std::string& text, std::vector<std::string>* errors = nullptr);
    bool loadFromFile(const std::string& path, std::vector<std::string>* errors = nullptr);
    std::string toText();
};

}  // namespace p10

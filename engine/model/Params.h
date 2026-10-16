// Parameter definitions. Parameters are stored as *panel positions* (knob
// rotation 0..1, switch 0/1, stepped integer). Conversion to physical units
// (Hz, seconds, semitones, loop gain) happens in the voice/modulation code
// through the tapers in CalibrationModel, so a stored program always means
// "the knobs were here", exactly like a hardware program.
#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace p10 {

enum class Section : uint8_t {
    Lfo,
    WheelMod,
    PolyMod,
    OscA,
    OscB,
    Mixer,
    Filter,
    FilterEnv,
    AmpEnv,
    Performance,
    Vintage,
    Program,
};

enum class ParamKind : uint8_t { Continuous, Switch, Stepped };

// Per-layer (per-timbre) parameters. Prophet-10 programs contain two layers
// (A and B) used by split/stack modes.
enum LayerParam : int {
    // LFO
    kLfoFreq,
    kLfoSaw,
    kLfoTri,
    kLfoSquare,
    kLfoInitialAmount,
    // Wheel modulation
    kWmSourceMix,
    kWmFreqA,
    kWmFreqB,
    kWmPwA,
    kWmPwB,
    kWmFilter,
    // Aftertouch destinations
    kAtFilter,
    kAtLfoAmount,
    // Poly Mod
    kPmFilterEnvAmt,
    kPmOscBAmt,
    kPmFreqA,
    kPmPwA,
    kPmFilter,
    // Oscillator A
    kOscAFreq,
    kOscASaw,
    kOscAPulse,
    kOscAPw,
    kOscASync,
    // Oscillator B
    kOscBFreq,
    kOscBFine,
    kOscBSaw,
    kOscBTri,
    kOscBPulse,
    kOscBPw,
    kOscBLoFreq,
    kOscBKbd,
    // Mixer
    kMixOscA,
    kMixOscB,
    kMixNoise,
    // Filter
    kFltCutoff,
    kFltResonance,
    kFltEnvAmt,
    kFltKbd,  // 0 off, 1 half, 2 full
    kFltRev,  // 0 = Rev1/2 (SSM2040 / SSI2140), 1 = Rev3 (CEM3320)
    kFltVel,
    // Filter envelope
    kFEnvA,
    kFEnvD,
    kFEnvS,
    kFEnvR,
    // Amp envelope
    kAEnvA,
    kAEnvD,
    kAEnvS,
    kAEnvR,
    kAEnvVel,
    // Performance
    kGlide,
    kUnison,
    kUnisonVoices,
    kUnisonDetune,
    kKeyPriority,  // 0 low, 1 high, 2 last
    kRetrigger,
    kBendRange,
    kVolume,
    // Vintage
    kVintage,
    kNumLayerParams
};

// Program-level parameters (the bi-timbral configuration).
enum ProgramParam : int {
    kLayerMode,    // 0 single (A), 1 split, 2 stack
    kSplitPoint,   // MIDI note; notes >= split point go to the upper layer
    kSplitUpperB,  // 1: layer B above the split (default), 0: layer A above
    kLayerABalance,
    kNumProgramParams
};

struct ParamInfo {
    const char* key;
    const char* name;
    Section section;
    ParamKind kind;
    double minValue;
    double maxValue;
    double defaultValue;
    int cc;  // default MIDI CC (layer A / program), -1 = NRPN only
};

const ParamInfo& layerParamInfo(int id);
const ParamInfo& programParamInfo(int id);
int findLayerParam(const std::string& key);
int findProgramParam(const std::string& key);

using LayerPatch = std::array<double, kNumLayerParams>;
using ProgramSettings = std::array<double, kNumProgramParams>;

struct Program {
    std::string name = "Basic Program";
    LayerPatch layer[2];
    ProgramSettings settings;
    Program();
    static Program init();  // the "basic patch"
};

}  // namespace p10

// UI-independent description of the control surface, in the order of the
// instrument's physical mental model (left to right). A future UI (standalone
// or plugin editor) renders exactly these controls, one control per function,
// with no menus for synthesis functions. The visual identity is to be
// original; nothing here encodes another manufacturer's artwork or trade dress.
//
// Per docs/DEVELOPMENT.md the final UI is built only after the engine has been
// validated against reference hardware.
#pragma once

#include <vector>

#include "../model/Params.h"

namespace p10 {

struct PanelSection {
    const char* title;
    std::vector<int> layerParams;  // LayerParam ids, in panel order
};

inline std::vector<PanelSection> panelLayout() {
    return {
        {"LFO", {kLfoFreq, kLfoInitialAmount, kLfoSaw, kLfoTri, kLfoSquare}},
        {"Wheel Mod", {kWmSourceMix, kWmFreqA, kWmFreqB, kWmPwA, kWmPwB, kWmFilter, kAtFilter, kAtLfoAmount}},
        {"Poly Mod", {kPmFilterEnvAmt, kPmOscBAmt, kPmFreqA, kPmPwA, kPmFilter}},
        {"Oscillator A", {kOscAFreq, kOscASaw, kOscAPulse, kOscAPw, kOscASync}},
        {"Oscillator B", {kOscBFreq, kOscBFine, kOscBSaw, kOscBTri, kOscBPulse, kOscBPw, kOscBLoFreq, kOscBKbd}},
        {"Mixer", {kMixOscA, kMixOscB, kMixNoise}},
        {"Filter", {kFltCutoff, kFltResonance, kFltEnvAmt, kFltKbd, kFltRev, kFltVel}},
        {"Filter Envelope", {kFEnvA, kFEnvD, kFEnvS, kFEnvR}},
        {"Amplifier Envelope", {kAEnvA, kAEnvD, kAEnvS, kAEnvR, kAEnvVel}},
        {"Performance", {kGlide, kUnison, kUnisonVoices, kUnisonDetune, kKeyPriority, kRetrigger, kBendRange,
                         kVolume}},
        {"Vintage", {kVintage}},
    };
}

}  // namespace p10

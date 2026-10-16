#include "Params.h"

#include <cstring>

namespace p10 {

namespace {
using S = Section;
using K = ParamKind;

// The default CC numbers are a project-defined map (not Sequential's): it uses
// only CCs that are undefined in the MIDI 1.0 spec. Every parameter is also
// reachable by NRPN (see midi/Midi.h). Initial values form the basic patch.
const ParamInfo kLayer[kNumLayerParams] = {
    {"lfo_freq", "LFO Frequency", S::Lfo, K::Continuous, 0, 1, 0.45, 3},
    {"lfo_saw", "LFO Sawtooth", S::Lfo, K::Switch, 0, 1, 0, 9},
    {"lfo_tri", "LFO Triangle", S::Lfo, K::Switch, 0, 1, 1, 14},
    {"lfo_square", "LFO Square", S::Lfo, K::Switch, 0, 1, 0, 15},
    {"lfo_initial_amount", "LFO Initial Amount", S::Lfo, K::Continuous, 0, 1, 0, 16},
    {"wm_source_mix", "Wheel Mod Source Mix (LFO-Noise)", S::WheelMod, K::Continuous, 0, 1, 0, 17},
    {"wm_freq_a", "Wheel Mod -> Freq A", S::WheelMod, K::Switch, 0, 1, 1, 18},
    {"wm_freq_b", "Wheel Mod -> Freq B", S::WheelMod, K::Switch, 0, 1, 1, 19},
    {"wm_pw_a", "Wheel Mod -> PW A", S::WheelMod, K::Switch, 0, 1, 0, 20},
    {"wm_pw_b", "Wheel Mod -> PW B", S::WheelMod, K::Switch, 0, 1, 0, 21},
    {"wm_filter", "Wheel Mod -> Filter", S::WheelMod, K::Switch, 0, 1, 0, 22},
    {"at_filter", "Aftertouch -> Filter Cutoff", S::WheelMod, K::Switch, 0, 1, 0, 23},
    {"at_lfo_amount", "Aftertouch -> LFO Amount", S::WheelMod, K::Switch, 0, 1, 0, 24},
    {"pm_filter_env", "Poly Mod Filter Env Amount", S::PolyMod, K::Continuous, 0, 1, 0, 25},
    {"pm_osc_b", "Poly Mod Osc B Amount", S::PolyMod, K::Continuous, 0, 1, 0, 26},
    {"pm_freq_a", "Poly Mod -> Freq A", S::PolyMod, K::Switch, 0, 1, 0, 27},
    {"pm_pw_a", "Poly Mod -> PW A", S::PolyMod, K::Switch, 0, 1, 0, 28},
    {"pm_filter", "Poly Mod -> Filter", S::PolyMod, K::Switch, 0, 1, 0, 29},
    {"osca_freq", "Osc A Frequency", S::OscA, K::Continuous, 0, 1, 0.5, 30},
    {"osca_saw", "Osc A Sawtooth", S::OscA, K::Switch, 0, 1, 1, 31},
    {"osca_pulse", "Osc A Pulse", S::OscA, K::Switch, 0, 1, 0, 85},
    {"osca_pw", "Osc A Pulse Width", S::OscA, K::Continuous, 0, 1, 0.5, 86},
    {"osca_sync", "Osc A Sync (to B)", S::OscA, K::Switch, 0, 1, 0, 87},
    {"oscb_freq", "Osc B Frequency", S::OscB, K::Continuous, 0, 1, 0.5, 89},
    {"oscb_fine", "Osc B Fine", S::OscB, K::Continuous, 0, 1, 0.5, 90},
    {"oscb_saw", "Osc B Sawtooth", S::OscB, K::Switch, 0, 1, 1, 102},
    {"oscb_tri", "Osc B Triangle", S::OscB, K::Switch, 0, 1, 0, 103},
    {"oscb_pulse", "Osc B Pulse", S::OscB, K::Switch, 0, 1, 0, 104},
    {"oscb_pw", "Osc B Pulse Width", S::OscB, K::Continuous, 0, 1, 0.5, 105},
    {"oscb_lofreq", "Osc B Low Frequency", S::OscB, K::Switch, 0, 1, 0, 106},
    {"oscb_kbd", "Osc B Keyboard", S::OscB, K::Switch, 0, 1, 1, 107},
    {"mix_osca", "Mixer Osc A", S::Mixer, K::Continuous, 0, 1, 0.8, 108},
    {"mix_oscb", "Mixer Osc B", S::Mixer, K::Continuous, 0, 1, 0.0, 109},
    {"mix_noise", "Mixer Noise", S::Mixer, K::Continuous, 0, 1, 0.0, 110},
    {"flt_cutoff", "Filter Cutoff", S::Filter, K::Continuous, 0, 1, 1.0, 74},
    {"flt_resonance", "Filter Resonance", S::Filter, K::Continuous, 0, 1, 0.0, 71},
    {"flt_env_amt", "Filter Envelope Amount", S::Filter, K::Continuous, 0, 1, 0.0, 111},
    {"flt_kbd", "Filter Keyboard Tracking", S::Filter, K::Stepped, 0, 2, 2, 112},
    {"flt_rev", "Filter Revision (0 Rev1/2, 1 Rev3)", S::Filter, K::Switch, 0, 1, 0, 113},
    {"flt_vel", "Filter Envelope Velocity", S::Filter, K::Switch, 0, 1, 0, 114},
    {"fenv_attack", "Filter Env Attack", S::FilterEnv, K::Continuous, 0, 1, 0.0, 115},
    {"fenv_decay", "Filter Env Decay", S::FilterEnv, K::Continuous, 0, 1, 0.5, 116},
    {"fenv_sustain", "Filter Env Sustain", S::FilterEnv, K::Continuous, 0, 1, 0.0, 117},
    {"fenv_release", "Filter Env Release", S::FilterEnv, K::Continuous, 0, 1, 0.4, 118},
    {"aenv_attack", "Amp Env Attack", S::AmpEnv, K::Continuous, 0, 1, 0.0, 73},
    {"aenv_decay", "Amp Env Decay", S::AmpEnv, K::Continuous, 0, 1, 0.5, 75},
    {"aenv_sustain", "Amp Env Sustain", S::AmpEnv, K::Continuous, 0, 1, 1.0, 76},
    {"aenv_release", "Amp Env Release", S::AmpEnv, K::Continuous, 0, 1, 0.35, 72},
    {"aenv_vel", "Amp Envelope Velocity", S::AmpEnv, K::Switch, 0, 1, 0, 119},
    {"glide", "Glide Rate", S::Performance, K::Continuous, 0, 1, 0.0, 5},
    {"unison", "Unison", S::Performance, K::Switch, 0, 1, 0, -1},
    {"unison_voices", "Unison Voice Count", S::Performance, K::Stepped, 1, 10, 10, -1},
    {"unison_detune", "Unison Detune", S::Performance, K::Continuous, 0, 1, 0.1, -1},
    {"key_priority", "Key Priority (Low/High/Last)", S::Performance, K::Stepped, 0, 2, 2, -1},
    {"retrigger", "Envelope Retrigger (mono)", S::Performance, K::Switch, 0, 1, 1, -1},
    {"bend_range", "Pitch Bend Range (semitones)", S::Performance, K::Stepped, 0, 12, 2, -1},
    {"volume", "Program Volume", S::Performance, K::Continuous, 0, 1, 0.8, -1},
    {"vintage", "Vintage", S::Vintage, K::Continuous, 0, 1, 0.0, -1},
};

const ParamInfo kProgram[kNumProgramParams] = {
    {"layer_mode", "Layer Mode (Single/Split/Stack)", S::Program, K::Stepped, 0, 2, 0, -1},
    {"split_point", "Split Point (MIDI note)", S::Program, K::Stepped, 0, 127, 60, -1},
    {"split_upper_b", "Layer B Above Split", S::Program, K::Switch, 0, 1, 1, -1},
    {"layer_balance", "Layer A/B Balance", S::Program, K::Continuous, 0, 1, 0.5, -1},
};
}  // namespace

const ParamInfo& layerParamInfo(int id) { return kLayer[id]; }
const ParamInfo& programParamInfo(int id) { return kProgram[id]; }

int findLayerParam(const std::string& key) {
    for (int i = 0; i < kNumLayerParams; ++i)
        if (key == kLayer[i].key) return i;
    return -1;
}

int findProgramParam(const std::string& key) {
    for (int i = 0; i < kNumProgramParams; ++i)
        if (key == kProgram[i].key) return i;
    return -1;
}

Program::Program() {
    for (int i = 0; i < kNumLayerParams; ++i) layer[0][i] = layer[1][i] = kLayer[i].defaultValue;
    for (int i = 0; i < kNumProgramParams; ++i) settings[i] = kProgram[i].defaultValue;
}

Program Program::init() { return Program(); }

}  // namespace p10

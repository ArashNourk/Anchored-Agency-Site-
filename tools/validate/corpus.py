"""The validation corpus: tests, the features each produces, tolerances, and
the calibration fit groups. `python3 -m tools.validate corpus` exports it as
JSON for other tooling.

Tolerances are the initial acceptance targets (docs/VALIDATION_FRAMEWORK.md).
A feature passes when |emulator - hardware mean| <= max(tolerance,
2 x hardware take-to-take standard deviation): the emulator is never asked to
be more repeatable than the instrument itself.
"""
import os

ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))
SCN = os.path.join(ROOT, "validation", "corpus", "scenarios")


def _segs(starts, dur):
    return [[round(a, 4), dur] for a in starts]


TESTS = [
    {"id": "C01", "name": "Level reference", "scenario": "C01_level_ref.scn", "role": "gain_reference",
     "extract": [{"id": "ref", "type": "level", "segments": [[0.2, 0.7]]}],
     "tolerance": {"ref.level_db": {"abs": 0.5}}},
    {"id": "C02", "name": "Osc A pitch and spectrum across the range", "scenario": "C02_osc_pitch.scn",
     "extract": [
         {"id": "pitch", "type": "pitch", "transpose": 12, "notes": [36, 48, 60, 72, 84, 96],
          "segments": _segs([i + 0.2 for i in range(6)], 0.6)},
         {"id": "harm_c5", "type": "harmonics", "note": 60, "transpose": 12, "segment": [2.2, 0.6], "count": 13},
         {"id": "harm_c7", "type": "harmonics", "note": 84, "transpose": 12, "segment": [4.2, 0.6], "count": 9}],
     "tolerance": {"pitch.cents": {"abs": 0.5}, "harm_c5.harm_db": {"abs": 0.75}, "harm_c7.harm_db": {"abs": 1.0}},
     "informs": ["osc.resetTimeSec", "osc.sawCurvature", "osc.outputBandwidthHz"]},
    {"id": "C03", "name": "Self-oscillation frequency vs cutoff, both filters", "scenario": "C03_filter_selfosc.scn",
     "extract": [{"id": "osc", "type": "selfosc", "segments": _segs([i * 0.7 + 0.3 for i in range(12)], 0.35)}],
     "tolerance": {"osc.freq_semis": {"abs": 0.1}, "osc.level_db": {"abs": 1.0}},
     "informs": ["filter.cutoffMinHz", "filter.cutoffMaxHz", "filter.cem.cutoffScale"]},
    {"id": "C04", "name": "Resonance onset and full-resonance level", "scenario": "C04_resonance_onset.scn",
     "extract": [{"id": "res", "type": "onset_sweep", "threshold_db": -50,
                  "ramps": [{"t0": 0.5, "t1": 6.5, "v0": 0.70, "v1": 1.0, "hold": [7.0, 0.9]},
                            {"t0": 8.5, "t1": 14.5, "v0": 0.70, "v1": 1.0, "hold": [15.0, 0.9]}]}],
     "tolerance": {"res.onset_knob": {"abs": 0.01}, "res.level_db": {"abs": 1.0}},
     "informs": ["filter.ssm.resOnsetKnob", "filter.ssm.kMax", "filter.cem.resOnsetKnob", "filter.cem.kMax"]},
    {"id": "C05", "name": "Amp envelope timing", "scenario": "C05_envelopes.scn",
     "extract": [{"id": "atk", "type": "env_attack", "onsets": [0, 3, 6], "dur": 2.4},
                 {"id": "dec", "type": "env_decay", "onsets": [9, 11.5], "dur": 2.3},
                 {"id": "rel", "type": "env_release", "offsets": [14.6, 17.6], "dur": 2.3}],
     "tolerance": {"atk.ln_rise": {"abs": 0.05}, "dec.ln_tau": {"abs": 0.05}, "rel.ln_tau": {"abs": 0.05}},
     "informs": ["env.attackMaxSec", "env.decayMaxSec", "env.releaseMaxSec", "env.timeTaper"]},
    {"id": "C06", "name": "Poly Mod (Osc B -> Freq A) depth", "scenario": "C06_polymod_depth.scn",
     "extract": [{"id": "pm", "type": "mod", "note": 60, "span": 16, "segments": _segs([0.3, 2.8, 5.3], 2.1)}],
     "tolerance": {"pm.pp_semis": {"rel": 0.03}, "pm.ln_rate": {"abs": 0.03}},
     "informs": ["pm.freqARangeSemis", "pm.amountTaper", "osc.b.loFreqShiftSemis"]},
    {"id": "C07", "name": "LFO rate and depth", "scenario": "C07_lfo.scn",
     "extract": [{"id": "lfo", "type": "mod", "note": 60, "span": 8, "segments": _segs([0.2, 3.2, 6.2], 2.7)}],
     "tolerance": {"lfo.pp_semis": {"rel": 0.05}, "lfo.ln_rate": {"abs": 0.02}},
     "informs": ["lfo.minHz", "lfo.maxHz", "mod.freqRangeSemis"]},
    {"id": "C08", "name": "Voice fingerprint (per-voice identity)", "scenario": "C08_voice_fingerprint.scn",
     "extract": [{"id": "fp", "type": "fingerprint", "voices": 10, "dur": 0.44,
                  "onsets": [round(i * 0.45, 3) for i in range(30)], "notes": [60 + i % 10 for i in range(30)]},
                 {"id": "anchor", "type": "fingerprint", "voices": 1, "dur": 0.44, "onsets": [13.7], "notes": [60]}],
     "tolerance": {"fp.cents": {"abs": 0.3}, "fp.level_db": {"abs": 0.2}, "fp.ln_decay": {"abs": 0.03},
                   "anchor.cents": {"abs": 0.3}, "anchor.level_db": {"abs": 0.2}, "anchor.ln_decay": {"abs": 0.03}},
     "cyclic": {"features": ["fp.cents", "fp.level_db", "fp.ln_decay"], "anchor": "anchor"},
     "informs": ["unit.v1..v10 (tuneCentsA, vcaGainDb, ampEnvTimeLn)"]},
    {"id": "C09", "name": "Sync sweep spectral trajectory", "scenario": "C09_sync_sweep.scn",
     "extract": [{"id": "sweep", "type": "centroid", "start": 0.05, "dur": 2.0, "hop": 0.1}],
     "tolerance": {"sweep.log2_centroid": {"abs": 0.04}}},
]

# Fit order matters: each group assumes the earlier ones are settled.
FIT_GROUPS = [
    {"name": "unit_profile", "method": "voice_profile", "test": "C08"},
    {"name": "osc_hf", "tests": ["C02"], "features": ["C02.pitch.cents"],
     "params": [{"key": "osc.resetTimeSec", "lo": 1e-7, "hi": 3e-5, "log": True}]},
    {"name": "resonance", "tests": ["C04"], "features": ["C04.res.onset_knob", "C04.res.level_db"],
     "params": [{"key": "filter.ssm.resOnsetKnob", "lo": 0.7, "hi": 0.98},
                {"key": "filter.ssm.kMax", "lo": 4.05, "hi": 6.0},
                {"key": "filter.cem.resOnsetKnob", "lo": 0.7, "hi": 0.98},
                {"key": "filter.cem.kMax", "lo": 4.05, "hi": 6.0}]},
    {"name": "cutoff_map", "tests": ["C03"], "features": ["C03.osc.freq_semis"],
     "params": [{"key": "filter.cutoffMinHz", "lo": 3, "hi": 40, "log": True},
                {"key": "filter.cutoffMaxHz", "lo": 8000, "hi": 48000, "log": True},
                {"key": "filter.cem.cutoffScale", "lo": 0.85, "hi": 1.15},
                # voice 1 plays all single-voice tests; its own cutoff offset
                # (component tolerance) is not separable from C08 alone
                {"key": "unit.v1.cutoffSemis", "lo": -3.0, "hi": 3.0}]},
    {"name": "envelopes", "tests": ["C05"], "features": ["C05.atk.ln_rise", "C05.dec.ln_tau", "C05.rel.ln_tau"],
     "params": [{"key": "env.attackMaxSec", "lo": 1, "hi": 30, "log": True},
                {"key": "env.decayMaxSec", "lo": 2, "hi": 40, "log": True},
                {"key": "env.releaseMaxSec", "lo": 2, "hi": 40, "log": True},
                {"key": "env.timeTaper", "lo": 0.5, "hi": 2.0}]},
    {"name": "polymod", "tests": ["C06"], "features": ["C06.pm.pp_semis"],
     "params": [{"key": "pm.freqARangeSemis", "lo": 12, "hi": 96},
                {"key": "pm.amountTaper", "lo": 1.0, "hi": 3.5}]},
    {"name": "lfo", "tests": ["C07"], "features": ["C07.lfo.pp_semis", "C07.lfo.ln_rate"],
     "params": [{"key": "lfo.minHz", "lo": 0.005, "hi": 0.2, "log": True},
                {"key": "lfo.maxHz", "lo": 5, "hi": 80, "log": True},
                {"key": "mod.freqRangeSemis", "lo": 3, "hi": 36}]},
]


def check():
    """Every fit-group feature must exist in its test's tolerance table."""
    for g in FIT_GROUPS:
        for f in g.get("features", []):
            tid, name = f.split(".", 1)
            if name not in test(tid)["tolerance"]:
                raise ValueError(f"fit group {g['name']}: unknown feature {f}")


def test(tid):
    return next(t for t in TESTS if t["id"] == tid)


def scenario_path(t):
    return os.path.join(SCN, t["scenario"])


check()

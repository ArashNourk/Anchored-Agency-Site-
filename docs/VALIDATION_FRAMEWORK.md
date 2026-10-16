# Hardware validation and calibration framework

ابوالفضل is meant to be a computational model of a specific instrument, not
something that just sounds vintage. This framework is how that claim gets
tested and earned. It turns recordings of a real Prophet-10 Rev4 into fitted
model constants and a residual report with explicit tolerances:

```
hardware Prophet ─► stimulus/response capture ─► alignment + level normalisation
      ─► feature extraction ─► calibration fit (ordered groups) ─► emulator
      ─► residual report (per feature, per test, per unit) ─► next model change
```

Code: `tools/validate/` (Python 3 + numpy; capture also needs `python-rtmidi`
and `sounddevice`). Entry point: `python3 -m tools.validate <command>`.

## 1. The claim this produces

The target statement, per reference unit and corpus version, is:

> Across reference units *U1…Un* (firmware *x*), on validation corpus *v*,
> *k* of *N* measured features are within tolerance, where each tolerance is
> the larger of the published acceptance target and twice the instrument's
> own take-to-take spread.

The report prints this statement with its numbers. A model change is
accepted only if it does not reduce the count on any reference unit.

## 2. Test corpus

Defined in `tools/validate/corpus.py` (export with `corpus`). The scenarios
are in `validation/corpus/scenarios/`. Each test fixes the stimulus (scenario),
the analysis windows, the extracted features, the tolerances, and the
calibration constants it informs.

| Test | Behaviour | Features | Tolerance (initial) | Informs |
|---|---|---|---|---|
| C01 | Level reference | output level | 0.5 dB | session gain (capture chain) |
| C02 | Osc A pitch and spectrum, C3–C8 | pitch error per octave; harmonics 2–13 at C5, 2–9 at C7 | 0.5 cent; 0.75 / 1.0 dB | reset time / HF tracking, saw shape, output bandwidth |
| C03 | Self-oscillation vs cutoff knob, both filters | frequency, level at 6 knob positions × 2 models | 10 cents; 1 dB | cutoff map, CEM scale |
| C04 | Resonance onset, both filters | knob position where self-oscillation appears during a slow resonance sweep; level at full resonance | 0.01 knob; 1 dB | resonance onset, maximum loop gain |
| C05 | Amp envelope | attack time at 3 knob positions, decay and release τ at 2 each | 5 % (ln) | envelope ranges and taper |
| C06 | Poly Mod Osc B → Freq A depth | peak-to-peak pitch deviation at 3 amounts, modulation rate | 3 %; 3 % | Poly Mod range and taper, Lo Freq shift |
| C07 | LFO → Freq A | depth and rate at 3 rate settings | 5 %; 2 % | LFO range, wheel/initial depth range |
| C08 | Voice fingerprint | per-voice pitch, level and decay time over 30 strikes; per-voice self-oscillation (cutoff offset); per-voice Osc B pitch | 0.3 cent; 0.2 dB; 3 %; 5 cents; 0.3 cent | unit voice profile |
| C09 | Sync sweep | spectral-centroid trajectory | 3 % | (validation only) |

The corpus grows with the model. The next additions in priority order are
filter noise-transfer curves (cutoff/resonance/drive surfaces), PWM and pulse
extremes, Poly Mod sidebands into the filter, Vintage positions (spread
distributions), glide trajectories, unison, voice stealing transients, and
velocity/aftertouch response.

### Design rules

* **One voice for single-voice tests.** C02–C07 and C09 run in unison with one
  voice, so every take hits the same physical voice (assumption A-VAL-06:
  one-voice unison uses voice 1). C08 ends with a one-voice unison note
  (the *anchor*) that identifies that voice among the ten.
* **Voice identity without knowing the voice numbers.** The hardware's
  assignment starting point is unknown, so C08's per-voice vectors are rotated
  to put the anchor voice first. The emulator's strike order starts on the
  same voice by construction.
* **Timing comes from the stimulus, not from the model.** The capture tool
  schedules the MIDI against the recording clock and stores the scheduled
  start (`take<N>.json`, `preroll_s`) beside each take. The session's single
  audio/MIDI path latency is measured once, from the sharp onset of the C01
  note. Every take is then placed on the scenario's time axis without
  reference to the emulator. Alignment by cross-correlating against an
  uncalibrated emulator was tried first and rejected: it was biased by up to
  94 ms exactly where the model was wrong (envelope shapes). It remains only
  as a fallback for recordings without timing data.
* **Level normalisation.** The capture chain's gain is unknown, so the
  session gain is the C01 level difference between emulator and hardware. It
  is recomputed for every calibration being evaluated, because a fitted
  voice-1 level changes the emulator's C01 too. C01 is therefore a reference,
  not a pass/fail test.
* **Smooth features for fitting.** Features near a threshold (oscillation
  onset) are measured as the position of the threshold (knob value at onset),
  never as levels that jump across it, because a jump gives the optimiser
  nothing to follow.
* **Repeatability sets the floor.** Every test is captured several times
  (default 3). Tolerance per feature = max(target, 2 × take-to-take SD).

## 3. Capture

```
python3 -m tools.validate map-template        # writes validation/hw_map.json
# fill in NRPN numbers and value ranges from the Prophet-10 Rev4 documentation
python3 -m tools.validate capture --unit P10-0001 --session 2026-11-02a \
        --firmware 1.x --temperature-c 22 --warmup-min 45 --takes 3
```

Session protocol: 45 minutes warm-up, tune routine, room temperature logged,
fixed interface gain, 48 kHz. Captures go to
`validation/captures/<unit>/<session>/<test>/take<N>.wav`, with
`session.json` holding the metadata. Raw captures are not committed. The
session's `features.json` and `report.md` are.

The NRPN map is deliberately shipped empty. Numbers and ranges have to come
from the instrument's documentation, not be guessed. Tests that change
parameters during a take need the map. With an incomplete map, the tool runs
only the static tests and prints a setup sheet for the operator.

## 4. Measurement and fitting

```
python3 -m tools.validate measure <session>    # -> <session>/features.json
python3 -m tools.validate fit <session>        # -> <session>/fitted.cal
python3 -m tools.validate report <session> --calibration <session>/fitted.cal
```

Fitting runs in groups, in dependency order. Each group's constants are fitted
with a bounded Nelder–Mead search, log-scaled where a constant spans decades.
The emulator renders run in the loop. The objective is the sum of squared
tolerance-normalised residuals of that group's features.

1. **unit_profile**: per-voice Osc A and Osc B tuning, VCA gain, envelope-time and cutoff
   offsets (`unit.vN.*`, A-UNIT-01), solved iteratively from C08. Mean level,
   mean envelope time and mean cutoff are left to the session gain, the
   envelope fit and the cutoff-map fit.
2. **osc_hf**: 3340 reset time (high-frequency tracking) from C02.
3. **resonance**: onset knob position and maximum loop gain, both models, from C04.
4. **cutoff_map**: cutoff range and Rev3 scale, from C03.
5. **envelopes**: attack/decay/release ranges and the shared taper, from C05.
6. **polymod**: Poly Mod range and amount taper, from C06.
7. **lfo**: LFO range and modulation depth range, from C07.

The groups are coupled. The HF tracking constant moves every voice's pitch,
and the envelope ranges move every voice's decay. So the whole sequence runs
twice, the second pass starting from the first, and each search restarts from
its best point to escape shallow minima. Constants that the single-voice tests
can only see through voice 1 (the HF tracking constant) absorb that voice's
own tolerance, so they are effective values for the measured unit rather than
"population" values.

`fitted.cal` loads straight into the engine: `calibration <file>` in a
scenario, or `CalibrationModel::loadFromFile`.

### Identifiability lessons built into the corpus

* A single voice's cutoff offset is indistinguishable from scaling the whole
  cutoff range: an offset of 0.8 semitones fitted equally well as a 5 %
  range change. C08 therefore measures every voice's cutoff, so each voice's
  offset is pinned relative to the voice average and the global map becomes
  identifiable.
* Features must be invariant to the capture chain. The first sync-sweep
  feature (magnitude-weighted spectral centroid) read a recording with a
  −85 dBFS noise floor as 3 % brighter while the underlying sound differed by
  0.5 %. It is now power-weighted over the bins within 60 dB of the peak. The
  self-test adds noise, gain and latency to its synthetic captures for
  exactly this reason.
* C08 also measures every voice's Osc B pitch. A sync timbre depends on the
  A/B pitch ratio of the voice, so per-voice Osc B tuning belongs in the unit
  profile.
* The same applies, more weakly, to the 3340 high-frequency tracking
  constant, which is seen only through voice 1. It is reported as an
  effective value for that voice.

## 5. Self-test

`python3 -m tools.validate selftest` checks the whole chain without hardware.
A hidden calibration with different constants, a different instrument serial
(so different voice personalities) and random per-voice offsets stands in for
the reference unit. Its renders get a random pre-roll, a −3 dB capture-chain
gain and noise. The framework then measures, fits from the defaults and
reports. The self-test passes when every feature is within tolerance after
fitting and every hidden constant is recovered within 5 %.

What it proves: each fit group is identifiable from its tests, and alignment,
gain normalisation, voice-rotation matching and the optimiser work. What it
cannot prove: that the model's structure matches the real instrument. If the
hardware residuals stay large after fitting, the topology is wrong somewhere,
not just the constants, and the report shows which test and feature.

Latest result ([validation/selftest_report.md](../validation/selftest_report.md)):

* timing: the hidden 7.3 ms latency recovered, every take placed within 0.04 ms
  of its true position;
* features within tolerance: 29 of 146 with default constants, 146 of 146
  after fitting;
* constants recovered within 2 % (resonance, cutoff map, envelopes, Poly Mod,
  LFO), apart from the 3340 HF tracking constant: +27 % against the global
  truth, because it absorbs voice 1's own hidden tolerance. Its features
  still pass, so it is an effective value for that unit;
* runtime about 35 minutes on 4 cores (two passes).

## 6. Engineering obligations this creates

* Every engine change re-runs `report` on every stored reference session.
  No feature may regress.
* A new behaviour enters the engine together with its corpus test.
* A constant leaves the "estimate" state in ASSUMPTIONS.md only when a
  report shows it fitted on at least one reference unit, and on two units
  when per-unit variation is plausible.

# Assumption register

Every behaviour of the Prophet-10 Rev4 that is not established by public
documentation is isolated in `engine/model/Calibration.h` under an assumption
ID. **None of the default values below is a measured value.** They are
engineering estimates chosen to be plausible and internally consistent, so that
the engine is usable before calibration. They must be replaced by measurements
from a real Prophet-10 Rev4 using the procedures given here and in
[VALIDATION.md](VALIDATION.md).

Every value can be overridden without recompiling, from a `key = value` file
(`p10render --dump-calibration` prints all keys with their IDs; scenarios load
one with `calibration <file>`).

Columns: **ID**, the assumed behaviour and default, **Why this default**, and
**How to measure**: the scenario in `validation/scenarios/` and the
`tools/analysis/p10analyze.py` command that determines the real value.

## Oscillators

| ID | Assumption (default) | Why this default | How to measure |
|---|---|---|---|
| A-OSC-01 | Osc A/B Frequency knob spans 48 semitones centred on the played key | Classic Prophet range is 4 octaves | Scenario 01 with the frequency knob at end stops; `pitch` → semitone offset at each stop |
| A-OSC-02 | Osc A and Osc B Frequency knobs are quantised to semitones | Prophet-5 practice | Slow knob sweep on hardware with `pitchtrack`: staircase means quantised, ramp means continuous |
| A-OSC-03 | Osc B Fine spans ±1 semitone | Unknown | Fine at both ends, `pitch` |
| A-OSC-04 | Low Freq shifts Osc B down 84 semitones | Gives ~0.1–20 Hz modulation range | Lo Freq on, Osc B → Freq A Poly Mod, `pitchtrack` modulation rate at several Osc B knob positions |
| A-OSC-05 | With keyboard tracking off, Osc B plays as if key 60 were held | Unknown | Kbd off, `pitch` on Osc B alone |
| A-OSC-06 | Saw, pulse and triangle have equal peak level (±1) | Unknown output scaling | Each waveform alone at the same mixer level, compare RMS/peak with `spectrum` fundamental level |
| A-OSC-07 | Several selected waveforms are **summed** (0 = sum, 1 = diode-OR max) | Resistor summing is the common topology | Saw+pulse together: compare the waveform shape and harmonic levels to saw-only and pulse-only captures. A sum predicts levels exactly; a max-combiner does not |
| A-OSC-08 | PW knob maps linearly to duty 0 … 1 (silent at both ends, square at centre) | Prophet-5 behaviour | Scenario 02, `duty` at each knob position; note where the output goes silent |
| A-OSC-09 | CEM3340 reset (retrace) time 1 µs | Datasheet-order figure | Scenario 01 upper notes: the flattening of the top octaves (with HF trim residual) |
| A-OSC-10 | Saw ramp curvature 0.015 | Small bow seen on real ramps | Waveform capture of saw alone at a low note; fit the parabola term |
| A-OSC-12 | Oscillator output bandwidth 60 kHz | Op-amp buffer estimate | Highest harmonics in scenario 01 at C7, compared with a pure BLEP saw |
| A-OSC-13 | Pulse is high during the *end* of the ramp (comparator ramp > PW) | CEM3340 pulse derivation | Saw+pulse capture: the step sizes at the reset reveal the relative phase |

## Mixer, noise

| ID | Assumption | How to measure |
|---|---|---|
| A-MIX-01 | Mixer level gain = knob^1.5 | Osc A level at 0.25/0.5/0.75/1.0, fundamental level (`spectrum`) |
| A-MIX-02 | Noise gain 0.7 relative to an oscillator | Noise level 1.0 vs Osc A level 1.0, RMS with the filter open |
| A-MIX-03/04 | Mixer saturation: tanh with drive 0.55 and asymmetry 0.06 | Two oscillators at full level (saw+pulse), open filter: even/odd harmonic and intermodulation levels compared with a single oscillator; repeat at level 0.5 |
| A-MIX-05 | AC coupling at 4 Hz | Narrow pulse DC shift settling time after note-on |

## Filters

| ID | Assumption | How to measure |
|---|---|---|
| A-FLT-01 | Cutoff knob is exponential from 10 Hz to 24 kHz | Scenario 05: self-oscillation frequency at knob 0.3…0.8 → CSV `knob,hz` → `fit-cutoff`. Do it for both filter models; differences go into `filter.ssm.cutoffScale` / `filter.cem.cutoffScale` |
| A-FLT-02 | Env Amount at full = +120 semitones | Self-oscillating filter, Env Amount at 0.25/0.5, sustain 1: frequency change |
| A-FLT-03/04 | Keyboard tracking reference key 60, HALF = 0.5 | Self-oscillation at keys 36/60/84 with OFF/HALF/FULL |
| A-FLT-05 | Internal noise floor 1e-5 (sets oscillation start-up) | Time from note-on to full self-oscillation amplitude, no input |
| A-FLT-1x | SSM2040 model constants (input gain, resonance taper, max loop gain, passband compensation 0.55, feedback limiter, HF resonance loss) | Scenario 04 (noise transfer at res 0 and 0.5), scenario 05 (oscillation onset knob, oscillation level vs cutoff), passband loss: saw at C2, res 0 vs 0.75, fundamental level |
| A-FLT-2x | CEM3320 model constants, passband compensation 0 (full Rev3-style bass loss) | Same as above with Rev3 selected |

Topology (OTA difference-nonlinearity cascade vs. current-mode ladder with
separate input/state nonlinearities) is a modelling decision based on the
chips' published structure, not a fitted value. If measured frequency
responses cannot be matched by the constants alone, the topology is wrong and
must be revised.

## Envelopes and VCA

| ID | Assumption | How to measure |
|---|---|---|
| A-ENV-01..03 | Attack 1 ms…10 s, decay/release 2 ms…15 s, exponential taper | Scenario 06 at ≥5 knob positions per stage: `envelope` (attack-decay) and `envelope --stage release` → CSV `knob,seconds` → `fit-env --stage attack|decay|release` |
| A-ENV-04 | Time taper exponent 1.0 | Output of `fit-env` |
| A-ENV-05 | Attack is an RC charge toward 1.3× the trip level | Attack curvature: fit the attack segment of a slow attack (knob 0.7) to `1 - exp(-t/τ)` scaled to the target |
| A-ENV-06 | Decay/release knob time = 4.6 τ (time to −40 dB) | Output of `envelope` (decay_tau) vs knob |
| A-ENV-07..09 | Velocity scales filter-env amount (depth 1.0) and amp level (depth 0.7), linear curve | Scenario 13 velocities 20/50/80/110/127: peak level and filter brightness per note |
| A-VCA-01 | Control feed-through 0.0015 (VCA "thump") | Silent oscillators, fast attack/release, measure the low-frequency pulse at the output |
| A-VCA-02 | Soft VCA saturation 0.25 | High-resonance, full-level patch: compression of peaks vs. low-level linearity |

## Poly Mod

| ID | Assumption | How to measure |
|---|---|---|
| A-PM-01 | Full amount → Freq A ±48 semitones | Osc B (Lo Freq, slow, triangle) → Freq A at amounts 0.1…1.0, `pitchtrack` modulation depth → CSV `amount,semis` → `fit-polymod --dest freqA` |
| A-PM-02 | Full amount → PW A ±0.5 duty | Same with PW A destination, `duty` at the extremes |
| A-PM-03 | Full amount → filter ±96 semitones | Self-oscillating filter, Lo Freq Osc B → Filter, `pitchtrack` → `fit-polymod --dest filter` |
| A-PM-04 | Filter Env amount is unipolar | Hardware knob centre: check whether the modulation is zero at the centre (bipolar) or at the end stop |
| A-PM-05 | Osc B Poly Mod source is the sum of its selected waveforms at ±1 | Compare depth with saw vs triangle vs pulse selected |
| A-PM-06 | Amount taper knob^2 | Output of `fit-polymod` |

## LFO, wheel, aftertouch

| ID | Assumption | How to measure |
|---|---|---|
| A-LFO-01 | LFO 0.03 Hz…25 Hz, exponential | Scenario 08: `pitchtrack`, period of the modulation at several knob positions |
| A-LFO-02 | Multiple waveforms are summed at unity gain | Scenario 08 last section (saw+tri) depth vs single waveform |
| A-LFO-03 | Square edges slewed 0.8 ms | Square LFO → Freq A, edge time in `pitchtrack --hop 1` |
| A-LFO-04 | Wheel-mod noise source = white noise low-passed at 80 Hz | Source Mix fully to noise, → Filter on a self-oscillating filter; spectrum of the resulting frequency modulation |
| A-LFO-05 | Initial amount and wheel amount add (0) rather than max (1) | Initial 0.5 + wheel half: depth vs either alone |
| A-LFO-06/07 | Full depth: ±12 semitones pitch, ±0.45 PW, ±60 semitones filter; amount taper ^2 | Depth at amounts 0.25/0.5/1.0 per destination |
| A-AT-01 | Aftertouch → filter: +36 semitones at full pressure | Scenario 13 pressure steps on a self-oscillating filter |
| A-AT-02 | Aftertouch → LFO amount adds to the wheel amount | Scenario 13 second section |

## Glide, unison

| ID | Assumption | How to measure |
|---|---|---|
| A-GLD-01/02 | Glide is an RC lag on the pitch CV (exponential approach, constant time regardless of interval), τ 0.5 ms…3 s | Unison 1 voice, glide 0.25/0.5/0.75, intervals of 5 and 24 semitones: `pitchtrack --hop 2`. Exponential = log of remaining distance is linear in time; constant-time = τ independent of the interval |
| A-UNI-01 | Unison detune at full = 50 cents total spread, linear across voices | Scenario 11: beat frequencies / per-voice pitch (record individual voices by soloing if possible) |

## Vintage

| ID | Assumption | How to measure |
|---|---|---|
| A-VIN-01..17 | Standard deviations of each component tolerance at the four Vintage anchors (Rev4 calibrated, Rev3-, Rev2-, Rev1-like), linear between anchors; drift time constants 25 s per voice, 140 s global; fast jitter 40 Hz bandwidth | Scenario 09 at each Vintage position on a warmed-up instrument (≥30 min). `noteseq` gives per-voice pitch, level and brightness spread; repeat the scenario 3× over 10 minutes to separate static spread (repeatable per voice) from drift (changes between runs). For envelope-time spread use scenario 06 notes landing on different voices |

On the Rev4 the Vintage control is implemented by the instrument's firmware;
whether it varies only pitch or also filter, envelope and VCA parameters (and
by how much) must be measured. The model applies it to all of them, as
required by the specification. A component found not to vary on the hardware
gets zero sigmas.

## Calibration (tuning) model

| ID | Assumption | How to measure |
|---|---|---|
| A-CAL-01 | Untrimmed spread: ±40 cents offset, ±25 cents/octave scale, ±6 semitones / ±5 % cutoff | Not observable on a tuned instrument; affects only how far aging can move. Keep unless a de-tuned measurement exists |
| A-CAL-02 | Correction resolution 0.12 cents | Residual spread right after a tune at Vintage minimum (scenario 09 at Vintage 0, immediately after tuning) |
| A-CAL-03 | Tune measurement noise 0.05 cents | Same, over several tune runs |
| A-CAL-04 | Aging 0.4 cents per √hour random walk | Pitch spread vs time since last tune (hours/days) |
| A-CAL-05 | HF trim removes 90 % of the reset-time error | Top-octave flattening in scenario 01 at Vintage 0 |

## Voice allocation

| ID | Assumption | How to measure |
|---|---|---|
| A-VAL-01 | Free voices are assigned by rotation | Scenario 09/10 on hardware with Vintage at maximum: each voice has a recognisable pitch/brightness signature (`noteseq`), so the sequence of signatures reveals the assignment order |
| A-VAL-02 | A key returns to the voice that last played it | Scenario 10: the same key struck repeatedly; one signature means reuse, rotating signatures mean no reuse |
| A-VAL-03 | Stealing takes the oldest note | Scenario 12: which held note disappears when the 11th and 12th notes arrive |
| A-VAL-06 | One-voice unison always uses the same physical voice (voice 1) | Corpus C08 anchor: the anchor note's pitch/level/decay signature must match one fingerprint voice consistently across sessions |
| A-VAL-04/05 | Stack and split use fixed 5 + 5 voice halves | Hold 6+ notes in one half of a split; if the 6th note sounds without stealing, the pool is dynamic |

## Unit profile

| ID | Assumption | How to measure |
|---|---|---|
| A-UNIT-01 | A reference unit's persistent per-voice deviations can be represented as per-voice tuning, VCA gain and envelope-time offsets on top of the Vintage model | Fitted from corpus test C08 (`python3 -m tools.validate fit`); the residuals of C08 across sessions show whether the representation is sufficient |

## Mapping and output

| ID | Assumption | How to measure |
|---|---|---|
| A-MAP-01 | A stored 0…1 knob position maps linearly to the hardware's displayed parameter range | Record the hardware display values used for every capture; any non-linear relation shows up as a systematic error in the fits above |
| A-OUT-01/02 | Output gain 0.25, output AC coupling 3 Hz | Absolute level calibration against the hardware line output (reference: single saw, filter open, all levels full) |

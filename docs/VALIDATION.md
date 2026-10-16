# Reference validation against a Prophet-10 Rev4

> The automated pipeline (capture, alignment, feature extraction, calibration
> fitting, residual reports) is described in
> [VALIDATION_FRAMEWORK.md](VALIDATION_FRAMEWORK.md). This document is the
> broader listening and comparison procedure around it.

Accuracy is judged by measurement against a real Prophet-10 Rev4, not by
whether the result "sounds analog". This document is the procedure. It has
**not yet been carried out**: no hardware recordings exist in this repository,
and every constant in the model is still an unmeasured estimate (see
[ASSUMPTIONS.md](ASSUMPTIONS.md)).

## 1. Capture setup

* Instrument warmed up for at least 30 minutes, then the tune routine run.
  Note the room temperature.
* Line output (left/mono) → audio interface at 48 kHz / 24-bit. No processing,
  fixed interface gain, the same gain for every capture. Record a 1 kHz
  reference tone at the start of each session to tie levels together.
* MIDI from the computer to the instrument on a fixed channel. Local control
  stays on; the panel is set by hand from the setup sheet.
* Each scenario starts from the instrument's basic (init) program, with
  Vintage at its minimum unless the scenario sets it.

## 2. Per-scenario procedure

For every file in `validation/scenarios/`:

1. `tools/analysis/scn2midi.py <scenario> hw.mid > sheet.txt`, which gives the
   MIDI performance and the panel setup sheet (parameter changes with their
   times).
2. Set the panel from the sheet. Write down the value the hardware *displays*
   for each knob next to the sheet entry (assumption A-MAP-01).
3. Play `hw.mid` into the instrument while recording → `hw.wav`. Apply timed
   parameter changes by hand where the sheet lists them, or split the scenario
   into separate captures, one per panel state.
4. Render the software side: `build/p10render <scenario> sw.wav`.
5. Measure both with `tools/analysis/p10analyze.py` (commands below), using
   identical time windows.
6. Put the hardware numbers into the fitting commands (`fit-env`,
   `fit-cutoff`, `fit-polymod`) or straight into a calibration file. Re-render
   with `calibration <file>` added to the scenario and repeat until the
   acceptance targets are met.

## 3. Measurements

| # | Behaviour | Scenario | Measurement | Fits / IDs | Acceptance target (initial) |
|---|---|---|---|---|---|
| 1 | Oscillator spectra across the keyboard | 01 | `spectrum` per note, `compare --what spectrum` | A-OSC-06/09/10/12, A-MIX | Harmonics 1–20 within ±1 dB, pitch within ±1 cent at Vintage 0 |
| 2 | Pulse width | 02 | `duty` per section, harmonic nulls via `spectrum` | A-OSC-08/13 | Duty within ±0.005; silence points at the same knob positions |
| 3 | Sync sweeps | 03 | Spectrogram / `compare --what spectrum` at fixed times | A-PM-01, A-ENV | Sweep trajectory (dominant formant frequency vs time) within ±3 % |
| 4 | Filter cutoff response | 04 | Noise transfer function = filtered PSD ÷ open PSD, per knob position | A-FLT-01/1x/2x | −3 dB point within ±1/6 octave; slope and resonant peak height within ±1 dB |
| 5 | Resonance / self-oscillation | 05 | `selfosc --segments ...` | A-FLT-01, kMax, resOnsetKnob | Oscillation frequency ±10 cents, level ±1 dB, same onset knob position ±0.02 |
| 6 | Envelope timing and curvature | 06 | `envelope` and `envelope --stage release` | A-ENV-01..06 | Times within ±5 %; attack curvature residual < 2 % of full scale |
| 7 | Audio-rate Poly Mod | 07 | Sideband levels (`spectrum`), modulation depth | A-PM-01..06 | Sideband levels ±1.5 dB |
| 8 | LFO | 08 | `pitchtrack`: rate and depth per section | A-LFO-01..07 | Rate ±2 %, depth ±5 % |
| 9 | Vintage positions | 09 (×4) | `noteseq`: spread of pitch / level / brightness | A-VIN-01..17 | Spread statistics within ±20 % of the hardware at each position |
| 10 | Repeated notes / voice assignment | 10 | `noteseq` signatures per note at maximum Vintage | A-VAL-01/02 | Same assignment sequence |
| 11 | Unison | 11 | Beating rate, `spectrum` line width | A-UNI-01 | Detune spread ±10 % |
| 12 | Voice stealing | 12 | Which notes are cut, and the click/transient at the steal | A-VAL-03 | Same stolen notes |
| 13 | Velocity and aftertouch | 13 | Peak level and brightness per velocity; cutoff shift per pressure step | A-ENV-07..09, A-AT-01/02 | Level ±1 dB, cutoff ±1/12 octave |

These targets are the first iteration's goals; tighten them as the model
converges.

## 4. Listening tests

After the objective targets are met:

* ABX between hardware and software for the patch set in scenarios 01, 03, 07,
  09 and 11, level-matched, with experienced Prophet players as listeners.
* Players reproduce their own patches on both instruments and play them
  (sync leads, Poly Mod bells, brass with filter envelope, unison bass, pads at
  high Vintage), then report any behavioural difference in a structured form:
  control, range, expected vs. observed.

A behavioural difference reported by a player becomes a new measurement in
the table above before any model change is made.

## 5. What the in-repo tests do and do not show

`build/p10tests` checks internal properties: alias levels, that the filters
self-oscillate and differ, envelope timing against the model's own mapping,
voice allocation semantics, Vintage spread and persistence, calibration
recovery after aging, stability under extreme modulation, presets and MIDI.
Passing them shows the engine does what it was designed to do. It says
nothing about whether the design matches the hardware. Only this procedure can
show that.

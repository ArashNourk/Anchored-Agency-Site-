# Architecture

The sound engine is the product. It is plain C++17 with no UI, audio-API or
plugin-SDK dependency, so it can later be wrapped as a standalone application,
AU, VST3 or CLAP without changes.

```
MIDI bytes ──► MidiController ──► Synth ──► VoiceAllocator ──► Voice ×10 ──► Σ (oversampled) ──► Decimator ──► out
                (CC/NRPN/learn)      │            ▲                ▲
                                     │            │                │ per-sample frames
                                     ├──► LayerModulator A/B ──────┘ (knob smoothing, LFO,
                                     │      wheel/aftertouch, panel→physical mapping)
                                     ├──► VintageEngine ──► per-voice VoiceAnalogState (block rate)
                                     └──► CalibrationModel (all uncertain constants)
```

## Modules (spec §24)

| Concern | Files |
|---|---|
| Voice engine | `engine/voice/Voice.*` |
| Oscillator model | `engine/dsp/Oscillator.*` (CEM3340), `engine/dsp/Blep.*` |
| Mixer / nonlinear gain | in `Voice::render` (summing amp → tanh with asymmetry → AC coupling) |
| Filter models | `engine/dsp/Filters.*`: `Ssm2040Filter` (Rev1/2), `Cem3320Filter` (Rev3) |
| Envelope models | `engine/dsp/Envelope.h` |
| Poly Mod engine | in `Voice::render`, per voice, per oversampled sample |
| Global modulation | `engine/voice/LayerModulator.*` |
| Voice allocator | `engine/voice/VoiceAllocator.*` |
| Vintage / component variation | `engine/model/Vintage.*` |
| Calibration constants | `engine/model/Calibration.*` |
| Preset / state | `engine/model/Params.*`, `engine/preset/PresetStore.*` |
| MIDI / performance | `engine/midi/MidiController.*` |
| Audio I/O | `engine/io/AudioIO.h` (host interface), `engine/io/WavFile.*`, `apps/p10render.cpp` |
| UI layer | `engine/ui/PanelLayout.h` (control-surface description only; see DEVELOPMENT.md) |

## Signal path of one voice

All of it runs at the oversampled rate (default 4×, 8× for renders):

1. **Osc B** (CEM3340 model): one ramp core; saw, triangle (rectified ramp)
   and pulse (ramp vs. PW comparator) share its phase. Its reset time is
   exported for sync.
2. **Poly Mod source** = Filter-Env amount × filter envelope + Osc B amount ×
   Osc B output (the band-limited sum of its selected waveforms).
3. **Osc A**: frequency × 2^(PolyMod·range) (exponential FM, recomputed every
   oversampled sample), PW + Poly Mod, hard reset at Osc B's exact sub-sample
   reset time when Sync is on.
4. Each oscillator's output passes a one-pole (output-buffer bandwidth).
5. **Mixer**: A·level + B·level + shared white noise·level → tanh saturator
   with even-order asymmetry → AC coupling. Mixer levels use a calibratable
   taper.
6. **Filter**: SSM2040 or CEM3320 model, cutoff = exponential converter of
   knob + env·amount·velocity + keyboard tracking + wheel mod + aftertouch +
   Poly Mod (audio rate) + per-voice offsets and scale error. Integrator gains
   are prewarped; without audio-rate filter modulation they are computed at
   the output rate and interpolated.
7. **VCA**: linear control from the amp envelope × velocity × per-voice gain,
   soft output saturation, control feed-through.

Envelopes, glide, LFO and drift run at the output rate; envelopes are
interpolated across the oversampled sub-steps.

## Band-limiting

* Every discontinuity (ramp reset, comparator flip, sync reset) and every
  slope discontinuity (triangle corners, curvature at the reset) is inserted as
  a BLEP/BLAMP residual at its exact sub-sample time, including when PW and
  frequency are modulated at audio rate (comparator crossings are solved
  against the interpolated threshold).
* The BLEP kernel is defined in absolute frequency (flat to ~20 kHz, cut off
  at 38 kHz, empty above ~54 kHz). That leaves the nonlinear stages (mixer,
  filter cells, VCA) little ultrasonic energy to fold back. Measured with the
  complete nonlinear path: saw −99 dB, narrow pulse −93 dB, sync −95 dB worst
  alias; sync + audio-rate Poly Mod −77 dB at 4×, −94 dB at 8×.
* The decimator is a Kaiser FIR (≥115 dB stopband) whose stopband starts at
  fs − 20 kHz, so anything folding during decimation lands above 20 kHz.

## Filter solving

Both filters are four trapezoidal (TPT) integrator stages with global
feedback. Each nonlinearity is linearised by its secant gain at the current
operating point, the linear zero-delay loop is solved in closed form, and the
operating point is refined once more (two passes; three passes give the same
result, one pass shifts the SSM self-oscillation frequency, so two is used).

## Vintage and calibration

`VintageEngine` gives each physical voice:

* untrimmed circuit error (`raw`), the correction table written by `tune()`
  (`cal`, limited by correction resolution and measurement noise) and
  long-term `aging`;
* persistent personality draws `z`. Vintage scales them per component
  (tuning, scaling, HF tracking, PW, saw shape, cutoff, tracking, resonance,
  both envelope time scales, sustain, VCA gain, feed-through, filter stage
  mismatch), so a voice always deviates in the same direction;
* temporal drift: per-voice and instrument-wide (correlated) Ornstein–Uhlenbeck
  processes, plus fast component noise.

`tune()` measures each voice through the same pitch/cutoff error functions the
voice uses (two-point measurement, offset + scale) and writes corrections. It
does not remove the Vintage personality or the short-term drift.

## Bi-timbral structure

A program holds two layers (A, B) plus layer mode (single / split / stack),
split point, upper-layer assignment and balance. Each layer has its own
`LayerModulator` (its own LFO and smoothing); performance controllers (wheel,
bend, pressure, sustain) are shared. Voices are physical: when a voice moves
from layer A to B it keeps its components and state.

## Real-time considerations

`Synth::render` does no allocation, takes no locks and does no I/O (BLEP
tables are built and cached on construction). Parameter setters are cheap and
take effect through per-sample smoothing. A plugin wrapper should apply MIDI
and parameter events between `render` calls at sample-accurate split points,
as `p10render` does.

Measured cost on the development container (one core, worst case with ten
voices, audio-rate filter Poly Mod, 4×): about 0.5 s of CPU per second of
audio. The usual knob-static case is lower. Options before release: SIMD
across voices (the voice loop is uniform), 2× oversampling for
low-latency/live mode, and a cheaper `exp2`/`tan` for the audio-rate paths.

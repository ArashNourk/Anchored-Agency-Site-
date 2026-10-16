# ابوالفضل (Abolfazl)

A ten-voice analog synthesizer modelled on the Prophet-10 Rev4. In the code
the engine keeps its short internal prefix `p10` (namespace, binaries, file
names); ابوالفضل is the instrument's name.

A virtual analog synthesizer engine whose behavioural specification is the
Sequential Prophet-10 Rev4: ten persistent analog-modelled voices, each with
two CEM3340-style oscillators, a mixer with noise and nonlinear gain staging,
switchable SSM2040 (Rev1/2) and CEM3320 (Rev3) four-pole filters, RC-style
filter and amp envelopes, and the per-voice audio-rate Poly Mod section. Also
included: the Prophet LFO / wheel-mod architecture, Vintage component
variation with a tuning/calibration model, glide, unison, velocity,
aftertouch, and split/stack bi-timbral operation.

This is an independent project. It is not affiliated with or endorsed by
Sequential. It uses no Sequential artwork or trade dress and ships no
factory programs.

## Status

The sound engine, the measurement tooling and the validation procedure are
in place. **The model has not yet been compared with a real Prophet-10
Rev4.** Every constant that public documentation does not establish is
isolated, documented and calibratable, so the remaining work is measurement,
not re-engineering. See [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md).

## Play it

The synth runs in a browser: the C++ engine is compiled to WebAssembly and
runs on the audio thread (AudioWorklet), with the full panel, a 61-key
on-screen keyboard, computer-keyboard playing, mod/bend wheels, split/stack,
starter programs, 40 user slots, program text import/export and Web MIDI
input with MIDI learn.

* Prebuilt: serve `web/dist/` and open it, e.g.
  `python3 -m http.server -d web/dist 8000`, then visit
  http://localhost:8000 in a current desktop browser (tested in Chromium).
  Serve it rather than opening the file from disk: audio worklets and Web
  MIDI need `localhost` or https.
* Press **Power on**, then play with the on-screen keys, `Z`–`M` / `Q`–`U`,
  or a MIDI keyboard.
* Rebuild after engine changes: activate emsdk, then `web/build.sh`.

*Live 2×* oversampling is the default for low CPU; *High 4×* gives the
cleanest sound on a fast machine.

## Layout

```
engine/          C++17 DSP engine (no UI / audio-API / plugin dependencies)
  dsp/           BLEP tables, CEM3340 oscillator, filter models, envelopes, decimator
  model/         parameters, calibration constants, Vintage/tuning engine
  voice/         voice, layer modulation (LFO, wheel, aftertouch), allocator
  midi/ preset/  MIDI performance layer, program storage
  io/ ui/        WAV I/O and host interface; control-surface description
apps/p10render   offline scenario renderer
web/             browser app: wasm C API, UI, build script, prebuilt dist/
tests/           engine self-validation tests
validation/      scenario scripts for hardware/software comparison
tools/validate/  hardware validation framework: capture, features, fitting, reports
tools/analysis/  ad-hoc measurement tools (numpy)
docs/            architecture, assumption register, validation procedure, status
```

## Build

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/p10tests                                  # engine tests
./build/p10render validation/scenarios/07_polymod_audio.scn polymod.wav
./build/p10render --dump-calibration > my.cal     # every calibratable constant
python3 tools/analysis/p10analyze.py spectrum polymod.wav --start 0.5 --dur 0.8
```

## Documentation

* [Architecture](docs/ARCHITECTURE.md): module split, signal path, band-limiting, filter solving, Vintage.
* [Assumption register](docs/ASSUMPTIONS.md): every uncertain value, why it was chosen, how to measure it.
* [Validation framework](docs/VALIDATION_FRAMEWORK.md): automated hardware capture, feature extraction, calibration fitting and residual reports (`python3 -m tools.validate`).
* [Validation procedure](docs/VALIDATION.md): capture setup, the 13 comparisons, acceptance targets.
* [Development status](docs/DEVELOPMENT.md): progress against the development sequence, known gaps.

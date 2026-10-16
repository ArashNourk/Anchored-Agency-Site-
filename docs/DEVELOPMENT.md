# Development status

Progress against the specified development sequence (§27). "Done" means
implemented and covered by the in-repo tests. **No step has been validated
against reference hardware yet**, because none was available.

| Step | Status |
|---|---|
| 1. Single voice: Osc A, Osc B, mixer, noise, both filter models, envelopes, VCA | Done |
| 2. Audio-rate Poly Mod | Done (per voice, per oversampled sample, Filter Env and Osc B → Freq A / PW A / Filter) |
| 3. Validate the single voice against hardware | **Not done.** Procedure, scenarios and analysis tools are ready (VALIDATION.md) |
| 4. Ten persistent voices, voice allocation | Done (rotation, same-key reuse, oldest-note stealing, all calibratable) |
| 5. Vintage / component variation | Done (persistent personalities, drift, tuning/calibration model) |
| 6. LFO, wheel mod, glide, unison, velocity, aftertouch | Done |
| 7. Split / stack bi-timbrality | Done |
| 8. Final UI and preset system | Preset system done (engine-side). **UI deliberately not built**: the spec requires the engine to be validated first. `engine/ui/PanelLayout.h` defines the control surface (sections in panel order, one control per function) for the future UI |

## Known gaps and open questions

* **All model constants are estimates** (see ASSUMPTIONS.md). Topology
  choices (OTA cascade vs. current-mode ladder, RC envelopes, RC glide,
  summed waveform selection) are informed by the chips' structure and also
  need to be confirmed by measurement.
* **Panel details that need the hardware or its manual to confirm**:
  semitone quantisation of the frequency knobs, the exact aftertouch and
  velocity routing options of the Rev4, the available key-priority modes,
  the Prophet-10 voice split in stack/split modes, and whether Vintage
  affects anything beyond the oscillators on the real instrument. Each is
  behind a calibration switch or constant.
* **Release/hold footswitch, arpeggiator, clock sync, poly-chain, global
  tuning tables and the Rev4's per-program pan/spread** are not implemented.
  Neither is the hardware's own MIDI CC/NRPN map: the engine uses its own
  CC map plus NRPN for every parameter. The SysEx translation layer is an
  unimplemented interface (`PatchTranslator`).
* **Real-time host**: there is an offline renderer and a host-agnostic API,
  but no live audio-device backend or plugin wrapper yet.
* **CPU**: about 50 % of one core at 4× for ten fully active voices in the
  worst case. Needs SIMD/optimisation before a live release (see
  ARCHITECTURE.md).

## Build and test

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/p10tests
./build/p10render validation/scenarios/03_sync_sweep.scn sync.wav
```

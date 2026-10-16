# Validation framework self-test

End-to-end self-test with synthetic hardware.

A hidden "truth" calibration (different constants AND a different instrument
serial, i.e. different voice personalities) plays the role of the reference
Prophet. Its renders get an unknown pre-roll, a capture-chain gain and noise,
like real captures. The framework then measures, fits from the defaults and
reports. Passing shows that every fit group is identifiable from its tests and
that alignment, gain normalisation, voice-rotation matching and fitting work.
It does not show anything about the real instrument.

## Parameter recovery

| Parameter | Default | Truth | Recovered | Error |
|---|---|---|---|---|
| osc.resetTimeSec (absorbs voice-1 personality) | 1e-06 | 6e-06 | 7.6291e-06 | +27.15 % |
| filter.ssm.resOnsetKnob | 0.86 | 0.9 | 0.90866 | +0.96 % |
| filter.ssm.kMax | 4.6 | 4.9 | 4.845 | -1.12 % |
| filter.cem.resOnsetKnob | 0.9 | 0.86 | 0.87253 | +1.46 % |
| filter.cem.kMax | 4.4 | 4.6 | 4.544 | -1.22 % |
| filter.cutoffMinHz | 10 | 14 | 13.881 | -0.85 % |
| filter.cutoffMaxHz | 24000 | 19000 | 18875 | -0.66 % |
| filter.cem.cutoffScale | 1 | 1.06 | 1.0595 | -0.04 % |
| env.attackMaxSec | 10 | 6 | 5.8869 | -1.89 % |
| env.decayMaxSec | 15 | 9 | 8.8952 | -1.16 % |
| env.releaseMaxSec | 15 | 11 | 10.788 | -1.93 % |
| env.timeTaper | 1 | 1.15 | 1.1589 | +0.77 % |
| pm.freqARangeSemis | 48 | 36 | 35.762 | -0.66 % |
| pm.amountTaper | 2 | 2.3 | 2.296 | -0.17 % |
| lfo.minHz | 0.03 | 0.05 | 0.049999 | -0.00 % |
| lfo.maxHz | 25 | 18 | 18 | +0.00 % |
| mod.freqRangeSemis | 12 | 9 | 8.9998 | -0.00 % |

Alignment error against the true pre-roll: max 0.04 ms over 27 takes.

Worst unit-profile tuning error vs the hidden offsets: 1.197 cents (includes the different serial's own Vintage residuals, which the profile is expected to absorb).

## Residuals before calibration

Unit: synthetic-unit · session: selftest · firmware: n/a · synthetic: True

**29 of 146 features within tolerance (19.9 %).**

Tolerance per feature = max(acceptance target, 2 × hardware take-to-take SD). Session gain +3.69 dB (level reference C01, not counted).

| Test | Behaviour | Within tolerance | Worst feature | Residual | Tolerance | Status |
|---|---|---|---|---|---|---|
| C02 | Osc A pitch and spectrum across the range | 13/26 | pitch.cents[5] | +6.108 | 0.5 | FAIL |
| C03 | Self-oscillation frequency vs cutoff, both filters | 1/24 | osc.freq_semis[5] | +3.224 | 0.1 | FAIL |
| C04 | Resonance onset and full-resonance level | 0/4 | res.onset_knob[0] | -0.03967 | 0.01 | FAIL |
| C05 | Amp envelope timing | 0/7 | atk.ln_rise[1] | +0.8764 | 0.05 | FAIL |
| C06 | Poly Mod (Osc B -> Freq A) depth | 3/6 | pm.pp_semis[0] | +1.965 | 0.0507 | FAIL |
| C07 | LFO rate and depth | 0/6 | lfo.ln_rate[2] | +0.161 | 0.02 | FAIL |
| C08 | Voice fingerprint (per-voice identity) | 10/53 | fp.ln_decay[8] | +0.7809 | 0.03 | FAIL |
| C09 | Sync sweep spectral trajectory | 2/20 | sweep.log2_centroid[0] | +0.6828 | 0.04 | FAIL |

## Feature detail

### C02 Osc A pitch and spectrum across the range

| Feature | i | Hardware | HW SD | Emulator | Residual | Tol | |
|---|---|---|---|---|---|---|---|
| pitch.cents | 0 | -1.561 | 9.6e-05 | -0.1849 | +1.38 | 0.5 | ✗ |
| pitch.cents | 1 | -1.757 | 5.1e-05 | -0.3266 | +1.43 | 0.5 | ✗ |
| pitch.cents | 2 | -2.132 | 4.9e-05 | -0.502 | +1.63 | 0.5 | ✗ |
| pitch.cents | 3 | -2.921 | 2.5e-05 | -0.6986 | +2.22 | 0.5 | ✗ |
| pitch.cents | 4 | -4.459 | 5e-06 | -0.9824 | +3.48 | 0.5 | ✗ |
| pitch.cents | 5 | -7.565 | 5.3e-06 | -1.457 | +6.11 | 0.5 | ✗ |
| harm_c5.harm_db | 0 | -7.118 | 0.00025 | -7.091 | +0.0274 | 0.75 | ✓ |
| harm_c5.harm_db | 1 | -10.78 | 0.00026 | -10.73 | +0.0544 | 0.75 | ✓ |
| harm_c5.harm_db | 2 | -13.39 | 0.00039 | -13.31 | +0.0831 | 0.75 | ✓ |
| harm_c5.harm_db | 3 | -15.45 | 0.0007 | -15.29 | +0.156 | 0.75 | ✓ |
| harm_c5.harm_db | 4 | -17.17 | 0.0005 | -16.98 | +0.187 | 0.75 | ✓ |
| harm_c5.harm_db | 5 | -18.66 | 0.00038 | -18.37 | +0.293 | 0.75 | ✓ |
| harm_c5.harm_db | 6 | -19.99 | 0.00089 | -19.63 | +0.361 | 0.75 | ✓ |
| harm_c5.harm_db | 7 | -21.19 | 0.00076 | -20.75 | +0.434 | 0.75 | ✓ |
| harm_c5.harm_db | 8 | -22.3 | 0.001 | -21.76 | +0.536 | 0.75 | ✓ |
| harm_c5.harm_db | 9 | -23.34 | 0.00021 | -22.75 | +0.597 | 0.75 | ✓ |
| harm_c5.harm_db | 10 | -24.34 | 0.0022 | -23.6 | +0.743 | 0.75 | ✓ |
| harm_c5.harm_db | 11 | -25.3 | 0.0012 | -24.45 | +0.845 | 0.75 | ✗ |
| harm_c7.harm_db | 0 | -7.582 | 0.00026 | -7.37 | +0.211 | 1 | ✓ |
| harm_c7.harm_db | 1 | -12.04 | 0.00036 | -11.43 | +0.607 | 1 | ✓ |
| harm_c7.harm_db | 2 | -15.76 | 0.00025 | -14.6 | +1.15 | 1 | ✗ |
| harm_c7.harm_db | 3 | -19.17 | 0.00052 | -17.39 | +1.78 | 1 | ✗ |
| harm_c7.harm_db | 4 | -22.56 | 0.0017 | -20.01 | +2.55 | 1 | ✗ |
| harm_c7.harm_db | 5 | -25.93 | 0.00092 | -22.59 | +3.34 | 1 | ✗ |
| harm_c7.harm_db | 6 | -29.42 | 0.0016 | -25.2 | +4.22 | 1 | ✗ |
| harm_c7.harm_db | 7 | -33.04 | 0.0032 | -27.9 | +5.14 | 1 | ✗ |

### C03 Self-oscillation frequency vs cutoff, both filters

| Feature | i | Hardware | HW SD | Emulator | Residual | Tol | |
|---|---|---|---|---|---|---|---|
| osc.freq_semis | 0 | 78.63 | 1.1e-05 | 80.66 | +2.03 | 0.1 | ✗ |
| osc.freq_semis | 1 | 91.1 | 1.6e-06 | 90.16 | -0.944 | 0.1 | ✗ |
| osc.freq_semis | 2 | 103.6 | 2e-08 | 103.6 | +0.0417 | 0.1 | ✓ |
| osc.freq_semis | 3 | 116.1 | 3.3e-07 | 117.1 | +1.03 | 0.1 | ✗ |
| osc.freq_semis | 4 | 128.7 | 1.7e-07 | 130.7 | +2.05 | 0.1 | ✗ |
| osc.freq_semis | 5 | 141.6 | 1.2e-07 | 144.8 | +3.22 | 0.1 | ✗ |
| osc.freq_semis | 6 | 81.93 | 2.5e-05 | 80.82 | -1.11 | 0.1 | ✗ |
| osc.freq_semis | 7 | 95.08 | 5.7e-06 | 93.54 | -1.54 | 0.1 | ✗ |
| osc.freq_semis | 8 | 108.3 | 1.3e-06 | 107 | -1.3 | 0.1 | ✗ |
| osc.freq_semis | 9 | 121.6 | 6.1e-07 | 120.5 | -1.07 | 0.1 | ✗ |
| osc.freq_semis | 10 | 134.8 | 3.5e-07 | 134 | -0.838 | 0.1 | ✗ |
| osc.freq_semis | 11 | 148.1 | 3.2e-07 | 147.5 | -0.6 | 0.1 | ✗ |
| osc.level_db | 0 | -20.65 | 0.00011 | -28.22 | -7.56 | 1 | ✗ |
| osc.level_db | 1 | -19.95 | 3.8e-05 | -20.98 | -1.03 | 1 | ✗ |
| osc.level_db | 2 | -19.96 | 0.00012 | -21 | -1.04 | 1 | ✗ |
| osc.level_db | 3 | -19.99 | 0.00012 | -21.03 | -1.05 | 1 | ✗ |
| osc.level_db | 4 | -20.09 | 0.00017 | -21.21 | -1.11 | 1 | ✗ |
| osc.level_db | 5 | -20.55 | 1.2e-05 | -22.04 | -1.49 | 1 | ✗ |
| osc.level_db | 6 | -30.89 | 0.00037 | -42.91 | -12 | 1 | ✗ |
| osc.level_db | 7 | -29.26 | 0.00017 | -30.32 | -1.06 | 1 | ✗ |
| osc.level_db | 8 | -29.27 | 0.00014 | -30.33 | -1.06 | 1 | ✗ |
| osc.level_db | 9 | -29.29 | 0.00018 | -30.35 | -1.07 | 1 | ✗ |
| osc.level_db | 10 | -29.41 | 0.00032 | -30.5 | -1.1 | 1 | ✗ |
| osc.level_db | 11 | -30.02 | 0.00018 | -31.31 | -1.29 | 1 | ✗ |

### C04 Resonance onset and full-resonance level

| Feature | i | Hardware | HW SD | Emulator | Residual | Tol | |
|---|---|---|---|---|---|---|---|
| res.onset_knob | 0 | 0.9205 | 3.9e-07 | 0.8808 | -0.0397 | 0.01 | ✗ |
| res.onset_knob | 1 | 0.8904 | 3.7e-07 | 0.9186 | +0.0282 | 0.01 | ✗ |
| res.level_db | 0 | -19.99 | 6.4e-05 | -21.04 | -1.05 | 1 | ✗ |
| res.level_db | 1 | -29.29 | 6.6e-05 | -30.35 | -1.07 | 1 | ✗ |

### C05 Amp envelope timing

| Feature | i | Hardware | HW SD | Emulator | Residual | Tol | |
|---|---|---|---|---|---|---|---|
| atk.ln_rise | 0 | -4.711 | 0 | -3.912 | +0.799 | 0.05 | ✗ |
| atk.ln_rise | 1 | -3.135 | 0 | -2.259 | +0.876 | 0.05 | ✗ |
| atk.ln_rise | 2 | -0.9782 | 0 | -0.2937 | +0.684 | 0.05 | ✗ |
| dec.ln_tau | 0 | -5.072 | 0.00032 | -4.562 | +0.51 | 0.05 | ✗ |
| dec.ln_tau | 1 | -3.525 | 0.00024 | -2.82 | +0.705 | 0.05 | ✗ |
| rel.ln_tau | 0 | -5.146 | 0.00099 | -4.614 | +0.532 | 0.05 | ✗ |
| rel.ln_tau | 1 | -3.42 | 0.00037 | -2.83 | +0.59 | 0.05 | ✗ |

### C06 Poly Mod (Osc B -> Freq A) depth

| Feature | i | Hardware | HW SD | Emulator | Residual | Tol | |
|---|---|---|---|---|---|---|---|
| pm.pp_semis | 0 | 1.691 | 6e-05 | 3.656 | +1.96 | 0.0507 | ✗ |
| pm.pp_semis | 1 | 6.09 | 6.7e-05 | 11.17 | +5.08 | 0.183 | ✗ |
| pm.pp_semis | 2 | 13.84 | 6e-05 | 22.81 | +8.96 | 0.415 | ✗ |
| pm.ln_rate | 0 | 0.7204 | 7.2e-07 | 0.7209 | +0.000483 | 0.03 | ✓ |
| pm.ln_rate | 1 | 0.7235 | 6.3e-07 | 0.7239 | +0.000385 | 0.03 | ✓ |
| pm.ln_rate | 2 | 0.7187 | 1.3e-07 | 0.7193 | +0.000594 | 0.03 | ✓ |

### C07 LFO rate and depth

| Feature | i | Hardware | HW SD | Emulator | Residual | Tol | |
|---|---|---|---|---|---|---|---|
| lfo.pp_semis | 0 | 4.301 | 2.9e-05 | 5.763 | +1.46 | 0.215 | ✗ |
| lfo.pp_semis | 1 | 4.261 | 3e-05 | 5.678 | +1.42 | 0.213 | ✗ |
| lfo.pp_semis | 2 | 4.062 | 9e-05 | 5.316 | +1.25 | 0.203 | ✗ |
| lfo.ln_rate | 0 | -0.05162 | 4.3e-07 | -0.1292 | -0.0776 | 0.02 | ✗ |
| lfo.ln_rate | 1 | 0.8374 | 1.5e-06 | 0.8709 | +0.0335 | 0.02 | ✗ |
| lfo.ln_rate | 2 | 1.715 | 7e-07 | 1.876 | +0.161 | 0.02 | ✗ |

### C08 Voice fingerprint (per-voice identity)

| Feature | i | Hardware | HW SD | Emulator | Residual | Tol | |
|---|---|---|---|---|---|---|---|
| fp.cents | 0 | -1.723 | 0.012 | -0.3635 | +1.36 | 0.3 | ✗ |
| fp.cents | 1 | -0.01867 | 0.012 | 0.04851 | +0.0672 | 0.3 | ✓ |
| fp.cents | 2 | -1.191 | 0.0087 | -0.3878 | +0.803 | 0.3 | ✗ |
| fp.cents | 3 | 1.215 | 0.029 | -0.005533 | -1.22 | 0.3 | ✗ |
| fp.cents | 4 | -2.22 | 0.02 | -0.004231 | +2.22 | 0.3 | ✗ |
| fp.cents | 5 | -3.216 | 0.0058 | 0.5206 | +3.74 | 0.3 | ✗ |
| fp.cents | 6 | -0.8527 | 0.015 | -0.05629 | +0.796 | 0.3 | ✗ |
| fp.cents | 7 | -0.8274 | 0.0082 | -0.006475 | +0.821 | 0.3 | ✗ |
| fp.cents | 8 | 0.7997 | 0.0025 | 0.6444 | -0.155 | 0.3 | ✓ |
| fp.cents | 9 | -0.6811 | 0.0072 | -0.3262 | +0.355 | 0.3 | ✗ |
| fp.level_db | 0 | -26.95 | 0.00023 | -24.11 | +2.84 | 0.2 | ✗ |
| fp.level_db | 1 | -25.8 | 0.00027 | -24.04 | +1.76 | 0.2 | ✗ |
| fp.level_db | 2 | -26.18 | 0.00032 | -24.17 | +2.01 | 0.2 | ✗ |
| fp.level_db | 3 | -26.5 | 0.00023 | -23.98 | +2.52 | 0.2 | ✗ |
| fp.level_db | 4 | -25.39 | 6.2e-05 | -24.17 | +1.22 | 0.2 | ✗ |
| fp.level_db | 5 | -26.55 | 0.00023 | -24.08 | +2.48 | 0.2 | ✗ |
| fp.level_db | 6 | -26.76 | 0.00029 | -24.44 | +2.32 | 0.2 | ✗ |
| fp.level_db | 7 | -26.47 | 0.00014 | -24.12 | +2.35 | 0.2 | ✗ |
| fp.level_db | 8 | -26.52 | 4.2e-05 | -24.23 | +2.29 | 0.2 | ✗ |
| fp.level_db | 9 | -26.8 | 0.00024 | -24.04 | +2.76 | 0.2 | ✗ |
| fp.ln_decay | 0 | -3.949 | 0.00016 | -3.257 | +0.691 | 0.03 | ✗ |
| fp.ln_decay | 1 | -3.918 | 4.1e-05 | -3.257 | +0.661 | 0.03 | ✗ |
| fp.ln_decay | 2 | -3.871 | 6.7e-05 | -3.272 | +0.599 | 0.03 | ✗ |
| fp.ln_decay | 3 | -4.006 | 5.4e-05 | -3.262 | +0.743 | 0.03 | ✗ |
| fp.ln_decay | 4 | -3.925 | 0.00027 | -3.268 | +0.657 | 0.03 | ✗ |
| fp.ln_decay | 5 | -4.009 | 9.7e-05 | -3.27 | +0.738 | 0.03 | ✗ |
| fp.ln_decay | 6 | -3.985 | 0.00025 | -3.285 | +0.699 | 0.03 | ✗ |
| fp.ln_decay | 7 | -3.986 | 9e-05 | -3.252 | +0.734 | 0.03 | ✗ |
| fp.ln_decay | 8 | -4.036 | 6.7e-05 | -3.255 | +0.781 | 0.03 | ✗ |
| fp.ln_decay | 9 | -3.931 | 0.00022 | -3.255 | +0.675 | 0.03 | ✗ |
| anchor.cents | 0 | -1.709 | 0.034 | -0.3318 | +1.38 | 0.3 | ✗ |
| anchor.level_db | 0 | -27.24 | 0.00059 | -24.19 | +3.05 | 0.2 | ✗ |
| anchor.ln_decay | 0 | -3.952 | 0.00035 | -3.242 | +0.71 | 0.03 | ✗ |
| fpc.cutoff_semis | 0 | 103.6 | 7.9e-07 | 103.6 | +0.0453 | 0.05 | ✓ |
| fpc.cutoff_semis | 1 | 103.9 | 1.3e-06 | 103.7 | -0.198 | 0.05 | ✗ |
| fpc.cutoff_semis | 2 | 103 | 1.4e-06 | 103.5 | +0.427 | 0.05 | ✗ |
| fpc.cutoff_semis | 3 | 103.6 | 1.6e-06 | 103.6 | +0.00495 | 0.05 | ✓ |
| fpc.cutoff_semis | 4 | 103.8 | 3.2e-07 | 103.5 | -0.31 | 0.05 | ✗ |
| fpc.cutoff_semis | 5 | 103.5 | 5.2e-07 | 103.6 | +0.0372 | 0.05 | ✓ |
| fpc.cutoff_semis | 6 | 103.9 | 4.5e-07 | 103.6 | -0.311 | 0.05 | ✗ |
| fpc.cutoff_semis | 7 | 103.2 | 1.1e-06 | 103.3 | +0.0873 | 0.05 | ✗ |
| fpc.cutoff_semis | 8 | 103.6 | 6.7e-07 | 103.8 | +0.216 | 0.05 | ✗ |
| fpc.cutoff_semis | 9 | 103.5 | 3.7e-07 | 103.5 | -0.0169 | 0.05 | ✓ |
| fpb.cents | 0 | -0.3342 | 0.021 | 0.0484 | +0.383 | 0.3 | ✗ |
| fpb.cents | 1 | -0.415 | 0.016 | -0.6819 | -0.267 | 0.3 | ✓ |
| fpb.cents | 2 | -0.194 | 0.0025 | -0.4371 | -0.243 | 0.3 | ✓ |
| fpb.cents | 3 | 0.03078 | 0.041 | 0.1772 | +0.146 | 0.3 | ✓ |
| fpb.cents | 4 | -0.5316 | 0.036 | 0.02482 | +0.556 | 0.3 | ✗ |
| fpb.cents | 5 | -0.3019 | 0.051 | -0.2367 | +0.0652 | 0.3 | ✓ |
| fpb.cents | 6 | -0.1942 | 0.014 | 0.4224 | +0.617 | 0.3 | ✗ |
| fpb.cents | 7 | -0.3933 | 0.0084 | 0.1128 | +0.506 | 0.3 | ✗ |
| fpb.cents | 8 | -0.6317 | 0.016 | 0.05948 | +0.691 | 0.3 | ✗ |
| fpb.cents | 9 | -0.6078 | 0.027 | 0.2395 | +0.847 | 0.3 | ✗ |

### C09 Sync sweep spectral trajectory

| Feature | i | Hardware | HW SD | Emulator | Residual | Tol | |
|---|---|---|---|---|---|---|---|
| sweep.log2_centroid | 0 | 8.064 | 5.2e-05 | 8.747 | +0.683 | 0.04 | ✗ |
| sweep.log2_centroid | 1 | 8.165 | 9.4e-06 | 8.144 | -0.0213 | 0.04 | ✓ |
| sweep.log2_centroid | 2 | 8.178 | 6.3e-05 | 8.191 | +0.0137 | 0.04 | ✓ |
| sweep.log2_centroid | 3 | 8.179 | 0.0001 | 8.233 | +0.054 | 0.04 | ✗ |
| sweep.log2_centroid | 4 | 8.178 | 0.00016 | 8.247 | +0.0688 | 0.04 | ✗ |
| sweep.log2_centroid | 5 | 8.179 | 4.5e-05 | 8.25 | +0.0708 | 0.04 | ✗ |
| sweep.log2_centroid | 6 | 8.178 | 9.2e-05 | 8.251 | +0.0725 | 0.04 | ✗ |
| sweep.log2_centroid | 7 | 8.179 | 0.0001 | 8.251 | +0.0719 | 0.04 | ✗ |
| sweep.log2_centroid | 8 | 8.178 | 3.3e-05 | 8.251 | +0.0724 | 0.04 | ✗ |
| sweep.log2_centroid | 9 | 8.179 | 7.1e-05 | 8.251 | +0.072 | 0.04 | ✗ |
| sweep.log2_centroid | 10 | 8.179 | 0.00013 | 8.251 | +0.0717 | 0.04 | ✗ |
| sweep.log2_centroid | 11 | 8.178 | 5.4e-05 | 8.251 | +0.0725 | 0.04 | ✗ |
| sweep.log2_centroid | 12 | 8.179 | 5.8e-05 | 8.251 | +0.0717 | 0.04 | ✗ |
| sweep.log2_centroid | 13 | 8.178 | 0.00016 | 8.25 | +0.0721 | 0.04 | ✗ |
| sweep.log2_centroid | 14 | 8.179 | 0.00013 | 8.251 | +0.0717 | 0.04 | ✗ |
| sweep.log2_centroid | 15 | 8.178 | 0.00011 | 8.251 | +0.0725 | 0.04 | ✗ |
| sweep.log2_centroid | 16 | 8.179 | 3.6e-05 | 8.25 | +0.0718 | 0.04 | ✗ |
| sweep.log2_centroid | 17 | 8.179 | 4.5e-05 | 8.25 | +0.0717 | 0.04 | ✗ |
| sweep.log2_centroid | 18 | 8.178 | 7.9e-05 | 8.251 | +0.0723 | 0.04 | ✗ |
| sweep.log2_centroid | 19 | 8.179 | 6.7e-05 | 8.251 | +0.0718 | 0.04 | ✗ |


## Residuals after calibration

Unit: synthetic-unit · session: selftest · firmware: n/a · synthetic: True

**146 of 146 features within tolerance (100.0 %).**

Tolerance per feature = max(acceptance target, 2 × hardware take-to-take SD). Session gain +3.06 dB (level reference C01, not counted).

| Test | Behaviour | Within tolerance | Worst feature | Residual | Tolerance | Status |
|---|---|---|---|---|---|---|
| C02 | Osc A pitch and spectrum across the range | 26/26 | pitch.cents[0] | +0.1265 | 0.5 | pass |
| C03 | Self-oscillation frequency vs cutoff, both filters | 24/24 | osc.freq_semis[0] | -0.01623 | 0.1 | pass |
| C04 | Resonance onset and full-resonance level | 4/4 | res.onset_knob[0] | +0.00183 | 0.01 | pass |
| C05 | Amp envelope timing | 7/7 | dec.ln_tau[0] | +0.001291 | 0.05 | pass |
| C06 | Poly Mod (Osc B -> Freq A) depth | 6/6 | pm.pp_semis[2] | -0.01471 | 0.415 | pass |
| C07 | LFO rate and depth | 6/6 | lfo.pp_semis[0] | -0.0004052 | 0.215 | pass |
| C08 | Voice fingerprint (per-voice identity) | 53/53 | anchor.cents[0] | -0.08471 | 0.3 | pass |
| C09 | Sync sweep spectral trajectory | 20/20 | sweep.log2_centroid[14] | -0.00217 | 0.04 | pass |

## Feature detail

### C02 Osc A pitch and spectrum across the range

| Feature | i | Hardware | HW SD | Emulator | Residual | Tol | |
|---|---|---|---|---|---|---|---|
| pitch.cents | 0 | -1.561 | 9.6e-05 | -1.435 | +0.127 | 0.5 | ✓ |
| pitch.cents | 1 | -1.757 | 5.1e-05 | -1.73 | +0.027 | 0.5 | ✓ |
| pitch.cents | 2 | -2.132 | 4.9e-05 | -2.215 | -0.083 | 0.5 | ✓ |
| pitch.cents | 3 | -2.921 | 2.5e-05 | -3.03 | -0.109 | 0.5 | ✓ |
| pitch.cents | 4 | -4.459 | 5e-06 | -4.549 | -0.0899 | 0.5 | ✓ |
| pitch.cents | 5 | -7.565 | 5.3e-06 | -7.491 | +0.0746 | 0.5 | ✓ |
| harm_c5.harm_db | 0 | -7.118 | 0.00025 | -7.116 | +0.00209 | 0.75 | ✓ |
| harm_c5.harm_db | 1 | -10.78 | 0.00026 | -10.77 | +0.00593 | 0.75 | ✓ |
| harm_c5.harm_db | 2 | -13.39 | 0.00039 | -13.38 | +0.0109 | 0.75 | ✓ |
| harm_c5.harm_db | 3 | -15.45 | 0.0007 | -15.43 | +0.0176 | 0.75 | ✓ |
| harm_c5.harm_db | 4 | -17.17 | 0.0005 | -17.14 | +0.0259 | 0.75 | ✓ |
| harm_c5.harm_db | 5 | -18.66 | 0.00038 | -18.63 | +0.035 | 0.75 | ✓ |
| harm_c5.harm_db | 6 | -19.99 | 0.00089 | -19.95 | +0.0369 | 0.75 | ✓ |
| harm_c5.harm_db | 7 | -21.19 | 0.00076 | -21.16 | +0.0278 | 0.75 | ✓ |
| harm_c5.harm_db | 8 | -22.3 | 0.001 | -22.28 | +0.0218 | 0.75 | ✓ |
| harm_c5.harm_db | 9 | -23.34 | 0.00021 | -23.33 | +0.0151 | 0.75 | ✓ |
| harm_c5.harm_db | 10 | -24.34 | 0.0022 | -24.33 | +0.0118 | 0.75 | ✓ |
| harm_c5.harm_db | 11 | -25.3 | 0.0012 | -25.29 | +0.0101 | 0.75 | ✓ |
| harm_c7.harm_db | 0 | -7.582 | 0.00026 | -7.611 | -0.0293 | 1 | ✓ |
| harm_c7.harm_db | 1 | -12.04 | 0.00036 | -12.04 | -0.000136 | 1 | ✓ |
| harm_c7.harm_db | 2 | -15.76 | 0.00025 | -15.74 | +0.0165 | 1 | ✓ |
| harm_c7.harm_db | 3 | -19.17 | 0.00052 | -19.2 | -0.0348 | 1 | ✓ |
| harm_c7.harm_db | 4 | -22.56 | 0.0017 | -22.52 | +0.0342 | 1 | ✓ |
| harm_c7.harm_db | 5 | -25.93 | 0.00092 | -25.91 | +0.0206 | 1 | ✓ |
| harm_c7.harm_db | 6 | -29.42 | 0.0016 | -29.41 | +0.00896 | 1 | ✓ |
| harm_c7.harm_db | 7 | -33.04 | 0.0032 | -33 | +0.0385 | 1 | ✓ |

### C03 Self-oscillation frequency vs cutoff, both filters

| Feature | i | Hardware | HW SD | Emulator | Residual | Tol | |
|---|---|---|---|---|---|---|---|
| osc.freq_semis | 0 | 78.63 | 1.1e-05 | 78.61 | -0.0162 | 0.1 | ✓ |
| osc.freq_semis | 1 | 91.1 | 1.6e-06 | 91.09 | -0.0102 | 0.1 | ✓ |
| osc.freq_semis | 2 | 103.6 | 2e-08 | 103.6 | -0.00884 | 0.1 | ✓ |
| osc.freq_semis | 3 | 116.1 | 3.3e-07 | 116.1 | -0.00681 | 0.1 | ✓ |
| osc.freq_semis | 4 | 128.7 | 1.7e-07 | 128.7 | -0.00407 | 0.1 | ✓ |
| osc.freq_semis | 5 | 141.6 | 1.2e-07 | 141.6 | +0.00332 | 0.1 | ✓ |
| osc.freq_semis | 6 | 81.93 | 2.5e-05 | 81.94 | +0.0118 | 0.1 | ✓ |
| osc.freq_semis | 7 | 95.08 | 5.7e-06 | 95.09 | +0.0136 | 0.1 | ✓ |
| osc.freq_semis | 8 | 108.3 | 1.3e-06 | 108.3 | +0.0114 | 0.1 | ✓ |
| osc.freq_semis | 9 | 121.6 | 6.1e-07 | 121.6 | +0.00481 | 0.1 | ✓ |
| osc.freq_semis | 10 | 134.8 | 3.5e-07 | 134.8 | -0.000869 | 0.1 | ✓ |
| osc.freq_semis | 11 | 148.1 | 3.2e-07 | 148.1 | -0.00168 | 0.1 | ✓ |
| osc.level_db | 0 | -21.29 | 0.00011 | -21.21 | +0.0819 | 1 | ✓ |
| osc.level_db | 1 | -20.59 | 3.8e-05 | -20.56 | +0.0278 | 1 | ✓ |
| osc.level_db | 2 | -20.59 | 0.00012 | -20.57 | +0.0246 | 1 | ✓ |
| osc.level_db | 3 | -20.62 | 0.00012 | -20.59 | +0.0307 | 1 | ✓ |
| osc.level_db | 4 | -20.73 | 0.00017 | -20.7 | +0.0295 | 1 | ✓ |
| osc.level_db | 5 | -21.18 | 1.2e-05 | -21.15 | +0.0322 | 1 | ✓ |
| osc.level_db | 6 | -31.53 | 0.00037 | -31.47 | +0.0614 | 1 | ✓ |
| osc.level_db | 7 | -29.9 | 0.00017 | -29.88 | +0.019 | 1 | ✓ |
| osc.level_db | 8 | -29.9 | 0.00014 | -29.88 | +0.0225 | 1 | ✓ |
| osc.level_db | 9 | -29.92 | 0.00018 | -29.91 | +0.0181 | 1 | ✓ |
| osc.level_db | 10 | -30.05 | 0.00032 | -30.02 | +0.0224 | 1 | ✓ |
| osc.level_db | 11 | -30.66 | 0.00018 | -30.63 | +0.0253 | 1 | ✓ |

### C04 Resonance onset and full-resonance level

| Feature | i | Hardware | HW SD | Emulator | Residual | Tol | |
|---|---|---|---|---|---|---|---|
| res.onset_knob | 0 | 0.9205 | 3.9e-07 | 0.9223 | +0.00183 | 0.01 | ✓ |
| res.onset_knob | 1 | 0.8904 | 3.7e-07 | 0.8912 | +0.000785 | 0.01 | ✓ |
| res.level_db | 0 | -20.62 | 6.4e-05 | -20.59 | +0.0294 | 1 | ✓ |
| res.level_db | 1 | -29.93 | 6.6e-05 | -29.91 | +0.0196 | 1 | ✓ |

### C05 Amp envelope timing

| Feature | i | Hardware | HW SD | Emulator | Residual | Tol | |
|---|---|---|---|---|---|---|---|
| atk.ln_rise | 0 | -4.711 | 0 | -4.711 | +0 | 0.05 | ✓ |
| atk.ln_rise | 1 | -3.135 | 0 | -3.135 | +0 | 0.05 | ✓ |
| atk.ln_rise | 2 | -0.9782 | 0 | -0.9782 | +0 | 0.05 | ✓ |
| dec.ln_tau | 0 | -5.072 | 0.00032 | -5.071 | +0.00129 | 0.05 | ✓ |
| dec.ln_tau | 1 | -3.525 | 0.00024 | -3.525 | -0.000647 | 0.05 | ✓ |
| rel.ln_tau | 0 | -5.146 | 0.00099 | -5.147 | -0.00116 | 0.05 | ✓ |
| rel.ln_tau | 1 | -3.42 | 0.00037 | -3.419 | +0.000975 | 0.05 | ✓ |

### C06 Poly Mod (Osc B -> Freq A) depth

| Feature | i | Hardware | HW SD | Emulator | Residual | Tol | |
|---|---|---|---|---|---|---|---|
| pm.pp_semis | 0 | 1.691 | 6e-05 | 1.691 | +9.97e-05 | 0.0507 | ✓ |
| pm.pp_semis | 1 | 6.09 | 6.7e-05 | 6.096 | +0.00509 | 0.183 | ✓ |
| pm.pp_semis | 2 | 13.84 | 6e-05 | 13.83 | -0.0147 | 0.415 | ✓ |
| pm.ln_rate | 0 | 0.7204 | 7.2e-07 | 0.7209 | +0.000533 | 0.03 | ✓ |
| pm.ln_rate | 1 | 0.7235 | 6.3e-07 | 0.724 | +0.00042 | 0.03 | ✓ |
| pm.ln_rate | 2 | 0.7187 | 1.3e-07 | 0.7194 | +0.000643 | 0.03 | ✓ |

### C07 LFO rate and depth

| Feature | i | Hardware | HW SD | Emulator | Residual | Tol | |
|---|---|---|---|---|---|---|---|
| lfo.pp_semis | 0 | 4.301 | 2.9e-05 | 4.301 | -0.000405 | 0.215 | ✓ |
| lfo.pp_semis | 1 | 4.261 | 3e-05 | 4.261 | -0.000388 | 0.213 | ✓ |
| lfo.pp_semis | 2 | 4.062 | 9e-05 | 4.062 | +6.72e-07 | 0.203 | ✓ |
| lfo.ln_rate | 0 | -0.05162 | 4.3e-07 | -0.05162 | -2.01e-06 | 0.02 | ✓ |
| lfo.ln_rate | 1 | 0.8374 | 1.5e-06 | 0.8374 | +9.83e-06 | 0.02 | ✓ |
| lfo.ln_rate | 2 | 1.715 | 7e-07 | 1.715 | +1.25e-05 | 0.02 | ✓ |

### C08 Voice fingerprint (per-voice identity)

| Feature | i | Hardware | HW SD | Emulator | Residual | Tol | |
|---|---|---|---|---|---|---|---|
| fp.cents | 0 | -1.723 | 0.012 | -1.75 | -0.0263 | 0.3 | ✓ |
| fp.cents | 1 | -0.01867 | 0.012 | -0.04037 | -0.0217 | 0.3 | ✓ |
| fp.cents | 2 | -1.191 | 0.0087 | -1.226 | -0.0346 | 0.3 | ✓ |
| fp.cents | 3 | 1.215 | 0.029 | 1.153 | -0.0623 | 0.3 | ✓ |
| fp.cents | 4 | -2.22 | 0.02 | -2.263 | -0.0429 | 0.3 | ✓ |
| fp.cents | 5 | -3.216 | 0.0058 | -3.265 | -0.0489 | 0.3 | ✓ |
| fp.cents | 6 | -0.8527 | 0.015 | -0.9027 | -0.05 | 0.3 | ✓ |
| fp.cents | 7 | -0.8274 | 0.0082 | -0.8699 | -0.0425 | 0.3 | ✓ |
| fp.cents | 8 | 0.7997 | 0.0025 | 0.7548 | -0.0449 | 0.3 | ✓ |
| fp.cents | 9 | -0.6811 | 0.0072 | -0.7362 | -0.0551 | 0.3 | ✓ |
| fp.level_db | 0 | -27.59 | 0.00023 | -27.57 | +0.0184 | 0.2 | ✓ |
| fp.level_db | 1 | -26.43 | 0.00027 | -26.41 | +0.0179 | 0.2 | ✓ |
| fp.level_db | 2 | -26.81 | 0.00032 | -26.79 | +0.0246 | 0.2 | ✓ |
| fp.level_db | 3 | -27.13 | 0.00023 | -27.11 | +0.0193 | 0.2 | ✓ |
| fp.level_db | 4 | -26.02 | 6.2e-05 | -25.99 | +0.0271 | 0.2 | ✓ |
| fp.level_db | 5 | -27.19 | 0.00023 | -27.17 | +0.0161 | 0.2 | ✓ |
| fp.level_db | 6 | -27.4 | 0.00029 | -27.38 | +0.0183 | 0.2 | ✓ |
| fp.level_db | 7 | -27.11 | 0.00014 | -27.09 | +0.0144 | 0.2 | ✓ |
| fp.level_db | 8 | -27.16 | 4.2e-05 | -27.15 | +0.0116 | 0.2 | ✓ |
| fp.level_db | 9 | -27.43 | 0.00024 | -27.41 | +0.0197 | 0.2 | ✓ |
| fp.ln_decay | 0 | -3.949 | 0.00016 | -3.947 | +0.00152 | 0.03 | ✓ |
| fp.ln_decay | 1 | -3.918 | 4.1e-05 | -3.914 | +0.00388 | 0.03 | ✓ |
| fp.ln_decay | 2 | -3.871 | 6.7e-05 | -3.871 | -0.000548 | 0.03 | ✓ |
| fp.ln_decay | 3 | -4.006 | 5.4e-05 | -4.009 | -0.003 | 0.03 | ✓ |
| fp.ln_decay | 4 | -3.925 | 0.00027 | -3.925 | +0.00016 | 0.03 | ✓ |
| fp.ln_decay | 5 | -4.009 | 9.7e-05 | -4.009 | -8.65e-05 | 0.03 | ✓ |
| fp.ln_decay | 6 | -3.985 | 0.00025 | -3.983 | +0.00119 | 0.03 | ✓ |
| fp.ln_decay | 7 | -3.986 | 9e-05 | -3.987 | -0.000866 | 0.03 | ✓ |
| fp.ln_decay | 8 | -4.036 | 6.7e-05 | -4.034 | +0.00155 | 0.03 | ✓ |
| fp.ln_decay | 9 | -3.931 | 0.00022 | -3.926 | +0.00493 | 0.03 | ✓ |
| anchor.cents | 0 | -1.709 | 0.034 | -1.794 | -0.0847 | 0.3 | ✓ |
| anchor.level_db | 0 | -27.87 | 0.00059 | -27.86 | +0.0143 | 0.2 | ✓ |
| anchor.ln_decay | 0 | -3.952 | 0.00035 | -3.949 | +0.00283 | 0.03 | ✓ |
| fpc.cutoff_semis | 0 | 103.6 | 7.9e-07 | 103.6 | -0.00522 | 0.05 | ✓ |
| fpc.cutoff_semis | 1 | 103.9 | 1.3e-06 | 103.9 | -0.00522 | 0.05 | ✓ |
| fpc.cutoff_semis | 2 | 103 | 1.4e-06 | 103 | -0.00523 | 0.05 | ✓ |
| fpc.cutoff_semis | 3 | 103.6 | 1.6e-06 | 103.6 | -0.00522 | 0.05 | ✓ |
| fpc.cutoff_semis | 4 | 103.8 | 3.2e-07 | 103.8 | -0.00522 | 0.05 | ✓ |
| fpc.cutoff_semis | 5 | 103.5 | 5.2e-07 | 103.5 | -0.00522 | 0.05 | ✓ |
| fpc.cutoff_semis | 6 | 103.9 | 4.5e-07 | 103.9 | -0.00523 | 0.05 | ✓ |
| fpc.cutoff_semis | 7 | 103.2 | 1.1e-06 | 103.2 | -0.00522 | 0.05 | ✓ |
| fpc.cutoff_semis | 8 | 103.6 | 6.7e-07 | 103.5 | -0.00523 | 0.05 | ✓ |
| fpc.cutoff_semis | 9 | 103.5 | 3.7e-07 | 103.5 | -0.00522 | 0.05 | ✓ |
| fpb.cents | 0 | -0.3342 | 0.021 | -0.3729 | -0.0387 | 0.3 | ✓ |
| fpb.cents | 1 | -0.415 | 0.016 | -0.4642 | -0.0492 | 0.3 | ✓ |
| fpb.cents | 2 | -0.194 | 0.0025 | -0.2409 | -0.0468 | 0.3 | ✓ |
| fpb.cents | 3 | 0.03078 | 0.041 | -0.01133 | -0.0421 | 0.3 | ✓ |
| fpb.cents | 4 | -0.5316 | 0.036 | -0.5693 | -0.0377 | 0.3 | ✓ |
| fpb.cents | 5 | -0.3019 | 0.051 | -0.3455 | -0.0436 | 0.3 | ✓ |
| fpb.cents | 6 | -0.1942 | 0.014 | -0.2426 | -0.0484 | 0.3 | ✓ |
| fpb.cents | 7 | -0.3933 | 0.0084 | -0.4433 | -0.0501 | 0.3 | ✓ |
| fpb.cents | 8 | -0.6317 | 0.016 | -0.6775 | -0.0457 | 0.3 | ✓ |
| fpb.cents | 9 | -0.6078 | 0.027 | -0.6738 | -0.066 | 0.3 | ✓ |

### C09 Sync sweep spectral trajectory

| Feature | i | Hardware | HW SD | Emulator | Residual | Tol | |
|---|---|---|---|---|---|---|---|
| sweep.log2_centroid | 0 | 8.064 | 5.2e-05 | 8.066 | +0.00177 | 0.04 | ✓ |
| sweep.log2_centroid | 1 | 8.165 | 9.4e-06 | 8.167 | +0.00192 | 0.04 | ✓ |
| sweep.log2_centroid | 2 | 8.178 | 6.3e-05 | 8.177 | -0.00116 | 0.04 | ✓ |
| sweep.log2_centroid | 3 | 8.179 | 0.0001 | 8.177 | -0.00206 | 0.04 | ✓ |
| sweep.log2_centroid | 4 | 8.178 | 0.00016 | 8.177 | -0.00112 | 0.04 | ✓ |
| sweep.log2_centroid | 5 | 8.179 | 4.5e-05 | 8.177 | -0.00169 | 0.04 | ✓ |
| sweep.log2_centroid | 6 | 8.178 | 9.2e-05 | 8.177 | -0.00107 | 0.04 | ✓ |
| sweep.log2_centroid | 7 | 8.179 | 0.0001 | 8.177 | -0.00211 | 0.04 | ✓ |
| sweep.log2_centroid | 8 | 8.178 | 3.3e-05 | 8.177 | -0.00141 | 0.04 | ✓ |
| sweep.log2_centroid | 9 | 8.179 | 7.1e-05 | 8.177 | -0.00117 | 0.04 | ✓ |
| sweep.log2_centroid | 10 | 8.179 | 0.00013 | 8.177 | -0.0018 | 0.04 | ✓ |
| sweep.log2_centroid | 11 | 8.178 | 5.4e-05 | 8.177 | -0.00151 | 0.04 | ✓ |
| sweep.log2_centroid | 12 | 8.179 | 5.8e-05 | 8.177 | -0.00177 | 0.04 | ✓ |
| sweep.log2_centroid | 13 | 8.178 | 0.00016 | 8.177 | -0.00103 | 0.04 | ✓ |
| sweep.log2_centroid | 14 | 8.179 | 0.00013 | 8.177 | -0.00217 | 0.04 | ✓ |
| sweep.log2_centroid | 15 | 8.178 | 0.00011 | 8.177 | -0.00152 | 0.04 | ✓ |
| sweep.log2_centroid | 16 | 8.179 | 3.6e-05 | 8.177 | -0.00163 | 0.04 | ✓ |
| sweep.log2_centroid | 17 | 8.179 | 4.5e-05 | 8.177 | -0.00149 | 0.04 | ✓ |
| sweep.log2_centroid | 18 | 8.178 | 7.9e-05 | 8.177 | -0.00157 | 0.04 | ✓ |
| sweep.log2_centroid | 19 | 8.179 | 6.7e-05 | 8.177 | -0.00208 | 0.04 | ✓ |

#!/usr/bin/env python3
"""Measurement and calibration-fitting tool for the Prophet-10 Rev4 model.

Every measurement runs identically on a hardware recording and on the
software render of the same scenario, so the two can be compared
quantitatively. Only numpy is required.

Subcommands
  pitch       <wav> [--start S --dur S]          fundamental (Hz, cents vs nearest note)
  spectrum    <wav> [--start --dur --harmonics N] harmonic amplitudes (dB re fundamental)
  pitchtrack  <wav> [--hop MS]                    f0 over time (glide, LFO depth/rate, drift)
  envelope    <wav> [--start --dur]               RMS envelope, attack time, decay/release taus
  duty        <wav> [--start --dur]               pulse duty cycle from the waveform
  noteseq     <wav> [--threshold DB]              per-note pitch/level/brightness statistics
  selfosc     <wav> --segments t0:t1,...          self-oscillation frequency/level per segment
  compare     <hw.wav> <sw.wav> --what spectrum|envelope|pitchtrack [--start --dur]
  fit-env     <csv>   knob,seconds rows           -> env.*MinSec/MaxSec/timeTaper keys
  fit-cutoff  <csv>   knob,hz rows                -> filter.cutoffMinHz/MaxHz keys
  fit-polymod <csv>   amount,semis rows            -> pm.amountTaper / pm.*RangeSemis

All recordings: mono or stereo WAV (16/24/32-bit PCM or 32-bit float). Use
the first channel. Hardware captures must be made with the procedure in
docs/VALIDATION.md (no effects, fixed gain, same sample rate as the render).
"""
import argparse
import struct
import sys

import numpy as np


# --------------------------------------------------------------------- I/O --
def read_wav(path):
    with open(path, "rb") as f:
        data = f.read()
    if data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        raise SystemExit(f"{path}: not a WAV file")
    pos = 12
    fmt = None
    while pos + 8 <= len(data):
        cid = data[pos:pos + 4]
        size = struct.unpack("<I", data[pos + 4:pos + 8])[0]
        body = data[pos + 8:pos + 8 + size]
        if cid == b"fmt ":
            tag, ch, rate = struct.unpack("<HHI", body[:8])
            bits = struct.unpack("<H", body[14:16])[0]
            if tag == 0xFFFE:
                tag = struct.unpack("<H", body[24:26])[0]
            fmt = (tag, ch, rate, bits)
        elif cid == b"data":
            tag, ch, rate, bits = fmt
            if tag == 3 and bits == 32:
                x = np.frombuffer(body, dtype="<f4").astype(np.float64)
            elif bits == 16:
                x = np.frombuffer(body, dtype="<i2") / 32768.0
            elif bits == 24:
                b = np.frombuffer(body, dtype=np.uint8).reshape(-1, 3)
                v = (b[:, 0].astype(np.int32) | (b[:, 1].astype(np.int32) << 8) | (b[:, 2].astype(np.int32) << 16))
                v = np.where(v >= 1 << 23, v - (1 << 24), v)
                x = v / float(1 << 23)
            elif bits == 32:
                x = np.frombuffer(body, dtype="<i4") / 2147483648.0
            else:
                raise SystemExit(f"{path}: unsupported format")
            return x.reshape(-1, ch)[:, 0].copy(), rate
        pos += 8 + size + (size & 1)
    raise SystemExit(f"{path}: no data chunk")


def segment(x, fs, start, dur):
    a = int(start * fs)
    b = len(x) if dur is None else min(len(x), a + int(dur * fs))
    return x[a:b]


# ---------------------------------------------------------------- analysis --
def bh_window(n):
    w = np.arange(n) / (n - 1)
    return (0.35875 - 0.48829 * np.cos(2 * np.pi * w) + 0.14128 * np.cos(4 * np.pi * w)
            - 0.01168 * np.cos(6 * np.pi * w))


def spectrum_db(x):
    n = len(x)
    X = np.abs(np.fft.rfft(x * bh_window(n))) + 1e-15
    return 20 * np.log10(X), np.fft.rfftfreq(n, 1.0)


def estimate_f0(x, fs, fmin=20.0, fmax=5000.0):
    """Harmonic-sum f0 estimate refined by parabolic interpolation."""
    n = 1 << int(np.ceil(np.log2(max(len(x), 4096))))
    xx = np.zeros(n)
    xx[:len(x)] = (x - np.mean(x)) * bh_window(len(x))
    mag = np.abs(np.fft.rfft(xx))
    freqs = np.fft.rfftfreq(n, 1.0 / fs)
    cands = np.arange(int(fmin / (fs / n)), int(fmax / (fs / n)))
    score = np.zeros(len(cands))
    logm = np.log(mag + 1e-12)
    for h in range(1, 6):
        idx = cands * h
        ok = idx < len(mag)
        score[ok] += logm[idx[ok]] / h
    k = cands[int(np.argmax(score))]
    # refine on the strongest harmonic near k*h for accuracy
    best_h, best_bin = 1, k
    for h in range(1, 9):
        c = k * h
        if c + 3 >= len(mag):
            break
        local = c - 3 + int(np.argmax(mag[c - 3:c + 4]))
        if mag[local] > mag[best_bin] * 0.5:
            best_h, best_bin = h, local
    a, b, c = np.log(mag[best_bin - 1:best_bin + 2] + 1e-12)
    off = 0.5 * (a - c) / (a - 2 * b + c) if (a - 2 * b + c) != 0 else 0.0
    return (best_bin + off) * fs / n / best_h


def cents_from_note(f):
    note = 69 + 12 * np.log2(f / 440.0)
    return int(round(note)), 100 * (note - round(note))


def harmonic_levels(x, fs, f0, count):
    db, fr = spectrum_db(x)
    fr = fr * fs
    out = []
    for h in range(1, count + 1):
        fh = h * f0
        if fh > fs / 2 * 0.95:
            break
        i = int(round(fh / (fs / len(x))))
        lo, hi = max(1, i - 4), min(len(db) - 1, i + 5)
        out.append((h, fh, float(np.max(db[lo:hi]))))
    ref = out[0][2] if out else 0.0
    return [(h, f, l - ref) for h, f, l in out]


def rms_envelope(x, fs, hop_ms=1.0, win_ms=4.0):
    hop = max(1, int(fs * hop_ms / 1000))
    win = max(hop, int(fs * win_ms / 1000))
    n = (len(x) - win) // hop
    env = np.array([np.sqrt(np.mean(x[i * hop:i * hop + win] ** 2)) for i in range(max(n, 0))])
    t = (np.arange(len(env)) * hop + win / 2) / fs
    return t, env


def envelope_metrics(x, fs, stage="attack-decay"):
    """attack-decay: segment must start before note-on and end before note-off
    (the last 20% of the segment is taken as the sustain level).
    release: segment must start at note-off."""
    t, e = rms_envelope(x, fs, hop_ms=1.0, win_ms=8.0)
    if len(e) == 0:
        return {}
    res = {}
    if stage == "release":
        start = e[0]
        good = (e > start * 10 ** (-60 / 20)) & (e < start * 0.9)
        p = np.polyfit(t[good], np.log(e[good]), 1)
        res["release_tau"] = float(-1.0 / p[0])
        res["release_to_-40dB"] = float(np.log(100) * -1.0 / p[0])
        return res
    pk = int(np.argmax(e))
    peak = e[pk]
    on = int(np.argmax(e > 0.02 * peak))
    res.update({"peak": float(peak), "t_peak": float(t[pk] - t[on]),
                "attack_10_90": float(t[int(np.argmax(e > 0.9 * peak))] - t[int(np.argmax(e > 0.1 * peak))])})
    floor = float(np.median(e[int(len(e) * 0.8):]))
    res["sustain_ratio"] = floor / peak
    y = e[pk:] - floor
    good = y > (peak - floor) * 0.01
    # use only the contiguous decaying part
    stop = int(np.argmin(good)) if not good.all() else len(good)
    if stop > 10:
        tt = t[pk:pk + stop]
        p = np.polyfit(tt, np.log(y[:stop]), 1)
        res["decay_tau"] = float(-1.0 / p[0]) if p[0] < 0 else float("inf")
    return res


def pitch_track(x, fs, hop_ms=10.0, win_ms=60.0):
    hop, win = int(fs * hop_ms / 1000), int(fs * win_ms / 1000)
    out = []
    for i in range(0, len(x) - win, hop):
        frame = x[i:i + win]
        if np.sqrt(np.mean(frame ** 2)) < 1e-4:
            continue
        out.append(((i + win / 2) / fs, estimate_f0(frame, fs)))
    return out


def detect_notes(x, fs, threshold_db=-40.0):
    """Onsets = level rises of >6 dB within 10 ms above an absolute threshold."""
    t, e = rms_envelope(x, fs, hop_ms=2.0, win_ms=10.0)
    if len(e) == 0:
        return []
    thr = np.max(e) * 10 ** (threshold_db / 20)
    lag = 5
    onsets, last = [], -1.0
    for i in range(lag, len(e)):
        if e[i] > thr and e[i] > 2.0 * e[i - lag] and t[i] - last > 0.1:
            onsets.append(t[i - lag])
            last = t[i]
    return onsets


# --------------------------------------------------------------- commands --
def cmd_pitch(a):
    x, fs = read_wav(a.wav)
    f = estimate_f0(segment(x, fs, a.start, a.dur), fs)
    n, c = cents_from_note(f)
    print(f"f0 {f:.4f} Hz  (MIDI {n} {c:+.2f} cents)")


def cmd_spectrum(a):
    x, fs = read_wav(a.wav)
    s = segment(x, fs, a.start, a.dur)
    f0 = estimate_f0(s, fs)
    print(f"f0 {f0:.3f} Hz")
    for h, f, l in harmonic_levels(s, fs, f0, a.harmonics):
        print(f"{h:3d} {f:10.1f} Hz {l:8.2f} dB")


def cmd_pitchtrack(a):
    x, fs = read_wav(a.wav)
    for t, f in pitch_track(x, fs, a.hop):
        n, c = cents_from_note(f)
        print(f"{t:8.3f} {f:10.3f} {n:4d} {c:+7.2f}")


def cmd_envelope(a):
    x, fs = read_wav(a.wav)
    for k, v in envelope_metrics(segment(x, fs, a.start, a.dur), fs, a.stage).items():
        print(f"{k:14s} {v:.5f}")


def cmd_duty(a):
    x, fs = read_wav(a.wav)
    s = segment(x, fs, a.start, a.dur)
    mid = 0.5 * (np.percentile(s, 98) + np.percentile(s, 2))
    print(f"duty {np.mean(s > mid):.4f}  (fraction of time above the mid level)")


def cmd_noteseq(a):
    x, fs = read_wav(a.wav)
    onsets = detect_notes(x, fs, a.threshold)
    rows = []
    for i, t0 in enumerate(onsets):
        t1 = onsets[i + 1] if i + 1 < len(onsets) else t0 + 0.5
        seg = segment(x, fs, t0 + 0.03, max(0.05, min(0.3, t1 - t0 - 0.05)))
        if len(seg) < 2048:
            continue
        f = estimate_f0(seg, fs)
        db, fr = spectrum_db(seg)
        mag = 10 ** (db / 20)
        centroid = float(np.sum(mag * fr * fs) / np.sum(mag))
        level = 20 * np.log10(np.sqrt(np.mean(seg ** 2)) + 1e-12)
        _, c = cents_from_note(f)
        rows.append((t0, f, c, level, centroid))
        print(f"{t0:8.3f}  f0 {f:9.3f}  {c:+7.2f} c  {level:7.2f} dBFS  centroid {centroid:8.1f} Hz")
    if rows:
        arr = np.array(rows)
        print(f"\nnotes {len(rows)}  pitch sd {np.std(arr[:, 2]):.3f} c  level sd {np.std(arr[:, 3]):.3f} dB"
              f"  centroid sd {np.std(arr[:, 4]):.1f} Hz")


def cmd_selfosc(a):
    x, fs = read_wav(a.wav)
    for segspec in a.segments.split(","):
        t0, t1 = (float(v) for v in segspec.split(":"))
        s = segment(x, fs, t0 + 0.2, t1 - t0 - 0.25)
        db, fr = spectrum_db(s)
        k = int(np.argmax(db[1:])) + 1
        a_, b_, c_ = db[k - 1:k + 2]
        off = 0.5 * (a_ - c_) / (a_ - 2 * b_ + c_)
        f = (k + off) * fs / len(s)
        lvl = 20 * np.log10(np.sqrt(np.mean(s ** 2)) + 1e-12)
        print(f"{t0:7.2f}-{t1:7.2f} s  {f:10.2f} Hz  {lvl:7.2f} dBFS")


def cmd_compare(a):
    xh, fsh = read_wav(a.hw)
    xs, fss = read_wav(a.sw)
    if fsh != fss:
        raise SystemExit("sample rates differ; resample the capture first")
    fs = fsh
    sh, ss = segment(xh, fs, a.start, a.dur), segment(xs, fs, a.start, a.dur)
    if a.what == "spectrum":
        fh, fsw = estimate_f0(sh, fs), estimate_f0(ss, fs)
        print(f"f0 hw {fh:.3f} Hz  sw {fsw:.3f} Hz  diff {1200 * np.log2(fsw / fh):+.2f} cents")
        lh, ls = harmonic_levels(sh, fs, fh, 40), harmonic_levels(ss, fs, fsw, 40)
        d = []
        for (h, _, l1), (_, _, l2) in zip(lh, ls):
            d.append(l2 - l1)
            print(f"{h:3d}  hw {l1:7.2f}  sw {l2:7.2f}  diff {l2 - l1:+6.2f} dB")
        print(f"harmonic level RMS difference {np.sqrt(np.mean(np.square(d))):.2f} dB")
    elif a.what == "envelope":
        mh, ms = envelope_metrics(sh, fs, a.stage), envelope_metrics(ss, fs, a.stage)
        for k in sorted(set(mh) | set(ms)):
            vh, vs = mh.get(k, float("nan")), ms.get(k, float("nan"))
            ratio = vs / vh if vh else float("nan")
            print(f"{k:14s} hw {vh:10.5f}  sw {vs:10.5f}  ratio {ratio:6.3f}")
    else:
        th, ts = pitch_track(sh, fs), pitch_track(ss, fs)
        n = min(len(th), len(ts))
        d = [1200 * np.log2(ts[i][1] / th[i][1]) for i in range(n)]
        print(f"frames {n}  mean {np.mean(d):+.2f} c  sd {np.std(d):.2f} c  max |d| {np.max(np.abs(d)):.2f} c")


def read_csv_pairs(path):
    rows = []
    with open(path) as f:
        for line in f:
            line = line.split("#")[0].strip()
            if not line or line[0].isalpha():
                continue
            a, b = line.split(",")[:2]
            rows.append((float(a), float(b)))
    return np.array(rows)


def cmd_fit_env(a):
    r = read_csv_pairs(a.csv)
    knob, sec = r[:, 0], r[:, 1]
    # model: T = Tmin * (Tmax/Tmin) ** (knob ** taper); grid search taper,
    # linear fit of log T against knob**taper for each.
    best = None
    for taper in np.linspace(0.3, 3.0, 271):
        X = knob ** taper
        p = np.polyfit(X, np.log(sec), 1)
        err = np.sum((np.polyval(p, X) - np.log(sec)) ** 2)
        if best is None or err < best[0]:
            best = (err, taper, p)
    err, taper, p = best
    tmin, tmax = np.exp(p[1]), np.exp(p[1] + p[0])
    print(f"# fit rms log error {np.sqrt(err / len(sec)):.4f}")
    print(f"env.{a.stage}MinSec = {tmin:.6g}\nenv.{a.stage}MaxSec = {tmax:.6g}\nenv.timeTaper = {taper:.4g}")


def cmd_fit_cutoff(a):
    r = read_csv_pairs(a.csv)
    p = np.polyfit(r[:, 0], np.log2(r[:, 1]), 1)
    fmin, fmax = 2 ** p[1], 2 ** (p[1] + p[0])
    resid = np.log2(r[:, 1]) - np.polyval(p, r[:, 0])
    print(f"# exponential fit, residual sd {1200 * np.std(resid):.1f} cents (large = non-exponential taper)")
    print(f"filter.cutoffMinHz = {fmin:.6g}\nfilter.cutoffMaxHz = {fmax:.6g}")


def cmd_fit_polymod(a):
    r = read_csv_pairs(a.csv)
    amt, semis = r[:, 0], r[:, 1]
    best = None
    for taper in np.linspace(0.5, 4.0, 351):
        X = amt ** taper
        rng = np.sum(X * semis) / np.sum(X * X)
        err = np.sum((rng * X - semis) ** 2)
        if best is None or err < best[0]:
            best = (err, taper, rng)
    err, taper, rng = best
    print(f"# rms error {np.sqrt(err / len(amt)):.3f} semitones")
    print(f"pm.amountTaper = {taper:.4g}\npm.{a.dest}RangeSemis = {rng:.5g}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sp = ap.add_subparsers(dest="cmd", required=True)

    def seg(p):
        p.add_argument("--start", type=float, default=0.0)
        p.add_argument("--dur", type=float, default=None)

    p = sp.add_parser("pitch"); p.add_argument("wav"); seg(p); p.set_defaults(fn=cmd_pitch)
    p = sp.add_parser("spectrum"); p.add_argument("wav"); seg(p)
    p.add_argument("--harmonics", type=int, default=32); p.set_defaults(fn=cmd_spectrum)
    p = sp.add_parser("pitchtrack"); p.add_argument("wav"); p.add_argument("--hop", type=float, default=10.0)
    p.set_defaults(fn=cmd_pitchtrack)
    p = sp.add_parser("envelope"); p.add_argument("wav"); seg(p)
    p.add_argument("--stage", choices=["attack-decay", "release"], default="attack-decay")
    p.set_defaults(fn=cmd_envelope)
    p = sp.add_parser("duty"); p.add_argument("wav"); seg(p); p.set_defaults(fn=cmd_duty)
    p = sp.add_parser("noteseq"); p.add_argument("wav"); p.add_argument("--threshold", type=float, default=-40)
    p.set_defaults(fn=cmd_noteseq)
    p = sp.add_parser("selfosc"); p.add_argument("wav"); p.add_argument("--segments", required=True)
    p.set_defaults(fn=cmd_selfosc)
    p = sp.add_parser("compare"); p.add_argument("hw"); p.add_argument("sw"); seg(p)
    p.add_argument("--what", choices=["spectrum", "envelope", "pitchtrack"], default="spectrum")
    p.add_argument("--stage", choices=["attack-decay", "release"], default="attack-decay")
    p.set_defaults(fn=cmd_compare)
    p = sp.add_parser("fit-env"); p.add_argument("csv")
    p.add_argument("--stage", choices=["attack", "decay", "release"], default="attack")
    p.set_defaults(fn=cmd_fit_env)
    p = sp.add_parser("fit-cutoff"); p.add_argument("csv"); p.set_defaults(fn=cmd_fit_cutoff)
    p = sp.add_parser("fit-polymod"); p.add_argument("csv")
    p.add_argument("--dest", choices=["freqA", "filter"], default="freqA"); p.set_defaults(fn=cmd_fit_polymod)
    a = ap.parse_args()
    a.fn(a)


if __name__ == "__main__":
    main()

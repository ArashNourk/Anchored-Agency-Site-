"""Feature extractors. Every extractor maps (signal, fs, spec) -> {name: [values]}.

The same code measures hardware captures and emulator renders, so any
estimator bias cancels in the comparison. Times in specs are seconds after
the aligned scenario start (t = 0 is the first scenario event).
"""
import numpy as np


# ------------------------------------------------------------------ helpers --
def seg(x, fs, t0, dur):
    a = max(0, int(round(t0 * fs)))
    return x[a:a + int(round(dur * fs))]


def bh(n):
    w = np.arange(n) / max(1, n - 1)
    return (0.35875 - 0.48829 * np.cos(2 * np.pi * w) + 0.14128 * np.cos(4 * np.pi * w)
            - 0.01168 * np.cos(6 * np.pi * w))


def spectrum(x, fs, pad=4):
    n = 1 << int(np.ceil(np.log2(len(x) * pad)))
    X = np.abs(np.fft.rfft((x - np.mean(x)) * bh(len(x)), n))
    return X, fs / n


def interp_peak(mag, k):
    """Parabolic interpolation on log magnitude around bin k."""
    if k <= 0 or k >= len(mag) - 1:
        return float(k), float(mag[k])
    a, b, c = np.log(mag[k - 1:k + 2] + 1e-30)
    d = a - 2 * b + c
    off = 0.5 * (a - c) / d if d != 0 else 0.0
    return k + off, float(np.exp(b - 0.25 * (a - c) * off))


def level_db(x):
    return float(20 * np.log10(np.sqrt(np.mean(np.square(x))) + 1e-12))


def nominal_hz(note):
    return 440.0 * 2 ** ((note - 69) / 12)


def f0_near(x, fs, nominal, max_h=8, fmax=9000.0, search_cents=80):
    """f0 from the strongest of the first harmonics, each searched within
    +-search_cents of its nominal frequency. Using a high harmonic divides
    the frequency-resolution error by its number."""
    mag, df = spectrum(x, fs)
    best = None
    for h in range(1, max_h + 1):
        fc = nominal * h
        if fc > fmax:
            break
        lo = int(fc * 2 ** (-search_cents / 1200) / df)
        hi = int(fc * 2 ** (search_cents / 1200) / df) + 1
        if hi >= len(mag) - 1:
            break
        k = lo + int(np.argmax(mag[lo:hi]))
        pos, amp = interp_peak(mag, k)
        score = amp * np.sqrt(h)  # prefer higher harmonics when comparably strong
        if best is None or score > best[0]:
            best = (score, pos * df / h)
    return best[1]


def f0_wide(x, fs, lo_hz, hi_hz):
    """Harmonic-sum f0 over a wide range (for modulated pitch)."""
    mag, df = spectrum(x, fs, pad=2)
    k0, k1 = int(lo_hz / df), int(hi_hz / df) + 1
    ks = np.arange(max(1, k0), k1)
    lm = np.log(mag + 1e-12)
    score = np.zeros(len(ks))
    for h in range(1, 5):
        idx = ks * h
        ok = idx < len(mag)
        score[ok] += lm[idx[ok]] / h
    k = int(ks[int(np.argmax(score))])
    pos, _ = interp_peak(mag, k)
    return pos * df


def rms_env(x, fs, hop_s=0.002, win_s=0.004):
    hop, win = max(1, int(hop_s * fs)), max(2, int(win_s * fs))
    n = max(0, (len(x) - win) // hop)
    if n == 0:
        return np.zeros(0), np.zeros(0)
    idx = np.arange(n)[:, None] * hop + np.arange(win)[None, :]
    e = np.sqrt(np.mean(x[idx] ** 2, axis=1))
    t = (np.arange(n) * hop + win / 2) / fs
    return t, e


def log_tau(t, e, lo_frac, hi_frac, floor=0.0):
    """ln(time constant) of an exponential fall from hi_frac to lo_frac."""
    peak = np.max(e)
    y = e - floor
    k = int(np.argmax(e))
    tt, yy = t[k:], y[k:]
    ok = (yy < hi_frac * (peak - floor)) & (yy > lo_frac * (peak - floor))
    if ok.sum() < 5:
        return float("nan")
    p = np.polyfit(tt[ok], np.log(yy[ok]), 1)
    return float(np.log(-1.0 / p[0])) if p[0] < 0 else float("nan")


# --------------------------------------------------------------- extractors --
def ex_level(x, fs, s):
    return {"level_db": [level_db(seg(x, fs, a, d)) for a, d in s["segments"]]}


def ex_pitch(x, fs, s):
    off = s.get("transpose", 0)
    out = []
    for (a, d), note in zip(s["segments"], s["notes"]):
        nom = nominal_hz(note + off)
        out.append(1200 * np.log2(f0_near(seg(x, fs, a, d), fs, nom) / nom))
    return {"cents": out}


def ex_harmonics(x, fs, s):
    a, d = s["segment"]
    y = seg(x, fs, a, d)
    nom = nominal_hz(s["note"] + s.get("transpose", 0))
    f0 = f0_near(y, fs, nom)
    mag, df = spectrum(y, fs)
    lv = []
    for h in range(1, s["count"] + 1):
        k = int(round(h * f0 / df))
        w = max(2, int(0.01 * h * f0 / df))
        lv.append(20 * np.log10(np.max(mag[k - w:k + w + 1]) + 1e-30))
    return {"harm_db": [v - lv[0] for v in lv[1:]]}


def ex_selfosc(x, fs, s):
    semis, lev = [], []
    for a, d in s["segments"]:
        y = seg(x, fs, a, d)
        mag, df = spectrum(y, fs)
        k = 1 + int(np.argmax(mag[1:int(15000 / df)]))
        pos, _ = interp_peak(mag, k)
        semis.append(12 * np.log2(max(pos * df, 1.0)))
        lev.append(level_db(y))
    return {"freq_semis": semis, "level_db": lev}


def ex_level_floor(x, fs, s):
    floor = s.get("floor_db", -90.0)
    return {"level_db": [max(floor, level_db(seg(x, fs, a, d))) for a, d in s["segments"]]}


def ex_env_attack(x, fs, s):
    """ln of the 10-90 % rise time: independent of the alignment offset."""
    out = []
    for a in s["onsets"]:
        t, e = rms_env(seg(x, fs, a - 0.05, s["dur"]), fs, 0.0005, 0.003)
        k = int(np.argmax(e))
        t10 = t[np.nonzero(e[:k + 1] >= 0.1 * e[k])[0][0]]
        t90 = t[np.nonzero(e[:k + 1] >= 0.9 * e[k])[0][0]]
        out.append(float(np.log(max(t90 - t10, 1e-4))))
    return {"ln_rise": out}


def ex_env_decay(x, fs, s):
    out = []
    for a in s["onsets"]:
        t, e = rms_env(seg(x, fs, a, s["dur"]), fs, 0.002, 0.006)
        out.append(log_tau(t, e, 0.05, 0.7))
    return {"ln_tau": out}


def ex_env_release(x, fs, s):
    out = []
    for a in s["offsets"]:
        t, e = rms_env(seg(x, fs, a, s["dur"]), fs, 0.002, 0.006)
        out.append(log_tau(t, e, 0.02, 0.7))
    return {"ln_tau": out}


def pitch_trace(y, fs, nominal, span_semis, hop=0.005, win=0.04):
    lo, hi = nominal * 2 ** (-span_semis / 12), nominal * 2 ** (span_semis / 12)
    w, h = int(win * fs), int(hop * fs)
    tr = [f0_wide(y[i:i + w], fs, lo, hi) for i in range(0, len(y) - w, h)]
    return np.array([12 * np.log2(f / nominal) for f in tr]), hop


def ex_mod(x, fs, s):
    nom = nominal_hz(s["note"])
    pp, rate = [], []
    for a, d in s["segments"]:
        tr, hop = pitch_trace(seg(x, fs, a, d), fs, nom, s.get("span", 16))
        pp.append(float(np.percentile(tr, 98) - np.percentile(tr, 2)))
        z = tr - np.mean(tr)
        ac = np.correlate(z, z, "full")[len(z) - 1:]
        ac /= ac[0] + 1e-12
        # first maximum after the first zero crossing
        zc = np.nonzero(ac < 0)[0]
        if len(zc) == 0:
            rate.append(float("nan"))
            continue
        k = zc[0] + int(np.argmax(ac[zc[0]:]))
        if 0 < k < len(ac) - 1:
            pos, _ = interp_peak(np.exp(ac + 2), k)  # positive surface for log-parabola
        else:
            pos = k
        rate.append(float(np.log(1.0 / (pos * hop))))
    return {"pp_semis": pp, "ln_rate": rate}


def ex_fingerprint(x, fs, s):
    nv = s["voices"]
    cents = [[] for _ in range(nv)]
    lev = [[] for _ in range(nv)]
    tau = [[] for _ in range(nv)]
    for i, (a, note) in enumerate(zip(s["onsets"], s["notes"])):
        v = i % nv
        y = seg(x, fs, a, s["dur"])
        nom = nominal_hz(note)
        cents[v].append(1200 * np.log2(f0_near(y[int(0.01 * fs):int(0.12 * fs)], fs, nom) / nom))
        lev[v].append(level_db(y[int(0.005 * fs):int(0.03 * fs)]))
        t, e = rms_env(y, fs, 0.002, 0.006)
        tau[v].append(log_tau(t, e, 0.1, 0.7))
    m = lambda arr: [float(np.nanmean(a)) for a in arr]
    return {"cents": m(cents), "level_db": m(lev), "ln_decay": m(tau)}


def ex_centroid(x, fs, s):
    a, d, hop = s["start"], s["dur"], s["hop"]
    out = []
    for i in range(int(d / hop)):
        y = seg(x, fs, a + i * hop, hop)
        mag, df = spectrum(y, fs, pad=1)
        f = np.arange(len(mag)) * df
        out.append(float(np.log2(np.sum(mag * f) / (np.sum(mag) + 1e-30) + 1e-9)))
    return {"log2_centroid": out}


def ex_onset_sweep(x, fs, s):
    """Knob position at which the level first exceeds `threshold_db` during a
    linear parameter ramp, plus the level at the end of the hold after it."""
    knobs, holds = [], []
    for r in s["ramps"]:
        t0, t1, v0, v1 = r["t0"], r["t1"], r["v0"], r["v1"]
        t, e = rms_env(seg(x, fs, t0, t1 - t0), fs, 0.005, 0.02)
        db = 20 * np.log10(e + 1e-12)
        hit = np.nonzero(db > s["threshold_db"])[0]
        if len(hit) == 0:
            knobs.append(v1 + 0.05)
        else:
            k = hit[0]
            if k > 0:  # interpolate the crossing between frames
                f = (s["threshold_db"] - db[k - 1]) / (db[k] - db[k - 1])
                tc = t[k - 1] + f * (t[k] - t[k - 1])
            else:
                tc = t[0]
            knobs.append(v0 + tc / (t1 - t0) * (v1 - v0))
        holds.append(level_db(seg(x, fs, r["hold"][0], r["hold"][1])))
    return {"onset_knob": knobs, "level_db": holds}


def ex_fp_selfosc(x, fs, s):
    """Per-voice self-oscillation frequency (semitones) minus the key offset."""
    out = []
    w0, wd = s["window"]
    for a, note in zip(s["onsets"], s["notes"]):
        y = seg(x, fs, a + w0, wd)
        mag, df = spectrum(y, fs)
        k = 1 + int(np.argmax(mag[1:int(15000 / df)]))
        pos, _ = interp_peak(mag, k)
        out.append(12 * np.log2(max(pos * df, 1.0)) - (note - 60))
    return {"cutoff_semis": out}


EXTRACTORS = {
    "fp_selfosc": ex_fp_selfosc,
    "onset_sweep": ex_onset_sweep,
    "level": ex_level, "pitch": ex_pitch, "harmonics": ex_harmonics, "selfosc": ex_selfosc,
    "level_floor": ex_level_floor, "env_attack": ex_env_attack, "env_decay": ex_env_decay,
    "env_release": ex_env_release, "mod": ex_mod, "fingerprint": ex_fingerprint, "centroid": ex_centroid,
}


def extract(test, x, fs):
    """All features of a test -> {"<extractor-id>.<feature>": [floats]}."""
    out = {}
    for ex in test["extract"]:
        for k, v in EXTRACTORS[ex["type"]](x, fs, ex).items():
            out[f'{ex["id"]}.{k}'] = [float(u) for u in v]
    return out


# ---------------------------------------------------------------- alignment --
def align_offset(capture, reference, fs, max_lag_s=3.0):
    """Delay of `capture` relative to `reference` (seconds), by cross-
    correlating log-envelopes. Returns (offset_s, confidence 0..1)."""
    def env(z):
        _, e = rms_env(z, fs, 0.005, 0.01)
        e = np.log10(e + 1e-5)
        return e - np.mean(e)
    a, b = env(capture), env(reference)
    n = len(a) + len(b)
    nfft = 1 << int(np.ceil(np.log2(n)))
    c = np.fft.irfft(np.fft.rfft(a, nfft) * np.conj(np.fft.rfft(b, nfft)), nfft)
    lags = np.concatenate([np.arange(0, nfft // 2), np.arange(-nfft // 2, 0)])
    maxlag = int(max_lag_s / 0.005)
    ok = np.abs(lags) <= maxlag
    k = np.argmax(np.where(ok, c, -np.inf))
    conf = float(c[k] / (np.sqrt(np.sum(a * a) * np.sum(b * b)) + 1e-12))
    # refine to sample accuracy with the raw signals around the coarse lag
    coarse = int(lags[k] * 0.005 * fs)
    w = int(0.01 * fs)
    best, best_l = -np.inf, coarse
    ra = np.abs(reference[: int(4 * fs)])
    for l in range(coarse - w, coarse + w + 1, max(1, w // 40)):
        if l < 0 or l + len(ra) > len(capture):
            continue
        v = float(np.dot(np.abs(capture[l:l + len(ra)]), ra))
        if v > best:
            best, best_l = v, l
    return best_l / fs, conf


def onset_time(x, fs, start=0.0, search=1.0, frac=0.2):
    """First time after `start` where |x| exceeds `frac` of the peak found in
    the following `search` seconds. Used on the sharp-attack C01 note."""
    a = max(0, int(start * fs))
    y = np.abs(x[a:a + int(search * fs)])
    if len(y) == 0 or np.max(y) <= 0:
        return float("nan")
    return (a + int(np.argmax(y > frac * np.max(y)))) / fs

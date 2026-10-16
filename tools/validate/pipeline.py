"""Measure a capture session, compare against the emulator, fit calibration.

Session layout (one hardware unit, one sitting):
    <session>/session.json            unit, firmware, temperature, chain, ...
    <session>/<test id>/take<N>.wav   recordings, 48 kHz, any pre-roll
"""
import glob
import json
import math
import os
from concurrent.futures import ThreadPoolExecutor

import numpy as np

from . import corpus, emulator, features, wavio

FS = 48000


# ---------------------------------------------------------------- measuring --
def load_session(session):
    with open(os.path.join(session, "session.json")) as f:
        meta = json.load(f)
    takes = {}
    for t in corpus.TESTS:
        files = sorted(glob.glob(os.path.join(session, t["id"], "take*.wav")))
        if files:
            takes[t["id"]] = files
    return meta, takes


def _preroll(path):
    side = path[:-4] + ".json"
    if os.path.exists(side):
        return json.load(open(side)).get("preroll_s")
    return None


def measure_session(session, log=print):
    """Locate each take on the scenario's time axis, normalise level with the
    C01 reference, extract features, and average takes.

    Timing: the capture tool stores the scheduled MIDI start (`preroll_s`)
    next to every take. The audio/MIDI path adds one constant latency per
    session, measured once from the sharp onset of the C01 note. Takes
    without timing data fall back to envelope cross-correlation against the
    emulator (less accurate where the model is still wrong).
    Returns {test_id: {"mean", "sd", "takes", "align"}}."""
    meta, takes = load_session(session)
    results, latency = {}, None
    order = sorted(takes, key=lambda tid: 0 if corpus.test(tid).get("role") == "gain_reference" else 1)
    for tid in order:
        t = corpus.test(tid)
        ref, fs_ref = emulator.render(corpus.scenario_path(t))
        per_take, aligns = [], []
        for path in takes[tid]:
            x, fs = wavio.read(path)
            if fs != FS or fs_ref != FS:
                raise ValueError(f"{path}: captures must be {FS} Hz (got {fs})")
            pre = _preroll(path)
            if t.get("role") == "gain_reference" and pre is not None:
                lat = features.onset_time(x, fs, pre - 0.05) - pre - features.onset_time(ref, fs, 0.0)
                latency = lat if latency is None else latency
            if pre is not None and latency is not None:
                off, how = pre + latency, "schedule"
            else:
                off, conf = features.align_offset(x, ref, fs)
                how = f"xcorr {conf:.2f}"
            aligns.append({"file": os.path.basename(path), "offset_s": round(off, 6), "method": how})
            y = x[max(0, int(round(off * fs))):]
            per_take.append(features.extract(t, y, fs))
        mean = {k: list(np.nanmean([p[k] for p in per_take], axis=0)) for k in per_take[0]}
        sd = {k: list(np.nanstd([p[k] for p in per_take], axis=0, ddof=1) if len(per_take) > 1 else
                      np.zeros(len(per_take[0][k]))) for k in per_take[0]}
        results[tid] = {"mean": mean, "sd": sd, "takes": len(per_take), "align": aligns}
        if t.get("role") == "gain_reference" and latency is not None:
            results[tid]["latency_s"] = latency
            log(f"  session latency {1000 * latency:.2f} ms (from the C01 onset)")
        log(f"  {tid} {t['name']}: {len(per_take)} take(s), timing: {aligns[0]['method']}")
    return results


# ------------------------------------------------------------ level scaling --
FLOOR_DB = -89.99


def session_gain(hw, overrides):
    """Capture-chain gain: emulator C01 level minus hardware C01 level, for
    the calibration being evaluated (so voice-1 level changes stay consistent)."""
    ref = next(t for t in corpus.TESTS if t.get("role") == "gain_reference")
    if ref["id"] not in hw:
        return 0.0
    emu = emu_features(ref["id"], overrides)
    return emu["ref.level_db"][0] - hw[ref["id"]]["mean"]["ref.level_db"][0]


def normalised(hw, gain_db):
    """Hardware features with every *.level_db value moved by the session gain
    (values at the measurement floor stay at the floor)."""
    out = {}
    for tid, r in hw.items():
        mean = {k: ([v if v <= FLOOR_DB else v + gain_db for v in vals] if k.endswith("level_db") else vals)
                for k, vals in r["mean"].items()}
        out[tid] = dict(r, mean=mean)
    return out


# --------------------------------------------------------------- comparison --
def cyclic_shift(test, hw):
    """Rotation of the hardware's strike order that puts the voice used by
    unison-1 (the anchor) at index 0, as in the emulator."""
    cyc = test.get("cyclic")
    if not cyc:
        return 0
    feats = cyc.get("match", cyc["features"])
    anchor = cyc["anchor"]
    n = len(hw[feats[0]])
    best, best_s = None, 0
    for s in range(n):
        d = 0.0
        for f in feats:
            key = anchor + "." + f.split(".", 1)[1]
            tol = test["tolerance"][f]["abs"]
            d += ((hw[f][s] - hw[key][0]) / tol) ** 2
        if best is None or d < best:
            best, best_s = d, s
    return best_s


def roll(test, values, s):
    cyc = test.get("cyclic")
    if not cyc or s == 0:
        return values
    out = dict(values)
    for f in cyc["features"]:
        v = values[f]
        out[f] = v[s:] + v[:s]
    return out


def compare(test, hw, emu, keys=None):
    """Element-wise residuals. hw = {"mean", "sd"}; emu = feature dict."""
    s = cyclic_shift(test, hw["mean"])
    hm, hs = roll(test, hw["mean"], s), roll(test, hw["sd"], s)
    rows = []
    for k, tol in test["tolerance"].items():
        if keys and k not in keys:
            continue
        for i, (h, e) in enumerate(zip(hm[k], emu[k])):
            sd = hs[k][i] if i < len(hs[k]) else 0.0
            base = tol.get("abs", 0.0) + tol.get("rel", 0.0) * abs(h)
            eff = max(base, 2.0 * (sd if np.isfinite(sd) else 0.0))
            r = e - h
            ok = np.isfinite(r) and abs(r) <= eff
            rows.append({"feature": k, "index": i, "hw": h, "hw_sd": sd, "emu": e, "residual": r,
                         "tolerance": eff, "norm": (r / eff) if np.isfinite(r) else float("nan"), "pass": bool(ok)})
    return rows, s


def emu_features(tid, overrides):
    t = corpus.test(tid)
    x, fs = emulator.render(corpus.scenario_path(t), overrides)
    return features.extract(t, x, fs)


# ------------------------------------------------------------------ fitting --
def _to_x(p, u):
    return p["lo"] * (p["hi"] / p["lo"]) ** u if p.get("log") else p["lo"] + u * (p["hi"] - p["lo"])


def _to_u(p, x):
    x = min(max(x, p["lo"]), p["hi"])
    return math.log(x / p["lo"]) / math.log(p["hi"] / p["lo"]) if p.get("log") else (x - p["lo"]) / (p["hi"] - p["lo"])


def objective(group, hw, overrides):
    hw = normalised(hw, session_gain(hw, overrides))
    total = 0.0
    for tid in group["tests"]:
        emu = emu_features(tid, overrides)
        keys = [k.split(".", 1)[1] for k in group["features"] if k.startswith(tid + ".")]
        rows, _ = compare(corpus.test(tid), hw[tid], emu, keys)
        for r in rows:
            total += r["norm"] ** 2 if np.isfinite(r["norm"]) else 1e4
    return total


def nelder_mead(f, u0, step=0.15, iters=120, tol=1e-4, pool=None):
    """Bounded Nelder-Mead on the unit cube."""
    n = len(u0)
    clip = lambda u: np.clip(u, 0.0, 1.0)
    pts = [clip(np.array(u0, float))]
    for i in range(n):
        p = np.array(u0, float)
        p[i] = p[i] + step if p[i] + step <= 1 else p[i] - step
        pts.append(clip(p))
    vals = list(pool.map(f, pts)) if pool else [f(p) for p in pts]
    for _ in range(iters):
        order = np.argsort(vals)
        pts, vals = [pts[i] for i in order], [vals[i] for i in order]
        if abs(vals[-1] - vals[0]) < tol * (1 + abs(vals[0])) and np.max(np.abs(np.array(pts) - pts[0])) < 1e-3:
            break
        c = np.mean(pts[:-1], axis=0)
        xr = clip(c + (c - pts[-1])); fr = f(xr)
        if fr < vals[0]:
            xe = clip(c + 2 * (c - pts[-1])); fe = f(xe)
            pts[-1], vals[-1] = (xe, fe) if fe < fr else (xr, fr)
        elif fr < vals[-2]:
            pts[-1], vals[-1] = xr, fr
        else:
            xc = clip(c + 0.5 * (pts[-1] - c)); fc = f(xc)
            if fc < vals[-1]:
                pts[-1], vals[-1] = xc, fc
            else:
                new = [pts[0]] + [clip(pts[0] + 0.5 * (p - pts[0])) for p in pts[1:]]
                newv = list(pool.map(f, new[1:])) if pool else [f(p) for p in new[1:]]
                pts, vals = new, [vals[0]] + newv
    k = int(np.argmin(vals))
    return pts[k], vals[k]


def fit_voice_profile(group, hw0, overrides, iters=3, log=print):
    t = corpus.test(group["test"])
    o = dict(overrides)
    for it in range(iters):
        hw = normalised(hw0, session_gain(hw0, o))
        emu = emu_features(t["id"], o)
        s = cyclic_shift(t, hw[t["id"]]["mean"])
        h = roll(t, hw[t["id"]]["mean"], s)
        dc = np.array(h["fp.cents"]) - np.array(emu["fp.cents"])
        dl = np.array(h["fp.level_db"]) - np.array(emu["fp.level_db"])
        dd = np.array(h["fp.ln_decay"]) - np.array(emu["fp.ln_decay"])
        dk = np.array(h["fpc.cutoff_semis"]) - np.array(emu["fpc.cutoff_semis"])
        db = np.array(h["fpb.cents"]) - np.array(emu["fpb.cents"])
        dl -= np.nanmean(dl)   # global level belongs to the session gain
        dd -= np.nanmean(dd)   # global envelope time belongs to the envelope fit
        dk -= np.nanmean(dk)   # global cutoff belongs to the cutoff-map fit
        for v in range(len(dc)):
            for key, d in (("tuneCentsA", dc[v]), ("tuneCentsB", db[v]), ("vcaGainDb", dl[v]),
                           ("ampEnvTimeLn", dd[v]), ("cutoffSemis", dk[v])):
                if np.isfinite(d):
                    k = f"unit.v{v + 1}.{key}"
                    o[k] = o.get(k, 0.0) + float(d)
        log(f"    iteration {it + 1}: rotation {s}, max |pitch error| {np.nanmax(np.abs(dc)):.3f} c, "
            f"max |level error| {np.nanmax(np.abs(dl)):.3f} dB, max |decay error| {np.nanmax(np.abs(dd)):.4f}, "
            f"max |cutoff error| {np.nanmax(np.abs(dk)):.3f} st, max |Osc B pitch error| {np.nanmax(np.abs(db)):.3f} c")
    return o


# Sum of squared tolerance-normalised residuals below which a group counts as
# fitted (every residual well inside its tolerance).
CONVERGED = 0.05


def fit(hw, overrides=None, groups=None, passes=2, restarts=2, log=print):
    """Fit calibration groups in dependency order. Groups are coupled (e.g.
    the HF tracking constant moves every voice's pitch, the envelope ranges
    move every voice's decay), so the whole sequence runs `passes` times, each
    pass starting from the previous result. Each Nelder-Mead search restarts
    `restarts` times from its best point to escape shallow local minima."""
    defaults = emulator.defaults()
    o = dict(overrides or {})
    with ThreadPoolExecutor(max_workers=os.cpu_count() or 2) as pool:
        for pn in range(passes):
            log(f"  pass {pn + 1} of {passes}")
            for g in corpus.FIT_GROUPS:
                if groups and g["name"] not in groups:
                    continue
                if g.get("method") == "voice_profile":
                    if g["test"] not in hw:
                        continue
                    log(f"  fitting {g['name']} (per-voice offsets)")
                    o = fit_voice_profile(g, hw, o, log=log)
                    continue
                if any(t not in hw for t in g["tests"]):
                    log(f"  skipping {g['name']}: no captures for {g['tests']}")
                    continue
                ps = g["params"]

                def f(u, ps=ps, g=g):
                    trial = dict(o)
                    for p, ui in zip(ps, u):
                        trial[p["key"]] = _to_x(p, float(ui))
                    return objective(g, hw, trial)

                u = [_to_u(p, o.get(p["key"], defaults[p["key"]])) for p in ps]
                before = f(np.array(u))
                val = before
                if before < CONVERGED:
                    log(f"  {g['name']}: already within tolerance (cost {before:.3f}), unchanged")
                    continue
                for r in range(restarts):
                    u, val = nelder_mead(f, u, step=0.15 / (r + 1), iters=40 * len(ps), pool=pool)
                    if val < CONVERGED:
                        break
                for p, ui in zip(ps, u):
                    o[p["key"]] = _to_x(p, float(ui))
                vals = ", ".join(f"{p['key']}={o[p['key']]:.5g}" for p in ps)
                log(f"  fitting {g['name']}: cost {before:.2f} -> {val:.2f}  ({vals})")
    return o


# ------------------------------------------------------------------- report --
def report(hw, overrides, title, meta):
    """Residual report for every captured test. Returns (markdown, data)."""
    gain = session_gain(hw, overrides)
    hw = normalised(hw, gain)
    data, lines = [], []
    npass = ntot = 0
    for t in corpus.TESTS:
        if t["id"] not in hw or t.get("role") == "gain_reference":
            continue
        emu = emu_features(t["id"], overrides)
        rows, s = compare(t, hw[t["id"]], emu)
        p = sum(r["pass"] for r in rows)
        npass += p
        ntot += len(rows)
        data.append({"test": t["id"], "name": t["name"], "rotation": s, "rows": rows})
        worst = max(rows, key=lambda r: abs(r["norm"]) if np.isfinite(r["norm"]) else 1e9)
        lines.append(f"| {t['id']} | {t['name']} | {p}/{len(rows)} | {worst['feature']}[{worst['index']}] "
                     f"| {worst['residual']:+.4g} | {worst['tolerance']:.3g} | {'pass' if p == len(rows) else 'FAIL'} |")
    head = [f"# {title}", "",
            f"Unit: {meta.get('unit', '?')} · session: {meta.get('session', '?')} · "
            f"firmware: {meta.get('firmware', '?')} · synthetic: {meta.get('synthetic', False)}", "",
            f"**{npass} of {ntot} features within tolerance ({100.0 * npass / max(1, ntot):.1f} %).**", "",
            "Tolerance per feature = max(acceptance target, 2 × hardware take-to-take SD). "
            f"Session gain {gain:+.2f} dB (level reference C01, not counted).", "",
            "| Test | Behaviour | Within tolerance | Worst feature | Residual | Tolerance | Status |",
            "|---|---|---|---|---|---|---|"]
    detail = ["", "## Feature detail", ""]
    for d in data:
        detail += [f"### {d['test']} {d['name']}" + (f" (voice rotation {d['rotation']})" if d["rotation"] else ""), "",
                   "| Feature | i | Hardware | HW SD | Emulator | Residual | Tol | |", "|---|---|---|---|---|---|---|---|"]
        for r in d["rows"]:
            detail.append(f"| {r['feature']} | {r['index']} | {r['hw']:.4g} | {r['hw_sd']:.2g} | {r['emu']:.4g} | "
                          f"{r['residual']:+.3g} | {r['tolerance']:.3g} | {'✓' if r['pass'] else '✗'} |")
        detail.append("")
    summary = {"features": ntot, "passed": npass, "tests": data}
    return "\n".join(head + lines + detail), summary

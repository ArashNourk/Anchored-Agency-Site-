"""Hardware capture: plays each corpus scenario into a Prophet-10 Rev4 over
MIDI while recording its output, so stimulus and response share one clock.

Requirements on the capture machine: `pip install python-rtmidi sounddevice`,
an audio interface at 48 kHz, and a MIDI interface. Panel state is sent as
NRPN through a parameter map (validation/hw_map.json): the map is NOT filled
in here, because the instrument's NRPN numbers and value ranges must be taken
from its own documentation (assumption A-MAP-01). Tests whose scenarios change
parameters during the take need the map; with an incomplete map the tool
prints a setup sheet for the operator and runs only static tests.
"""
import datetime
import json
import os
import time

import numpy as np

from . import corpus, wavio

FS = 48000
MAP_PATH = os.path.join(corpus.ROOT, "validation", "hw_map.json")


def parse_scenario(path):
    static, timed, events, length = [], [], [], 2.0
    with open(path) as f:
        for line in f:
            a = line.split("#")[0].split()
            if not a:
                continue
            if a[0] == "set":
                static.append((a[1], float(a[2])))
            elif a[0] == "ramp":
                # sent as parameter steps every 20 ms (p10render uses 1 ms; both are
                # smoothed by the instrument's own control smoothing)
                t0, t1, key, v0, v1 = float(a[1]), float(a[2]), a[3], float(a[4]), float(a[5])
                n = max(1, int((t1 - t0) / 0.02))
                for i in range(n + 1):
                    timed.append((t0 + (t1 - t0) * i / n, key, v0 + (v1 - v0) * i / n))
            elif a[0] == "length":
                length = float(a[1])
            elif a[0] == "at":
                t, cmd = float(a[1]), a[2]
                if cmd == "set":
                    timed.append((t, a[3], float(a[4])))
                elif cmd == "on":
                    events.append((t, [0x90, int(a[3]), int(a[4])]))
                elif cmd == "off":
                    events.append((t, [0x80, int(a[3]), 64]))
                elif cmd == "cc":
                    events.append((t, [0xB0, int(a[3]), int(a[4])]))
                elif cmd == "pressure":
                    events.append((t, [0xD0, int(float(a[3]) * 127)]))
                elif cmd == "bend":
                    v = max(0, min(16383, int(8192 + float(a[3]) * 8191)))
                    events.append((t, [0xE0, v & 127, v >> 7]))
    return static, timed, sorted(events, key=lambda e: e[0]), length


def write_map_template(path=MAP_PATH):
    """Every engine parameter, with empty NRPN number and hardware range."""
    keys = []
    for layer in "AB":
        for p in _layer_keys():
            keys.append(f"{layer}.{p}")
    keys += ["P.layer_mode", "P.split_point", "P.split_upper_b", "P.layer_balance"]
    data = {"_doc": "Fill nrpn (number) and max (hardware value for knob position 1.0) from the "
                    "Prophet-10 Rev4 documentation. Leave null when the parameter has no NRPN.",
            "params": {k: {"nrpn": None, "max": None} for k in keys}}
    with open(path, "w") as f:
        json.dump(data, f, indent=1)
    return path


def _layer_keys():
    import re
    src = open(os.path.join(corpus.ROOT, "engine", "model", "Params.cpp")).read()
    block = src[src.index("kLayer[kNumLayerParams]"):src.index("kProgram[kNumProgramParams]")]
    return re.findall(r'\{"([a-z0-9_]+)",', block)


def _nrpn_msgs(ch, num, value):
    return [[0xB0 | ch, 99, num >> 7], [0xB0 | ch, 98, num & 127],
            [0xB0 | ch, 6, (value >> 7) & 127], [0xB0 | ch, 38, value & 127]]


def _param_msgs(pmap, ch, key, value):
    e = pmap.get(key)
    if not e or e.get("nrpn") is None or e.get("max") is None:
        return None
    return _nrpn_msgs(ch, int(e["nrpn"]), int(round(value * e["max"])) if e["max"] > 2 else int(round(value)))


def capture(unit, session, out_root, tests=None, takes=3, midi_port=None, audio_device=None, channel=1, meta=None):
    try:
        import rtmidi
        import sounddevice as sd
    except ImportError:
        raise SystemExit("capture needs: pip install python-rtmidi sounddevice")
    pmap = {}
    if os.path.exists(MAP_PATH):
        pmap = json.load(open(MAP_PATH))["params"]
    ch = channel - 1
    mo = rtmidi.MidiOut()
    ports = mo.get_ports()
    idx = next((i for i, n in enumerate(ports) if midi_port and midi_port in n), 0 if ports else None)
    if idx is None:
        raise SystemExit("no MIDI output port")
    mo.open_port(idx)
    sdir = os.path.join(out_root, unit, session)
    os.makedirs(sdir, exist_ok=True)
    info = {"unit": unit, "session": session, "date": datetime.datetime.now().isoformat(timespec="seconds"),
            "sample_rate": FS, "midi_port": ports[idx], "audio_device": str(audio_device), "synthetic": False}
    info.update(meta or {})
    json.dump(info, open(os.path.join(sdir, "session.json"), "w"), indent=1)
    for t in corpus.TESTS:
        if tests and t["id"] not in tests:
            continue
        static, timed, events, length = parse_scenario(corpus.scenario_path(t))
        unmapped_timed = [k for _, k, _ in timed if _param_msgs(pmap, ch, k, 0) is None]
        if unmapped_timed:
            print(f"{t['id']}: skipped, needs NRPN map entries for {sorted(set(unmapped_timed))}")
            continue
        sheet = [(k, v) for k, v in static if _param_msgs(pmap, ch, k, v) is None]
        print(f"\n{t['id']} {t['name']}")
        if sheet:
            print("  Load the basic (init) program, then set by hand:")
            for k, v in sheet:
                print(f"    {k} = {v}")
            input("  Press Enter when the panel is set...")
        for k, v in static:
            for m in _param_msgs(pmap, ch, k, v) or []:
                mo.send_message(m)
        sched = sorted([(tt, m) for tt, m in events] +
                       [(tt, mm) for tt, k, v in timed for mm in _param_msgs(pmap, ch, k, v)], key=lambda e: e[0])
        os.makedirs(os.path.join(sdir, t["id"]), exist_ok=True)
        for n in range(1, takes + 1):
            pre = 0.5
            rec = sd.rec(int((length + pre + 1.0) * FS), samplerate=FS, channels=1, device=audio_device, dtype="float32")
            t0 = time.perf_counter() + pre
            for tt, m in sched:
                while time.perf_counter() < t0 + tt:
                    time.sleep(0.0005)
                mo.send_message(m if (m[0] & 0x0F) else [m[0] | ch] + m[1:])
            sd.wait()
            for note in range(128):
                mo.send_message([0x80 | ch, note, 0])
            path = os.path.join(sdir, t["id"], f"take{n}.wav")
            wavio.write(path, np.asarray(rec[:, 0]), FS)
            # scheduled start of the scenario relative to the recording start
            json.dump({"preroll_s": pre}, open(path[:-4] + ".json", "w"))
            print(f"  take {n}: {path}  peak {float(np.max(np.abs(rec))):.3f}")
            time.sleep(1.0)
    return sdir

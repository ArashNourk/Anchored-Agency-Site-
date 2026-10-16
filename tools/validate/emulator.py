"""Runs the emulator (build/p10render) on a scenario with calibration overrides."""
import hashlib
import json
import os
import subprocess
import tempfile
import threading

from . import wavio
from .corpus import ROOT

RENDER = os.environ.get("P10RENDER", os.path.join(ROOT, "build", "p10render"))
CACHE = os.path.join(tempfile.gettempdir(), "p10val-cache")


def defaults():
    """All calibration keys and their built-in default values."""
    out = subprocess.run([RENDER, "--dump-calibration"], check=True, capture_output=True, text=True).stdout
    vals = {}
    for line in out.splitlines():
        line = line.split("#")[0].strip()
        if "=" in line:
            k, v = line.split("=", 1)
            vals[k.strip()] = float(v)
    return vals


def write_calibration(path, overrides):
    with open(path, "w") as f:
        f.write("# fitted by tools/validate; keys not listed keep their defaults\n")
        for k in sorted(overrides):
            f.write(f"{k} = {overrides[k]:.10g}\n")


def render(scenario, overrides=None, serial=None):
    """Render a scenario; returns (mono float64 array, fs). Results are cached."""
    overrides = overrides or {}
    with open(scenario) as f:
        text = f.read()
    key = hashlib.sha1(json.dumps([text, sorted(overrides.items()), serial, os.path.getmtime(RENDER)]).encode()).hexdigest()
    os.makedirs(CACHE, exist_ok=True)
    out = os.path.join(CACHE, key + ".wav")
    if not os.path.exists(out):
        head = []
        if serial is not None:
            head.append(f"serial {serial}")
        if overrides:
            cal = os.path.join(CACHE, key + ".cal")
            write_calibration(cal, overrides)
            head.append(f"calibration {cal}")
        scn = os.path.join(CACHE, f"{key}.{threading.get_ident()}.scn")
        with open(scn, "w") as f:
            f.write("\n".join(head) + "\n" + text)
        tmp = f"{out}.{os.getpid()}.{threading.get_ident()}.wav"
        r = subprocess.run([RENDER, scn, tmp], capture_output=True, text=True)
        if r.returncode != 0:
            raise RuntimeError(f"p10render failed on {scenario}: {r.stderr}")
        os.replace(tmp, out)
    return wavio.read(out)

"""python3 -m tools.validate <command>

  corpus                         print the test corpus and fit groups as JSON
  map-template                   write validation/hw_map.json (NRPN map to fill in)
  capture  --unit U --session S  record the corpus from the hardware
  measure  <session>             extract features -> <session>/features.json
  fit      <session> [--groups g1,g2] [--out file.cal]
  report   <session> [--calibration file.cal]   residual report (markdown + json)
  selftest                       synthetic-hardware end-to-end test
"""
import argparse
import json
import os
import sys

from . import capture, corpus, emulator, pipeline, selftest

CAPTURES = os.path.join(corpus.ROOT, "validation", "captures")


def load_features(session):
    path = os.path.join(session, "features.json")
    if not os.path.exists(path):
        print("measuring first...")
        save_features(session)
    return json.load(open(path))


def save_features(session):
    hw = pipeline.measure_session(session)
    json.dump(hw, open(os.path.join(session, "features.json"), "w"), indent=1, default=float)
    return hw


def read_cal(path):
    o = {}
    for line in open(path):
        line = line.split("#")[0]
        if "=" in line:
            k, v = line.split("=", 1)
            o[k.strip()] = float(v)
    return o


def main(argv=None):
    ap = argparse.ArgumentParser(prog="python3 -m tools.validate", description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sp = ap.add_subparsers(dest="cmd", required=True)
    sp.add_parser("corpus")
    sp.add_parser("map-template")
    c = sp.add_parser("capture")
    c.add_argument("--unit", required=True); c.add_argument("--session", required=True)
    c.add_argument("--tests"); c.add_argument("--takes", type=int, default=3)
    c.add_argument("--midi-port"); c.add_argument("--audio-device"); c.add_argument("--channel", type=int, default=1)
    c.add_argument("--firmware"); c.add_argument("--temperature-c", type=float); c.add_argument("--warmup-min", type=float)
    m = sp.add_parser("measure"); m.add_argument("session")
    f = sp.add_parser("fit"); f.add_argument("session"); f.add_argument("--groups"); f.add_argument("--out")
    f.add_argument("--start", help="calibration file to start from")
    r = sp.add_parser("report"); r.add_argument("session"); r.add_argument("--calibration")
    st = sp.add_parser("selftest"); st.add_argument("--root", default=CAPTURES)
    a = ap.parse_args(argv)

    if a.cmd == "corpus":
        json.dump({"tests": corpus.TESTS, "fit_groups": corpus.FIT_GROUPS}, sys.stdout, indent=1)
    elif a.cmd == "map-template":
        print("wrote", capture.write_map_template())
    elif a.cmd == "capture":
        meta = {"firmware": a.firmware, "temperature_c": a.temperature_c, "warmup_min": a.warmup_min}
        capture.capture(a.unit, a.session, CAPTURES, a.tests.split(",") if a.tests else None, a.takes,
                        a.midi_port, a.audio_device, a.channel, meta)
    elif a.cmd == "measure":
        save_features(a.session)
    elif a.cmd == "fit":
        hw = load_features(a.session)
        start = read_cal(a.start) if a.start else {}
        o = pipeline.fit(hw, start, a.groups.split(",") if a.groups else None)
        out = a.out or os.path.join(a.session, "fitted.cal")
        emulator.write_calibration(out, o)
        print("wrote", out)
    elif a.cmd == "report":
        hw = load_features(a.session)
        o = read_cal(a.calibration) if a.calibration else {}
        meta = json.load(open(os.path.join(a.session, "session.json")))
        md, data = pipeline.report(hw, o, "Prophet-10 Rev4 model residuals", meta)
        open(os.path.join(a.session, "report.md"), "w").write(md)
        json.dump(data, open(os.path.join(a.session, "report.json"), "w"), indent=1, default=float)
        print(md.split("\n\n")[2])
    elif a.cmd == "selftest":
        s, worst = selftest.run(a.root)
        ok = s["passed"] == s["features"] and worst < 0.05
        print("SELFTEST", "PASSED" if ok else "FAILED")
        return 0 if ok else 1
    return 0


if __name__ == "__main__":
    sys.exit(main())

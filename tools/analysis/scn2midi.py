#!/usr/bin/env python3
"""Convert a validation scenario (.scn) into the hardware side of the test:

  * a Standard MIDI File with the scenario's performance events (notes,
    velocities, CCs, pitch bend, channel pressure) on one channel, to be
    played into the Prophet-10 Rev4 while its output is recorded, and
  * a panel setup sheet: every `set` / timed `set` / `ramp` parameter change,
    which the operator applies on the hardware (or via its own documented
    NRPNs) at the listed times.

Panel positions in scenarios are 0..1 knob rotations. The mapping of a
position to the hardware's displayed value is assumption A-MAP-01 (linear
over the displayed range); record the actual displayed values on the sheet.

usage: scn2midi.py <scenario.scn> <out.mid> [--channel 1]
"""
import argparse
import struct


def vlq(n):
    out = [n & 0x7F]
    n >>= 7
    while n:
        out.append(0x80 | (n & 0x7F))
        n >>= 7
    return bytes(reversed(out))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("scn")
    ap.add_argument("mid")
    ap.add_argument("--channel", type=int, default=1)
    a = ap.parse_args()
    ch = (a.channel - 1) & 15
    events, sheet = [], []
    with open(a.scn) as f:
        for line in f:
            line = line.split("#")[0].split()
            if not line:
                continue
            if line[0] == "set":
                sheet.append((0.0, f"{line[1]} = {line[2]}"))
            elif line[0] == "ramp":
                sheet.append((float(line[1]), f"sweep {line[3]} {line[4]} -> {line[5]} until {line[2]} s"))
            elif line[0] == "at":
                t, cmd = float(line[1]), line[2]
                if cmd == "on":
                    events.append((t, bytes([0x90 | ch, int(line[3]), int(line[4])])))
                elif cmd == "off":
                    events.append((t, bytes([0x80 | ch, int(line[3]), 64])))
                elif cmd == "cc":
                    events.append((t, bytes([0xB0 | ch, int(line[3]), int(line[4])])))
                elif cmd == "bend":
                    v = max(0, min(16383, int(8192 + float(line[3]) * 8191)))
                    events.append((t, bytes([0xE0 | ch, v & 0x7F, v >> 7])))
                elif cmd == "pressure":
                    events.append((t, bytes([0xD0 | ch, max(0, min(127, int(float(line[3]) * 127)))])))
                elif cmd == "set":
                    sheet.append((t, f"{line[3]} = {line[4]}"))
                elif cmd == "tune":
                    sheet.append((t, "run the instrument's tune routine"))
    events.sort(key=lambda e: e[0])
    # 1 tick = 1 ms: 500 ticks per quarter at 120 bpm.
    track = bytearray()
    last = 0
    for t, msg in events:
        tick = int(round(t * 1000))
        track += vlq(tick - last) + msg
        last = tick
    track += vlq(0) + b"\xFF\x2F\x00"
    tempo = vlq(0) + b"\xFF\x51\x03" + (500000).to_bytes(3, "big")
    body = tempo + bytes(track)
    with open(a.mid, "wb") as f:
        f.write(b"MThd" + struct.pack(">IHHH", 6, 0, 1, 500))
        f.write(b"MTrk" + struct.pack(">I", len(body)) + body)
    print(f"# Panel setup sheet for {a.scn}")
    print("# Start from the basic (init) program, filter Rev as listed, Vintage as listed.")
    for t, s in sorted(sheet, key=lambda x: x[0]):
        print(f"{t:8.3f} s  {s}")


if __name__ == "__main__":
    main()

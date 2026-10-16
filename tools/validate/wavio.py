"""WAV read/write (PCM 16/24/32, float32), numpy only."""
import struct

import numpy as np


def read(path):
    with open(path, "rb") as f:
        data = f.read()
    if data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        raise ValueError(f"{path}: not a WAV file")
    pos, fmt = 12, None
    while pos + 8 <= len(data):
        cid, size = data[pos:pos + 4], struct.unpack("<I", data[pos + 4:pos + 8])[0]
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
                b = np.frombuffer(body[: len(body) // 3 * 3], dtype=np.uint8).reshape(-1, 3).astype(np.int32)
                v = b[:, 0] | (b[:, 1] << 8) | (b[:, 2] << 16)
                x = np.where(v >= 1 << 23, v - (1 << 24), v) / float(1 << 23)
            elif bits == 32:
                x = np.frombuffer(body, dtype="<i4") / 2147483648.0
            else:
                raise ValueError(f"{path}: unsupported WAV format")
            return x.reshape(-1, ch)[:, 0].copy(), rate
        pos += 8 + size + (size & 1)
    raise ValueError(f"{path}: no data chunk")


def write(path, x, fs):
    x = np.asarray(x, dtype="<f4")
    with open(path, "wb") as f:
        f.write(b"RIFF" + struct.pack("<I", 36 + x.nbytes) + b"WAVEfmt ")
        f.write(struct.pack("<IHHIIHH", 16, 3, 1, fs, fs * 4, 4, 32))
        f.write(b"data" + struct.pack("<I", x.nbytes) + x.tobytes())

#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# BIGORNA: the FM-1's sessions, their patterns and loops, and GLOBAL SETUP, read over USB MIDI into a backup file the
# Backup page restores (docs/backup.html; the protocol: firmware/src/backup.c, docs/js/fm1backup.js). It only lists
# and reads: nothing on the FM-1 is written.
#   tools/fm1_backup.py [OUT.json]       (default build/backups/bigorna-backup-DATE.json)
import base64
import datetime
import json
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import fm1_install as fi

CHUNK = 256


def frame(cmd, body=b""):
    m = bytes([0x00, 0x59, cmd, len(body) & 0xFF, (len(body) >> 8) & 0xFF, (len(body) >> 16) & 0xFF]) + bytes(body)
    return bytes([0xF0]) + bytes(fi.pack7(m + bytes([~sum(body) & 0xFF]))) + bytes([0xF7])


def ask(link, cmd, body=b"", ms=3000):
    for _ in range(3):
        link.drain()
        if not link.send(frame(cmd, body)):
            raise SystemExit("fm1_backup: the FM-1 is no longer reachable")
        end = time.monotonic() + ms / 1000
        while (left := end - time.monotonic()) > 0:
            p = link.read(left)
            if not p:
                break
            d = bytes(fi.unpack7(p[1:-1]))
            if len(d) < 7 or d[0] != 0 or d[1] != 0x59 or d[2] != cmd:
                continue
            n = d[3] | d[4] << 8 | d[5] << 16
            if len(d) < 7 + n:
                continue
            b = d[6:6 + n]
            if ~sum(b) & 0xFF == d[6 + n]:
                return b
    return None


def main():
    be = fi.MidoBackend()
    ins = [n for n in be.input_names() if "FM-1" in n]
    if not ins:
        raise SystemExit("fm1_backup: no FM-1 on USB MIDI")
    out_name = fi.pair_output(ins[0], be.output_names())
    link = be.open(ins[0], out_name)
    try:
        b = ask(link, 0x50, ms=1500)
        if b is None:
            raise SystemExit("fm1_backup: the FM-1 does not answer (no backup in its firmware?)")
        recs = [(b[1 + i * 6] | b[2 + i * 6] << 8, int.from_bytes(b[3 + i * 6:7 + i * 6], "little")) for i in range(b[0])]
        out = []
        for num, length in recs:
            data = bytearray(length)
            off = 0
            while off < length:
                r = ask(link, 0x51, bytes([num & 0xFF, num >> 8]) + off.to_bytes(4, "little") + bytes([CHUNK & 0xFF, CHUNK >> 8]))
                if r is None:
                    raise SystemExit(f"fm1_backup: the FM-1 stopped answering while reading record {num}")
                n = r[6] | r[7] << 8
                if not n or int.from_bytes(r[2:6], "little") != off:
                    raise SystemExit(f"fm1_backup: record {num} could not be read whole")
                data[off:off + n] = r[8:8 + n]
                off += n
            out.append({"number": num, "length": length, "data": base64.b64encode(bytes(data)).decode()})
    finally:
        link.close()
    now = datetime.datetime.now()
    path = Path(sys.argv[1]) if len(sys.argv) > 1 else \
        Path(__file__).resolve().parent.parent / "build" / "backups" / f"bigorna-backup-{now:%Y-%m-%d-%H%M}.json"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps({"format": "bigorna-backup", "version": 1, "firmware": ins[0], "saved": now.isoformat(),
                                "records": out}))
    print(f"fm1_backup: {len(out)} records -> {path}")


if __name__ == "__main__":
    main()

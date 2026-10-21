// SPDX-License-Identifier: GPL-3.0-only
// BIGORNA (after TONITOR's): the backup protocol (firmware/src/backup.c), for the backup page and its test: framed
// as the installer's, F0 pack7(00 59 cmd len24 body ~sum) F7.
//   0x50 list -> count, then number (u16) and length (u32) of each record
//   0x51 read number off n -> number off n, then n bytes;  0x52 write number len, 0x53 data off bytes, 0x54 commit
//   -> a status: 0 done, 1 no flash, 2 not as asked, 3 no room, 4 not a whole record, 5 the flash failed
// A record is a 4 KB sector of the FM-1's flash: session k (1 .. 16) is (k - 1) x 16 (the session) and + 1 .. 3 (its
// patterns), its loops 0x1000 + (k - 1) x 16 + j, GLOBAL SETUP 0x2000.
// A link: send(array) -> bool, read(ms) -> a received F0..F7 array or null, drain().
import { pack7, unpack7 } from "./fm1ota.js";

export const LOOP = 0x1000, SETUP = 0x2000, CHUNK = 256;
export const STATUS = ["done", "the FM-1 has no flash store", "the request was not understood",
  "there is no room left on the FM-1", "the record was not whole (damaged file?)", "writing the flash failed"];
const now = () => (typeof performance !== "undefined" ? performance.now() : Date.now());

/** what a record is: {kind: "session" | "patterns" | "loops" | "setup", session: 1 .. 16} */
export function kindOf(number) {
  if (number === SETUP) return { kind: "setup", session: 0 };
  if (number >= LOOP) return { kind: "loops", session: ((number - LOOP) >> 4) + 1 };
  return { kind: number % 16 ? "patterns" : "session", session: (number >> 4) + 1 };
}
/** the order a restore writes them in: each session's loops, then its patterns, then the session; GLOBAL SETUP
 * last (the firmware points a session at the loops that came before it) */
export function restoreOrder(recs) {
  const key = (r) => {
    const k = kindOf(r.number);
    return k.kind === "setup" ? 1e6 : k.session * 100 + (k.kind === "loops" ? 0 : k.kind === "patterns" ? 10 : 20) + (r.number & 15);
  };
  return [...recs].sort((a, b) => key(a) - key(b));
}
export function frame(cmd, body = []) {
  const m = [0x00, 0x59, cmd, body.length & 0xFF, (body.length >> 8) & 0xFF, (body.length >> 16) & 0xFF, ...body];
  m.push(~body.reduce((a, b) => a + b, 0) & 0xFF);
  return [0xF0, ...pack7(m), 0xF7];
}
export async function ask(link, cmd, body = [], ms = 3000) {
  for (let tries = 0; tries < 3; tries++) {
    link.drain();
    if (!link.send(frame(cmd, body))) throw new Error("The FM-1 is no longer reachable.");
    const end = now() + ms;
    let left;
    while ((left = end - now()) > 0) {
      const p = await link.read(left);
      if (!p) break;
      const d = unpack7(p.slice(1, -1));
      if (d.length < 7 || d[0] !== 0 || d[1] !== 0x59 || d[2] !== cmd) continue;
      const n = d[3] | (d[4] << 8) | (d[5] << 16);
      if (d.length < 7 + n) continue;
      const b = d.slice(6, 6 + n);
      if ((~b.reduce((a, x) => a + x, 0) & 0xFF) !== d[6 + n]) continue;
      return b;
    }
  }
  return null;
}
const le32 = (b, i) => (b[i] | (b[i + 1] << 8) | (b[i + 2] << 16) | (b[i + 3] << 24)) >>> 0;
const w32 = (v) => [v & 0xFF, (v >>> 8) & 0xFF, (v >>> 16) & 0xFF, (v >>> 24) & 0xFF];

/** the records on the FM-1: [{number, length}], or null if it does not answer (no backup in its firmware) */
export async function list(link) {
  const b = await ask(link, 0x50, [], 1500);
  if (!b) return null;
  const recs = [];
  for (let i = 0; i < b[0]; i++)
    recs.push({ number: b[1 + i * 6] | (b[2 + i * 6] << 8), length: le32(b, 3 + i * 6) });
  return recs;
}
export async function readRecord(link, rec, done = () => {}) {
  const out = new Uint8Array(rec.length);
  for (let off = 0; off < rec.length;) {
    const b = await ask(link, 0x51, [rec.number & 0xFF, rec.number >> 8, ...w32(off), CHUNK & 0xFF, CHUNK >> 8]);
    if (!b) throw new Error(`The FM-1 stopped answering while reading record ${rec.number}.`);
    const n = b[6] | (b[7] << 8);
    if (!n || le32(b, 2) !== off) throw new Error(`Record ${rec.number} could not be read whole.`);
    out.set(b.slice(8, 8 + n), off);
    off += n;
    done(n);
  }
  return out;
}
const named = (number) => {
  const k = kindOf(number);
  return k.kind === "setup" ? "GLOBAL SETUP" : k.kind === "session" ? `Session ${k.session}` : `Session ${k.session}'s ${k.kind}`;
};
export async function writeRecord(link, number, bytes, done = () => {}) {
  const check = (b, what) => {
    if (!b) throw new Error(`The FM-1 stopped answering (${what}).`);
    if (b[0]) throw new Error(`${named(number)}: ${STATUS[b[0]] || "error " + b[0]}.`);
  };
  check(await ask(link, 0x52, [number & 0xFF, number >> 8, ...w32(bytes.length)], 5000), "starting");
  for (let off = 0; off < bytes.length; off += CHUNK) {
    const part = Array.from(bytes.slice(off, off + CHUNK));
    check(await ask(link, 0x53, [...w32(off), ...part], 5000), "sending");
    done(part.length);
  }
  check(await ask(link, 0x54, [], 8000), "writing");
}

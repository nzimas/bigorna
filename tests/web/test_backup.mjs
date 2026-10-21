// SPDX-License-Identifier: GPL-3.0-only
// BIGORNA (after TONITOR's): the backup page's protocol (docs/js/fm1backup.js) against the firmware's
// (firmware/src/backup.c, run by build/host bkserve): every record listed and read whole, the flash wiped, written
// back in the page's order, a restart, the same records -- the sessions load with their loops, GLOBAL SETUP back;
// a damaged record refused.
import { spawn } from "node:child_process";
import { createInterface } from "node:readline";
import { list, readRecord, writeRecord, restoreOrder, kindOf } from "../../docs/js/fm1backup.js";

const dev = spawn(process.argv[2] || "build/host", ["bkserve"]);
const lines = [], waiters = [];
createInterface({ input: dev.stdout }).on("line", (l) => (waiters.length ? waiters.shift()(l) : lines.push(l)));
const next = (ms) => new Promise((res) => {
  if (lines.length) return res(lines.shift());
  const t = setTimeout(() => { waiters.splice(waiters.indexOf(f), 1); res(null); }, ms);
  const f = (l) => { clearTimeout(t); res(l); };
  waiters.push(f);
});
const hex = (a) => a.map((b) => b.toString(16).padStart(2, "0")).join("");
const unhex = (s) => s.match(/../g).map((h) => parseInt(h, 16));
const link = {
  send(p) { dev.stdin.write(hex(p) + "\n"); return true; },
  async read(ms) { const l = await next(ms); return l && /^f0/.test(l) ? unhex(l) : null; },
  drain() {},
};
const say = async (cmd) => { dev.stdin.write(cmd + "\n"); return next(5000); };
let fail = 0;
const check = (ok, what) => { if (!ok) { console.log("FAIL backup page: " + what); fail++; } };

if ((await next(10000)) !== "READY") check(false, "the device did not start");
const recs = await list(link);
const kinds = (recs || []).map((r) => kindOf(r.number));
check(recs && kinds.filter((k) => k.kind === "session").length === 2 && kinds.some((k) => k.kind === "setup") &&
      kinds.filter((k) => k.kind === "loops" && k.session === 3).length === 2 && kinds.some((k) => k.kind === "patterns"),
      `the list (${JSON.stringify(recs)})`);
const before = [await say("LOAD 2"), await say("LOAD 9")];
const saved = [];
for (const r of recs || []) saved.push({ number: r.number, bytes: await readRecord(link, r) });
check(await say("WIPE") === "OK", "wipe");
check((await list(link)).length === 0, "the flash not wiped");
for (const r of restoreOrder(saved)) await writeRecord(link, r.number, r.bytes);
check(await say("SCAN") === "OK", "restart");
const again = await list(link);
check(again && again.length === recs.length, `not all back after a restart (${again && again.length} of ${recs.length})`);
const after = [await say("LOAD 2"), await say("LOAD 9")];
check(before[0] === "OK 133 753 1" && after[0] === before[0], `session 3 not as it was (${before[0]} / ${after[0]})`);
check(before[1] === "OK 97 0 1" && after[1] === before[1], `session 10 not as it was (${before[1]} / ${after[1]})`);
for (const r of saved.filter((x) => kindOf(x.number).kind !== "loops")) {   // (the loops: a new generation)
  const b = await readRecord(link, { number: r.number, length: r.bytes.length });
  check(kindOf(r.number).kind === "session" || b.every((x, i) => x === r.bytes[i]), `record ${r.number} differs`);
}
for (const r of restoreOrder(saved)) await writeRecord(link, r.number, r.bytes);   // (again, over itself)
check(await say("SCAN") === "OK" && (await say("LOAD 2")) === before[0], "restored twice: session 3 not as it was");
let refused = false;                                   // a damaged record: refused
const bad = saved.find((r) => kindOf(r.number).kind === "session").bytes.slice();
bad[200] ^= 0x55;
try { await writeRecord(link, saved.find((r) => kindOf(r.number).kind === "session").number, bad); }
catch (e) { refused = /not whole/.test(e.message); }
check(refused, "a damaged record not refused");
dev.stdin.end();
console.log(fail ? "backup page: FAILED" : `backup page: ok (${recs.length} records, ${saved.reduce((a, r) => a + r.bytes.length, 0)} bytes, out and back)`);
process.exit(fail ? 1 : 0);

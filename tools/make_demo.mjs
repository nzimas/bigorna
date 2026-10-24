// SPDX-License-Identifier: GPL-3.0-only
// BIGORNA: the demo's sessions (tools/demo/bigorna-demo.bin, for tools/render_demo.mjs): the sessions of a backup file
// (the Backup page's, or tools/fm1_backup.py's) restored by the firmware itself (build/host bkserve, tests/run_tests.sh builds it) into an
// erased store, the session to start with loaded at its pattern (so the FM-1 notes it as the last), the store's
// sectors written as fm1-emu boots them (persistence.js: each its offset, u32, then its 4 KB).
//   node tools/make_demo.mjs BACKUP.json SESSION PATTERN [OUT]      (SESSION 1 .. 16, PATTERN 1 .. 16)
import { spawn } from "node:child_process";
import { readFileSync } from "node:fs";
import { createInterface } from "node:readline";
import { writeRecord, restoreOrder } from "../docs/js/fm1backup.js";

const [file, ses, pat, out = "tools/demo/bigorna-demo.bin"] = process.argv.slice(2);
if (!file || !(ses >= 1 && ses <= 16) || !(pat >= 1 && pat <= 16)) {
  console.log("make_demo: BACKUP.json SESSION PATTERN [OUT]");
  process.exit(1);
}
const j = JSON.parse(readFileSync(file, "utf8"));
if (j.format !== "bigorna-backup" || !Array.isArray(j.records)) throw new Error(`${file}: not a Bigorna backup`);
const recs = j.records.map((r) => ({ number: r.number, bytes: Uint8Array.from(Buffer.from(r.data, "base64")) }));

const dev = spawn("build/host", ["bkserve"]);
const lines = [], waiters = [];
createInterface({ input: dev.stdout }).on("line", (l) => (waiters.length ? waiters.shift()(l) : lines.push(l)));
const next = (ms) => new Promise((res) => {
  if (lines.length) return res(lines.shift());
  const t = setTimeout(() => { waiters.splice(waiters.indexOf(f), 1); res(null); }, ms);
  const f = (l) => { clearTimeout(t); res(l); };
  waiters.push(f);
});
const hex = (a) => Array.from(a, (b) => b.toString(16).padStart(2, "0")).join("");
const link = {
  send(p) { dev.stdin.write(hex(p) + "\n"); return true; },
  async read(ms) { const l = await next(ms); return l && /^f0/.test(l) ? l.match(/../g).map((h) => parseInt(h, 16)) : null; },
  drain() {},
};
const say = async (cmd, want) => {
  dev.stdin.write(cmd + "\n");
  const r = await next(5000);
  if (!r || !r.startsWith(want)) throw new Error(`make_demo: ${cmd}: ${r}`);
  return r;
};

if ((await next(10000)) !== "READY") throw new Error("make_demo: build/host bkserve did not start");
await say("CLEAN", "OK");
for (const r of restoreOrder(recs)) await writeRecord(link, r.number, r.bytes);
await say("SCAN", "OK");
await say(`PAT ${ses - 1} ${pat - 1}`, "OK");
const loaded = await say(`LOAD ${ses - 1}`, "OK");
const dumped = await say(`DUMP ${out}`, "OK");
dev.stdin.end();
console.log(`make_demo: ${recs.length} records, session ${ses} at pattern ${pat} (tempo, loop notes, ` +
            `send cc: ${loaded.slice(3)}) -> ${out}, ${dumped.slice(3)} sectors`);

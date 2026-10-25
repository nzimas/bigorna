// SPDX-License-Identifier: GPL-3.0-only
// BIGORNA: a pattern of the demo's sessions, recorded offline in fm1-emu (tools/demo: its WebAssembly module, MIT, and
// a flash image of the sessions, booting into one: tools/make_demo.mjs) with the site's package: at 240 MHz, MASTER
// at 800 of 1023, ARP, the pattern's white key, PLAY; SECONDS from PLAY, as raw audio (16-bit, stereo, 44.1 kHz:
// tools/make_loop.py cuts the home page's loop from it). Slower than real time (the emulator cannot play it live: the
// sessions keep both cores busy), but every sample as the firmware makes it.
//   node tools/render_demo.mjs PATTERN SECONDS OUT.raw [FLASH]     (PATTERN 1 .. 16; FLASH tools/demo/session-3.bin)
// (after fm1-emu's web/bench.mjs, MIT: the same calls into the module)
import { readFileSync, writeFileSync } from "node:fs";
import { jitImports } from "./demo/jit.js";

const [pat, secs, out, flash = "tools/demo/session-3.bin"] = [+process.argv[2], +process.argv[3], process.argv[4], process.argv[5]];
if (!(pat >= 1 && pat <= 16) || !(secs > 0) || !out) {
  console.log("render_demo: PATTERN SECONDS OUT.raw [FLASH]");
  process.exit(1);
}
const MHZ = 240, PLAY_MS = 3000;
// the white keys from the left: the emulator's note keys n0 (F3) .. n26 (G5)
const WHITE = [0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26];
const key = `n${WHITE[pat - 1]}`;
const script = `100:pot:800,1000:ARP,1200:ARP^,2000:${key},2200:${key}^,${PLAY_MS}:PLAY,${PLAY_MS + 200}:PLAY^`;

let x;
const stats = { regions: 0, failed: 0, compileMs: 0, compileMaxMs: 0, bytes: 0 };
const { instance } = await WebAssembly.instantiate(readFileSync("tools/demo/fm1_web.wasm"), { env: jitImports(() => x, stats) });
x = instance.exports;
x.fm1_jit(1, 16);
x.fm1_ahead(1024);
const put = (bytes) => {
  const p = x.fm1_alloc(bytes.length);
  new Uint8Array(x.memory.buffer, p, bytes.length).set(bytes);
  return p;
};
const pkg = readFileSync("docs/firmware/bigorna.fwsc"), demo = readFileSync(flash);
if (x.fm1_boot_fwsc(put(pkg), pkg.length, 0, 0, MHZ) !== 0) throw new Error("render_demo: the package did not boot");
if (x.fm1_flash_restore(put(demo), demo.length) !== 0) throw new Error("render_demo: the demo's sectors were refused");
const sb = new TextEncoder().encode(script);
if (x.fm1_bench_script(put(sb), sb.length)) throw new Error("render_demo: the input script was refused");

// as the page's worklet: 20 ms of emulated time a quantum while it boots, 1.5 quanta once audio plays
let budget = Math.ceil(MHZ * 1e6 * 0.02);
const play = Math.ceil(MHZ * 1e6 * 1.5 * 128 / 44100), pcm = [];
while (x.fm1_ms() < PLAY_MS + secs * 1000) {
  const before = x.fm1_ms(), have = x.fm1_render(128, budget);
  if (have) budget = play;
  const l = new Float32Array(x.memory.buffer, x.fm1_out_l(), have), r = new Float32Array(x.memory.buffer, x.fm1_out_r(), have);
  if (before >= PLAY_MS)                         /* (from PLAY: the emulated time the quantum began at) */
    for (let i = 0; i < have; i++) pcm.push(Math.max(-32768, Math.min(32767, Math.round(l[i] * 32768))),
                                          Math.max(-32768, Math.min(32767, Math.round(r[i] * 32768))));
  if (x.fm1_halted()) throw new Error(`render_demo: halted at ${x.fm1_ms()} ms`);
}
writeFileSync(out, Buffer.from(Int16Array.from(pcm).buffer));
console.log(`render_demo: pattern ${pat}, ${(pcm.length / 2 / 44100).toFixed(2)} s -> ${out}`);

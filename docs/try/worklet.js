// The emulator in the audio thread: each 128-frame quantum runs the machine until its I2S
// DMA has produced 128 frames, so the audio device paces the emulation; and also, for an even
// amount of work per quantum, its share of emulated time while the queue holds less than one
// DMA half buffer beyond them (fm1_ahead: half the firmwares hand out 256 or 512 frames at
// once). The page gets the LCD and the LEDs about 30 times a second; keys, knobs and the pot
// come back as messages.
// The WebAssembly JIT's host side, as web/jit.js (a copy: an AudioWorklet does not import
// modules everywhere): compile a hot block's module, install its `run` in the function table.
function jitImports(exports, stats) {
  return {
    fm1_jit_compile(ptr, len) {
      const start = now();
      try {
        const x = exports();
        const bytes = new Uint8Array(x.memory.buffer, ptr, len).slice();
        const table = x.__indirect_function_table;
        const run = new WebAssembly.Instance(new WebAssembly.Module(bytes), { env: { memory: x.memory, table } }).exports.run;
        const slot = table.grow(1);
        table.set(slot, run);
        stats.regions++;
        stats.bytes += len;
        stats.tableSlots = table.length;
        return slot;
      } catch (e) {
        stats.failed++;
        stats.error = String(e);
        return 0;
      } finally {
        const elapsed = now() - start;
        stats.compileMs += elapsed;
        stats.compileMaxMs = Math.max(stats.compileMaxMs, elapsed);
      }
    },
  };
}

// How much a quantum may emulate, in the emulator's run budget (cpu0's instructions, at most
// one per cycle): while the firmware boots, 20 ms of emulated time, so a boot is quick (there is
// no audio to keep yet); once audio plays, 1.5 quanta, so a firmware that stalls or falls silent
// does not hold the audio thread for 20 ms each quantum. The pace (fm1_ahead) needs one quantum
// when it may run a whole DMA half buffer ahead; fm1_render raises the cap by the rest of the half
// buffer when ahead is less (?ahead=256 with 512-frame halves). Without the pace (?ahead=0) a
// quantum may have to run a whole half buffer's time, so it keeps 20 ms.
const AHEAD_FRAMES = 1024;
function budgets(mhz, frames) {
  return { boot: mhz * 1e6 * 0.02, play: mhz * 1e6 * 1.5 * frames / sampleRate };
}

// the wall clock, for how long each quantum's emulation takes (where an AudioWorklet has no
// performance.now(), Date.now(): whole milliseconds, which still average out over a second)
const now = globalThis.performance?.now ? () => performance.now() : () => Date.now();

class Fm1 extends AudioWorkletProcessor {
  constructor() {
    super();
    this.x = null;
    this.calls = 0;
    this.bootToken = 0;
    this.snapshots = [];
    this.port.onmessageerror = () => this.port.postMessage({ type: "error", message: "a message could not be read" });
    this.port.onmessage = async (e) => {
      const m = e.data;
      if (m.type === "boot") {
        const token = ++this.bootToken;
        // Keep the previous (paused) instance until the replacement boots successfully.
        try {
          await this.boot(m, token);
        } catch (err) {
          if (token !== this.bootToken) return;
          this.paused = false;
          this.port.postMessage({ type: "error", seq: m.seq, message: `boot: ${err}` });
        }
      } else if (m.type === "cancel-boot") {
        ++this.bootToken;
        this.x = null;
        this.snapshots = [];
      } else if (m.type === "snapshot" && !this.x) {
        this.port.postMessage({ type: "snapshot", seq: m.seq, request: m.request, unavailable: true });
      } else if (this.x) {
        if (m.type === "snapshot") {
          if (m.seq !== this.seq) return;
          this.snapshots.push({ ...m, started: now() });
          this.flushSnapshots();
        } else if (m.type === "cancel-snapshot") this.snapshots = this.snapshots.filter(r => r.request !== m.request);
        else if (m.type === "resume") this.paused = false;
        else if (m.type === "key") this.x.fm1_key(m.id, m.down ? 1 : 0);
        else if (m.type === "enc") this.x.fm1_enc(m.enc, m.clicks);
        else if (m.type === "pot") this.x.fm1_pot(m.value);
        else if (m.type === "midi") for (const b of m.bytes) this.x.fm1_midi(b);
        else if (m.type === "analog") this.x.fm1_analog(m.idx, m.value);   // SMK-37: pads, faders, wheels
        else if (m.type === "vel") this.x.fm1_velocity(m.us);             // SMK-37: the next keys' velocity
      }
    };
  }

  async boot(m, token) {
    // the module arrives as bytes: a compiled WebAssembly.Module does not cross into the
    // AudioWorklet's scope (Chrome delivers it as a messageerror). Compiled once, here: on a
    // phone compiling it again on every firmware change takes a noticeable while
    if (!this.module) this.module = await WebAssembly.compile(m.wasm);
    let x;
    this.jit = { regions: 0, failed: 0, error: "", bytes: 0, tableSlots: 0, compileMs: 0, compileMaxMs: 0 };
    x = (await WebAssembly.instantiate(this.module, { env: jitImports(() => x, this.jit) })).exports;
    if (token !== this.bootToken) return;
    // hot code compiled to WebAssembly, a few blocks per quantum at most (?jit=0: interpreter only)
    x.fm1_jit(m.jit === false ? 0 : 1, 16);
    const ahead = x.fm1_ahead ? m.ahead ?? AHEAD_FRAMES : 0;
    x.fm1_ahead?.(ahead);               // (?ahead=0: each quantum runs for its own frames only)
    // the whole update package (.fwsc): the machine unpacks it, gets the flash chip and the
    // boot parameters from it (what M-VAVE's own firmware needs; the others boot the same)
    const copy = (buf) => {
      const p = x.fm1_alloc(buf.byteLength);
      new Uint8Array(x.memory.buffer, p, buf.byteLength).set(new Uint8Array(buf));
      return p;
    };
    const pkg = copy(m.package);
    if (x.fm1_boot_fwsc(pkg, m.package.byteLength, 0, 0, m.mhz) !== 0) {
      throw new Error("not an FM-1 update package (or a DX7 bank it cannot use)");
    }
    if (m.sectors?.byteLength && x.fm1_flash_restore(copy(m.sectors), m.sectors.byteLength) !== 0) {
      throw new Error("invalid saved flash sectors");
    }
    if (m.syx && x.fm1_load_syx(copy(m.syx), m.syx.byteLength) === 1) {
      throw new Error("this firmware cannot load that DX7 bank");
    }
    this.x = x;                         // (a reboot replaces the whole instance)
    this.mhz = m.mhz;
    this.ahead = ahead;
    this.budget = budgets(m.mhz, 128).boot;
    this.playing = false;               // the firmware's audio has started (the budget is capped)
    this.lastTiming = null;
    this.views = null;
    this.lcdRevision = null;
    this.reportFrames = 0;
    this.reportCalls = 0;
    this.short = 0;
    this.calls = 0;
    this.busy = 0;                      // ms spent emulating since the last frame message
    this.worst = 0;                     // the longest quantum among them (once audio plays)
    this.timedMs = 0;                   // the emulated ms when the timed interval began
    this.timedIdle = 0;                 // and the idle ms skipped by then
    this.board = x.fm1_board();         // 0 the FM-1, 1 the SMK-37 (the package says which)
    this.seq = m.seq;
    this.paused = false;
    this.snapshots = [];
    this.flashRevision = x.fm1_flash_revision();
    this.observedRevision = this.flashRevision;
    this.flashChangedAt = this.lastSnapshot = now();
    const base = new Uint8Array(x.memory.buffer, x.fm1_flash_base(), x.fm1_flash_len()).slice();
    const flash = new Uint8Array(x.memory.buffer, x.fm1_flash(), x.fm1_flash_len()).slice();
    this.port.postMessage({ type: "booted", seq: m.seq, board: this.board, revision: this.flashRevision, base, flash }, [base.buffer, flash.buffer]);
  }

  observeFlash() {
    const revision = this.x.fm1_flash_revision();
    if (revision !== this.observedRevision) {
      this.observedRevision = revision;
      this.flashChangedAt = now();
      this.port.postMessage({ type: "flash-changed", seq: this.seq, revision });
    }
    return revision;
  }

  flushSnapshots() {
    const x = this.x;
    if (!x) return;
    this.observeFlash();
    const quiet = now() - this.flashChangedAt >= 250 || x.fm1_halted();
    const busy = x.fm1_flash_busy();
    for (const request of this.snapshots.splice(0)) {
      if (busy || !quiet) {
        if (request.request !== undefined && now() - request.started >= 1500) {
          // A constantly writing firmware must not prevent switching away. Keep its
          // last stable save instead of persisting a possibly half-written sector.
          if (request.pause) this.paused = true;
          this.port.postMessage({ type: "snapshot", seq: this.seq, request: request.request,
            unavailable: true, error: request.pause ? null : "flash did not become quiet; try saving again" });
        } else this.snapshots.push(request);
        continue;
      }
      const flash = new Uint8Array(x.memory.buffer, x.fm1_flash(), x.fm1_flash_len()).slice();
      this.flashRevision = x.fm1_flash_revision();
      this.observedRevision = this.flashRevision;
      this.lastSnapshot = now();
      if (request.pause) this.paused = true;
      this.port.postMessage({ type: "snapshot", seq: this.seq, request: request.request,
        revision: this.flashRevision, flash }, [flash.buffer]);
    }
  }

  process(_inputs, outputs) {
    const out = outputs[0];
    const x = this.x;
    if (!x || this.paused) return true;
    const n = out[0].length;
    const playing = this.playing;       // (the worst quantum: of those once audio played)
    const t0 = now();
    let have;
    try {
      have = x.fm1_render(n, this.budget);
    } catch (err) {                       // a trap in the emulator: stop and say so
      this.x = null;
      for (const request of this.snapshots.splice(0)) {
        this.port.postMessage({ type: "snapshot", seq: this.seq, request: request.request, unavailable: true });
      }
      this.port.postMessage({ type: "error", seq: this.seq, fatal: true, message: `render: ${err}` });
      return true;
    }
    const buffer = x.memory.buffer, lp = x.fm1_out_l(), rp = x.fm1_out_r();
    if (!this.views || this.views.l.buffer !== buffer || this.views.l.length !== n ||
        this.views.l.byteOffset !== lp || this.views.r.byteOffset !== rp) {
      this.views = { l: new Float32Array(buffer, lp, n), r: new Float32Array(buffer, rp, n) };
    }
    const { l, r } = this.views;
    out[0].set(l);
    if (out[1]) out[1].set(r);
    if (have > 0 && !this.playing) {
      this.playing = true;
      if (this.ahead > 0) this.budget = budgets(this.mhz, n).play;
    }
    this.short = (this.short || 0) + (have < n ? 1 : 0);
    this.reportFrames += n;
    this.reportCalls++;
    if (this.reportFrames >= sampleRate / 30) {
      const flashRevision = this.observeFlash();
      if (flashRevision !== this.flashRevision && now() - this.lastSnapshot >= 1000 &&
          now() - this.flashChangedAt >= 250 && !this.snapshots.length) {
        this.snapshots.push({});
      }
      this.flushSnapshots();
      const revision = x.fm1_lcd_revision();
      const lcd = revision !== this.lcdRevision ? new Uint16Array(x.memory.buffer, x.fm1_lcd(), 240 * 240).slice() : null;
      this.lcdRevision = revision;
      const leds = new Float32Array(x.memory.buffer, x.fm1_leds(), 41).slice();
      const pads = this.board ? new Uint32Array(x.memory.buffer, x.fm1_pads(), 16).slice() : null;
      const transfer = [leds.buffer];
      if (lcd) transfer.push(lcd.buffer);
      if (pads) transfer.push(pads.buffer);
      // Report the preceding interval: its final postMessage is included in its timing.
      const ms = x.fm1_ms(), idle = x.fm1_idle_ms?.() ?? 0;
      this.port.postMessage({ type: "frame", seq: this.seq, lcd, leds, ms,
        short: this.short, halted: x.fm1_halted(), jit: this.jit, pads, playing: this.playing,
        busy: this.lastTiming?.busy ?? 0, worst: this.lastTiming?.worst ?? 0,
        emulated: this.lastTiming?.emulated ?? 0, idle: this.lastTiming?.idle ?? 0,
        quanta: this.lastTiming?.quanta ?? 0, frames: this.lastTiming?.frames ?? n }, transfer);
      const took = now() - t0;
      // busy: ms of wall clock the interval's quanta took; emulated: ms of emulated time they ran,
      // idle: of which idle skipping jumped over (cost nothing)
      this.lastTiming = { busy: this.busy + took, worst: playing ? Math.max(this.worst, took) : this.worst,
        emulated: ms - this.timedMs, idle: idle - this.timedIdle,
        quanta: this.reportCalls, frames: this.reportFrames / this.reportCalls };
      this.timedMs = ms;
      this.timedIdle = idle;
      this.busy = 0;
      this.worst = 0;
      this.reportFrames = 0;
      this.reportCalls = 0;
    } else {
      const took = now() - t0;
      this.busy += took;
      if (playing) this.worst = Math.max(this.worst, took);
    }
    return true;
  }
}
registerProcessor("fm1", Fm1);

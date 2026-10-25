/* SPDX-License-Identifier: MIT -- Bigorna website */
(function () {   /* the home page's demo patterns: one at a time, each a gapless loop (Web Audio) until stopped */
  var box = document.getElementById("demo");
  if (!box) return;
  var rows = [].slice.call(box.querySelectorAll(".demo-list li")), state = box.querySelector(".demo-state"),
      canvas = box.querySelector(".demo-scope"), idle = state.textContent;
  var ctx, an, bins, raf, cur = null, src = null, gain = null;
  var still = window.matchMedia && matchMedia("(prefers-reduced-motion: reduce)").matches;

  function audio() {
    if (!ctx) {
      ctx = new (window.AudioContext || window.webkitAudioContext)();
      an = ctx.createAnalyser();
      an.fftSize = 2048;
      an.smoothingTimeConstant = 0.82;
      bins = new Uint8Array(an.frequencyBinCount);
      an.connect(ctx.destination);
    }
    ctx.resume();
  }
  function load(li) {                  /* (each fetched and decoded once) */
    if (!li._buf)
      li._buf = fetch(li.dataset.src)
        .then(function (r) { if (!r.ok) throw new Error(r.status); return r.arrayBuffer(); })
        .then(function (b) { return new Promise(function (ok, no) { ctx.decodeAudioData(b, ok, no); }); })
        .catch(function (e) { li._buf = null; throw e; });
    return li._buf;
  }
  function mark(li, on) {
    var b = li.querySelector(".demo-play"), name = li.querySelector(".demo-name").textContent;
    li.classList.toggle("on", on);
    b.setAttribute("aria-pressed", on ? "true" : "false");
    b.setAttribute("aria-label", (on ? "Stop " : "Play ") + name);
  }
  function halt() {                    /* the one playing, faded out */
    if (src) {
      var t = ctx.currentTime;
      gain.gain.cancelScheduledValues(t);
      gain.gain.setValueAtTime(gain.gain.value, t);
      gain.gain.linearRampToValueAtTime(0, t + 0.06);
      src.stop(t + 0.07);
      src = gain = null;
    }
    if (cur) { cur.classList.remove("wait"); mark(cur, false); }
    cur = null;
    state.textContent = idle;
  }
  function play(li) {
    halt();
    audio();
    cur = li;
    li.classList.add("wait");
    state.textContent = "Loading…";
    load(li).then(function (buf) {
      if (cur !== li) return;          /* (another chosen, or stopped, while it loaded) */
      li.classList.remove("wait");
      src = ctx.createBufferSource();
      src.buffer = buf;
      src.loop = true;
      src.loopStart = +li.dataset.loopStart;
      src.loopEnd = +li.dataset.loopEnd;
      gain = ctx.createGain();
      gain.gain.setValueAtTime(0, ctx.currentTime);
      gain.gain.linearRampToValueAtTime(1, ctx.currentTime + 0.03);
      src.connect(gain).connect(an);
      src.start(0, src.loopStart);
      mark(li, true);
      state.textContent = "Playing · loops until stopped";
      if (!still) draw();
    }, function () {
      if (cur !== li) return;
      li.classList.remove("wait");
      cur = null;
      state.textContent = "Could not load the demo";
    });
  }
  rows.forEach(function (li) {
    li.querySelector(".demo-play").addEventListener("click", function () {
      if (cur === li) halt(); else play(li);
    });
  });

  /* the spectrum: thin bars, log-spaced, steel; they settle back when it stops */
  var level = [];
  function draw() {
    cancelAnimationFrame(raf);
    var dpr = window.devicePixelRatio || 1, w = canvas.clientWidth, h = canvas.clientHeight;
    if (canvas.width !== Math.round(w * dpr) || canvas.height !== Math.round(h * dpr)) {
      canvas.width = Math.round(w * dpr);
      canvas.height = Math.round(h * dpr);
    }
    var g = canvas.getContext("2d"), n = Math.max(24, Math.floor(w / 7)), on = !!src, any = false;
    if (on) an.getByteFrequencyData(bins);
    g.setTransform(dpr, 0, 0, dpr, 0, 0);
    g.clearRect(0, 0, w, h);
    for (var i = 0; i < n; i++) {
      var lo = Math.floor(2 * Math.pow(400, i / n)), hi = Math.max(lo + 1, Math.floor(2 * Math.pow(400, (i + 1) / n))), v = 0;
      if (on) { for (var k = lo; k < hi && k < bins.length; k++) v = Math.max(v, bins[k]); v /= 255; }
      level[i] = Math.max(v, (level[i] || 0) * 0.9);
      if (level[i] > 0.004) any = true;
      var bh = Math.max(1, level[i] * level[i] * (h - 4));
      g.fillStyle = "rgba(200, 204, 210, " + (0.25 + 0.6 * level[i]) + ")";
      g.fillRect(i * (w / n) + 1, (h - bh) / 2, Math.max(1.5, w / n - 3), bh);
    }
    if (on || any) raf = requestAnimationFrame(draw);
    else g.clearRect(0, 0, w, h);
  }
})();

/* SPDX-License-Identifier: MIT -- Bigorna website */
(function () {   /* the home page's demo pattern: one loop, gapless (Web Audio), until it is stopped */
  var box = document.getElementById("demo");
  if (!box) return;
  var btn = box.querySelector(".demo-play"), state = box.querySelector(".demo-state"),
      canvas = box.querySelector(".demo-scope"), idle = state.textContent;
  var ctx, buf, loading, src, gain, an, bins, raf;
  var still = window.matchMedia && matchMedia("(prefers-reduced-motion: reduce)").matches;

  function load() {
    if (!loading)
      loading = fetch(box.dataset.src)
        .then(function (r) { if (!r.ok) throw new Error(r.status); return r.arrayBuffer(); })
        .then(function (b) { return new Promise(function (ok, no) { ctx.decodeAudioData(b, ok, no); }); })
        .then(function (b) { buf = b; return b; }, function (e) { loading = null; throw e; });
    return loading;
  }
  function start() {
    if (!ctx) {
      ctx = new (window.AudioContext || window.webkitAudioContext)();
      an = ctx.createAnalyser();
      an.fftSize = 2048;
      an.smoothingTimeConstant = 0.82;
      bins = new Uint8Array(an.frequencyBinCount);
      an.connect(ctx.destination);
    }
    ctx.resume();
    box.classList.add("wait");
    state.textContent = "Loading…";
    load().then(function () {
      if (!box.classList.contains("wait")) return;   /* (stopped while it loaded) */
      box.classList.remove("wait");
      src = ctx.createBufferSource();
      src.buffer = buf;
      src.loop = true;
      src.loopStart = +box.dataset.loopStart;
      src.loopEnd = +box.dataset.loopEnd;
      gain = ctx.createGain();
      gain.gain.setValueAtTime(0, ctx.currentTime);
      gain.gain.linearRampToValueAtTime(1, ctx.currentTime + 0.03);
      src.connect(gain).connect(an);
      src.start(0, src.loopStart);
      box.classList.add("on");
      btn.setAttribute("aria-pressed", "true");
      btn.setAttribute("aria-label", "Stop the demo pattern");
      state.textContent = "Playing · loops until stopped";
      if (!still) draw();
    }, function () {
      box.classList.remove("wait");
      state.textContent = "Could not load the demo";
    });
  }
  function stop() {
    box.classList.remove("wait", "on");
    btn.setAttribute("aria-pressed", "false");
    btn.setAttribute("aria-label", "Play the demo pattern");
    state.textContent = idle;
    if (src) {
      var t = ctx.currentTime;
      gain.gain.cancelScheduledValues(t);
      gain.gain.setValueAtTime(gain.gain.value, t);
      gain.gain.linearRampToValueAtTime(0, t + 0.06);
      src.stop(t + 0.07);
      src = null;
    }
  }
  btn.addEventListener("click", function () {
    if (box.classList.contains("on") || box.classList.contains("wait")) stop(); else start();
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
    var g = canvas.getContext("2d"), n = Math.max(24, Math.floor(w / 7)), on = box.classList.contains("on"), any = false;
    if (on) an.getByteFrequencyData(bins);
    g.setTransform(dpr, 0, 0, dpr, 0, 0);
    g.clearRect(0, 0, w, h);
    for (var i = 0; i < n; i++) {
      var lo = Math.floor(2 * Math.pow(400, i / n)), hi = Math.max(lo + 1, Math.floor(2 * Math.pow(400, (i + 1) / n))), v = 0;
      if (on) { for (var k = lo; k < hi && k < bins.length; k++) v = Math.max(v, bins[k]); v /= 255; }
      level[i] = Math.max(v, (level[i] || 0) * 0.9);
      if (level[i] > 0.004) any = true;
      var bh = Math.max(1, level[i] * level[i] * (h - 16));
      g.fillStyle = "rgba(200, 204, 210, " + (0.25 + 0.6 * level[i]) + ")";
      g.fillRect(i * (w / n) + 1, (h - bh) / 2, Math.max(1.5, w / n - 3), bh);
    }
    if (on || any) raf = requestAnimationFrame(draw);
    else g.clearRect(0, 0, w, h);
  }
})();

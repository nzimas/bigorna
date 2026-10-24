/* SPDX-License-Identifier: MIT -- Bigorna website */
(function () {   /* Try it: the emulator's window as tall as its page (the same site, so its height can be read) */
  var f = document.getElementById("emu");
  function fit() {
    try {
      var d = f.contentDocument;
      /* (wider than tall and 560 px or less, as a phone held sideways, the emulator lays itself out for that: never so) */
      if (d && d.body) f.style.height = Math.max(f.clientWidth > 560 ? 561 : 0, d.body.scrollHeight) + "px";
    } catch (e) { /* (kept at the stylesheet's height) */ }
  }
  function watch() {
    var n = 0, t = setInterval(function () { fit(); if (++n >= 20) clearInterval(t); }, 500);   /* (while it boots) */
    fit();
    /* (the frame's own ResizeObserver: one of this page's does not follow the frame's document) */
    try { new f.contentWindow.ResizeObserver(fit).observe(f.contentDocument.body); } catch (e) { /* (one fit) */ }
  }
  f.addEventListener("load", watch);   /* (and again when the emulator reloads its page, a new clock chosen) */
  try { if (f.contentDocument.readyState === "complete" && f.contentDocument.body) watch(); } catch (e) { /* */ }
})();

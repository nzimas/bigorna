// SPDX-License-Identifier: GPL-3.0-only
// BIGORNA (after TONITOR's): the backup page -- the sessions (with their patterns and loops) and GLOBAL SETUP on an
// FM-1 read into a file, and written back from one, over Web MIDI. The FM-1 is found as the installer finds it
// (fm1ota.js); the protocol is fm1backup.js's.
import { Updater } from "./fm1ota.js";
import { kindOf, restoreOrder, list as listOn, readRecord as readOn, writeRecord as writeOn } from "./fm1backup.js";

const $ = (id) => document.getElementById(id);
const ui = {
  status: $("status"), detail: $("detail"), facts: $("facts"), dev: $("f-dev"), fileInfo: $("f-file"),
  bar: $("bar"), fill: $("bar").firstElementChild, connect: $("connect"), save: $("save"),
  file: $("file"), agree: $("agree"), restore: $("restore"),
  rStatus: $("r-status"), rDetail: $("r-detail"), rBar: $("r-bar"), rFill: $("r-bar").firstElementChild,
};
let updater = null, device = null, onDevice = null, backup = null, busy = false;

function say(status, detail = "", kind = "") {
  ui.status.textContent = status;
  ui.status.className = "status" + (kind ? " " + kind : "");
  ui.detail.textContent = detail;
}
function progress(f) {
  ui.bar.hidden = f === null;
  if (f !== null) ui.fill.style.width = `${Math.round(f * 100)}%`;
}
// the restore's own status and bar, under its button
function rsay(status, detail = "", kind = "") {
  ui.rStatus.hidden = !status;
  ui.rStatus.textContent = status;
  ui.rStatus.className = "status" + (kind ? " " + kind : "");
  ui.rDetail.hidden = !detail;
  ui.rDetail.textContent = detail;
}
function rprogress(f) {
  ui.rBar.hidden = f === null;
  if (f === null) return;
  ui.rFill.style.width = `${Math.round(f * 100)}%`;
  if (/^Restoring/.test(ui.rStatus.textContent)) ui.rStatus.textContent = `Restoring… ${Math.round(f * 100)}%`;
}
function refresh() {
  ui.connect.disabled = busy;
  ui.save.disabled = busy || !onDevice || !onDevice.length;
  ui.restore.disabled = busy || !device || !backup || !ui.agree.checked;
}
const describe = (recs) => {
  const ses = [...new Set(recs.map((r) => kindOf(r.number)).filter((k) => k.kind === "session").map((k) => k.session))];
  const loops = new Set(recs.map((r) => kindOf(r.number)).filter((k) => k.kind === "loops").map((k) => k.session));
  const parts = [];
  parts.push(ses.length ? `${ses.length} session${ses.length > 1 ? "s" : ""} (${ses.map((s) => s + (loops.has(s) ? " with loops" : "")).join(", ")})` : "no sessions");
  if (recs.some((r) => kindOf(r.number).kind === "setup")) parts.push("GLOBAL SETUP");
  return parts.join(", ");
};
const named = (n) => {
  const k = kindOf(n);
  return k.kind === "setup" ? "GLOBAL SETUP" : k.kind === "session" ? `Session ${k.session}` : `Session ${k.session}, its ${k.kind}`;
};

const b64 = (u8) => {
  let s = "";
  for (let i = 0; i < u8.length; i += 0x8000) s += String.fromCharCode(...u8.subarray(i, i + 0x8000));
  return btoa(s);
};
const unb64 = (s) => Uint8Array.from(atob(s), (c) => c.charCodeAt(0));

async function connect() {
  busy = true;
  refresh();
  say("Looking for the FM-1…", "Allow MIDI access if the browser asks.");
  try {
    if (!navigator.requestMIDIAccess)
      throw new Error("This browser has no Web MIDI. Open this page in a desktop Chrome, Edge or Opera.");
    if (!updater) updater = new Updater(await navigator.requestMIDIAccess({ sysex: true }));
    if (device) device.link.close();
    device = await updater.find();
    if (!device) {
      say("No FM-1 found", "Check the cable (it must carry data), switch the FM-1 on, close other MIDI programs, and try again.", "bad");
      return;
    }
    onDevice = /^FM-1_80\d\d$/.test(device.id.text || "") ? await listOn(device.link) : null;
    if (!onDevice) {
      say("This FM-1 cannot be backed up from here",
        `It does not run Bigorna build 55 or later (it runs ${device.id.text || "another firmware"}). Install Bigorna first (Install page), then come back.`, "bad");
      onDevice = null;
      return;
    }
    ui.facts.hidden = false;
    ui.dev.textContent = describe(onDevice);
    say("FM-1 found", onDevice.length ? "Save its sessions to a file, or restore a file to it." :
      "It holds no sessions yet. You can restore a file to it.", "ok");
  } catch (e) {
    say("Something went wrong", e.message, "bad");
  } finally {
    busy = false;
    refresh();
  }
}

async function save() {
  busy = true;
  refresh();
  try {
    const total = onDevice.reduce((a, r) => a + r.length, 0);
    let got = 0;
    const out = [];
    progress(0);
    for (const rec of onDevice) {
      say("Reading…", named(rec.number));
      const bytes = await readOn(device.link, rec, (n) => progress((got += n) / total));
      out.push({ number: rec.number, kind: kindOf(rec.number).kind, session: kindOf(rec.number).session,
        length: bytes.length, data: b64(bytes) });
    }
    const file = { format: "bigorna-backup", version: 1, firmware: device.id.text, saved: new Date().toISOString(), records: out };
    const blob = new Blob([JSON.stringify(file, null, 1)], { type: "application/json" });
    const a = document.createElement("a");
    a.href = URL.createObjectURL(blob);
    a.download = `bigorna-backup-${new Date().toISOString().slice(0, 10)}.json`;
    document.body.appendChild(a);
    a.click();
    a.remove();
    setTimeout(() => URL.revokeObjectURL(a.href), 10000);
    say("Saved", `${describe(onDevice)} written to ${a.download} (in your downloads).`, "ok");
  } catch (e) {
    say("The backup did not finish", e.message, "bad");
  } finally {
    progress(null);
    busy = false;
    refresh();
  }
}

async function openFile() {
  backup = null;
  ui.fileInfo.textContent = "no file opened";
  const f = ui.file.files[0];
  if (!f) return refresh();
  try {
    const j = JSON.parse(await f.text());
    if (j.format !== "bigorna-backup" || !Array.isArray(j.records)) throw new Error("This is not a Bigorna backup file.");
    const recs = j.records.map((r) => ({ number: r.number, bytes: unb64(r.data) }));
    if (recs.some((r) => !Number.isInteger(r.number) || r.bytes.length < 16 || r.bytes.length > 4096))
      throw new Error("The file is damaged.");
    backup = restoreOrder(recs);
    ui.facts.hidden = false;
    ui.fileInfo.textContent = `${describe(recs)}, saved ${(j.saved || "").slice(0, 10)}`;
    rsay(device ? "File opened" : "File opened: now find the FM-1 (above)", "Tick the box, then restore it.");
  } catch (e) {
    rsay("The file could not be opened", e.message, "bad");
  }
  refresh();
}

async function restore() {
  busy = true;
  refresh();
  const guard = (e) => {
    e.preventDefault();
    e.returnValue = "";
  };
  window.addEventListener("beforeunload", guard);
  try {
    const total = backup.reduce((a, r) => a + r.bytes.length, 0);
    let sent = 0;
    rprogress(0);
    if (!(await listOn(device.link))) throw new Error("The FM-1 does not answer.");   // (a restore starts afresh)
    for (const r of backup) {
      rsay(`Restoring… ${Math.round((sent / total) * 100)}%`, named(r.number));
      await writeOn(device.link, r.number, r.bytes, (n) => rprogress((sent += n) / total));
    }
    onDevice = await listOn(device.link);
    if (onDevice) ui.dev.textContent = describe(onDevice);
    rsay("Restored", `${describe(backup)} written to the FM-1. Load a session on the FM-1 to hear it.`, "ok");
  } catch (e) {
    rsay("The restore did not finish", e.message + " What was written before stays; run it again to finish.", "bad");
  } finally {
    window.removeEventListener("beforeunload", guard);
    rprogress(null);
    busy = false;
    refresh();
  }
}

ui.connect.addEventListener("click", connect);
ui.save.addEventListener("click", save);
ui.file.addEventListener("change", openFile);
ui.agree.addEventListener("change", refresh);
ui.restore.addEventListener("click", restore);
if (!window.isSecureContext) say("Open this page over https", "Web MIDI only works on a secure page.", "bad");
refresh();

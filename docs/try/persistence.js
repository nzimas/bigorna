// Firmware identity, sparse flash saves and browser storage. No audio-thread work here.
export const FLASH_SIZE = 1 << 20;
export const SECTOR_SIZE = 4096;
const MAX_PACKAGE = 8 << 20;
export const FORMAT = 'fm1-emulator-save';

export async function packageHash(bytes) {
  return [...new Uint8Array(await crypto.subtle.digest('SHA-256', bytes))]
    .map(b => b.toString(16).padStart(2, '0')).join('');
}

export function changedSectors(base, flash) {
  if (base.length !== FLASH_SIZE || flash.length !== FLASH_SIZE) throw new Error('invalid flash size');
  const sectors = [];
  // Compare four bytes per iteration; copied snapshots are naturally aligned.
  const aligned = base.byteOffset % 4 === 0 && flash.byteOffset % 4 === 0;
  const a = aligned ? new Uint32Array(base.buffer, base.byteOffset, FLASH_SIZE / 4) : base;
  const b = aligned ? new Uint32Array(flash.buffer, flash.byteOffset, FLASH_SIZE / 4) : flash;
  const step = aligned ? 4 : 1;
  for (let at = 0; at < FLASH_SIZE; at += SECTOR_SIZE) {
    for (let i = at / step; i < (at + SECTOR_SIZE) / step; i++) {
      if (a[i] !== b[i]) {
        sectors.push({ at, data: flash.slice(at, at + SECTOR_SIZE) });
        break;
      }
    }
  }
  return sectors;
}

export function validateSectors(sectors) {
  if (!Array.isArray(sectors) || sectors.length > 256) throw new Error('invalid flash sectors');
  const seen = new Set();
  for (const s of sectors) {
    if (!Number.isInteger(s.at) || s.at < 0 || s.at >= FLASH_SIZE || s.at % SECTOR_SIZE ||
        !(s.data instanceof Uint8Array) || s.data.length !== SECTOR_SIZE || seen.has(s.at)) {
      throw new Error('invalid or duplicate flash sector');
    }
    seen.add(s.at);
  }
  return sectors;
}

export function packSectors(sectors) {
  validateSectors(sectors);
  const bytes = new Uint8Array(sectors.length * (4 + SECTOR_SIZE));
  const view = new DataView(bytes.buffer);
  sectors.forEach((s, i) => {
    const o = i * (4 + SECTOR_SIZE);
    view.setUint32(o, s.at, true);
    bytes.set(s.data, o + 4);
  });
  return bytes;
}

const encode = bytes => {
  let text = '';
  for (let i = 0; i < bytes.length; i += 8192) text += String.fromCharCode(...bytes.subarray(i, i + 8192));
  return btoa(text);
};
const decode = (text, max) => {
  if (typeof text !== 'string' || text.length > 4 * Math.ceil(max / 3) ||
      (text.length % 4 !== 0 || /[^A-Za-z0-9+/=]/.test(text))) throw new Error('invalid backup data');
  const bytes = Uint8Array.from(atob(text), c => c.charCodeAt(0));
  if (bytes.length > max) throw new Error('backup data too large');
  return bytes;
};

export function exportSave(record) {
  validateSectors(record.sectors);
  return JSON.stringify({ format: FORMAT, version: 1, hash: record.hash, name: record.name,
    package: record.byo ? null : encode(record.package), sectors: record.sectors.map(s => ({ at: s.at, data: encode(s.data) })) });
}

export async function importSave(text, resolvePackage = async () => null) {
  if (text.length > 16 << 20) throw new Error('backup too large');
  const r = JSON.parse(text);
  if (r.format !== FORMAT || r.version !== 1 || typeof r.name !== 'string' || r.name.length > 256 ||
      !Array.isArray(r.sectors) || r.sectors.length > 256) throw new Error('unsupported emulator save');
  if (typeof r.hash !== 'string' || !/^[a-f0-9]{64}$/.test(r.hash)) throw new Error('invalid firmware hash');
  const byo = r.package == null;
  const pkg = byo ? await resolvePackage(r.hash) : decode(r.package, MAX_PACKAGE);
  if (!pkg) throw new Error('open the matching .fwsc firmware first, then import this save again');
  if (!(pkg instanceof Uint8Array) || pkg.length < 20 * 0x30 || pkg.length > MAX_PACKAGE || await packageHash(pkg) !== r.hash) throw new Error('firmware hash mismatch');
  const sectors = r.sectors.map(s => ({ at: s.at, data: decode(s.data, SECTOR_SIZE) }));
  validateSectors(sectors);
  return { version: 1, hash: r.hash, name: r.name, package: pkg, byo, sectors, revision: 0, url: null };
}

export class SaveStore {
  constructor(factory = globalThis.indexedDB) { this.factory = factory; }
  async open() {
    if (!this.connection) this.connection = new Promise((resolve, reject) => {
      if (!this.factory) { reject(new Error('browser storage unavailable')); return; }
      const request = this.factory.open('fm1-emulator', 2);
      let blocked = false;
      request.onupgradeneeded = () => {
        for (const name of ['packages', 'states', 'entries']) {
          if (!request.result.objectStoreNames.contains(name)) request.result.createObjectStore(name);
        }
      };
      request.onerror = () => reject(request.error);
      request.onblocked = () => { blocked = true; reject(new Error('close other emulator tabs to upgrade storage')); };
      request.onsuccess = () => {
        const db = request.result;
        if (blocked) { db.close(); return; }
        db.onversionchange = () => { db.close(); this.connection = null; };
        resolve(db);
      };
    }).catch(error => { this.connection = null; throw error; });
    return this.connection;
  }
  async get(store, key) {
    const db = await this.open();
    return new Promise((resolve, reject) => {
      const tx = db.transaction(store, 'readonly');
      const request = tx.objectStore(store).get(key);
      tx.oncomplete = () => resolve(request.result);
      tx.onabort = tx.onerror = () => reject(tx.error || new Error('could not read save'));
    });
  }
  async load(hash) {
    const [pkg, state] = await Promise.all([this.get('packages', hash), this.get('states', hash)]);
    if (!pkg || !state) return null;
    if (state.version !== 1) throw new Error('unsupported stored save version');
    validateSectors(state.sectors);
    if (await packageHash(pkg) !== hash) throw new Error('stored firmware hash mismatch');
    return { ...state, hash, package: pkg };
  }
  async remember(hash, entry) {
    const db = await this.open();
    return new Promise((resolve, reject) => {
      const tx = db.transaction('entries', 'readwrite');
      tx.objectStore('entries').put({ hash }, entry);
      tx.oncomplete = resolve;
      tx.onabort = tx.onerror = () => reject(tx.error || new Error('could not remember firmware'));
    });
  }
  async list() {
    const db = await this.open();
    return new Promise((resolve, reject) => {
      const tx = db.transaction('states', 'readonly');
      const request = tx.objectStore('states').getAll();
      tx.oncomplete = () => resolve(request.result);
      tx.onabort = tx.onerror = () => reject(tx.error || new Error('could not list saves'));
    });
  }
  async entry(key) {
    const entry = await this.get('entries', key);
    return entry ? this.load(entry.hash) : null;
  }
  async save(record, entry) {
    if (record.package.length > MAX_PACKAGE) throw new Error('firmware package too large');
    validateSectors(record.sectors);
    const db = await this.open();
    return new Promise((resolve, reject) => {
      const tx = db.transaction(['packages', 'states', 'entries'], 'readwrite');
      const states = tx.objectStore('states');
      let failure;
      const request = states.get(record.hash);
      const revision = record.revision + 1;
      request.onsuccess = () => {
        if ((request.result?.revision || 0) !== record.revision) {
          failure = new Error('another tab changed this save; export your changes, then reload');
          tx.abort();
          return;
        }
        const packages = tx.objectStore('packages');
        const installed = packages.getKey(record.hash);
        installed.onsuccess = () => {
          if (!installed.result) packages.put(record.package, record.hash);
        };
        const { package: _pkg, ...state } = record;
        states.put({ ...state, revision }, record.hash);
        tx.objectStore('entries').put({ hash: record.hash }, entry);
      };
      tx.oncomplete = () => resolve(revision);
      tx.onabort = tx.onerror = () => reject(failure || tx.error || new Error('could not save firmware'));
    });
  }
}

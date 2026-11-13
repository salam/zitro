// zitro.spot/voci: parents upload word lists for their stick's vocabulary trainer and manage them.
// The stick's code comes from the address's #fragment (the QR code on the stick) and goes to the server
// only in the X-Voci-Code header, never in an address. ../api/voci.php reads and keeps the lists.
'use strict';

const LANG = (navigator.language || '').toLowerCase().startsWith('de') ? 'de' : 'en';
const MAX_UPLOAD = 5 * 1024 * 1024;

const T = {
  de: {
    docTitle: 'Wörterlisten · Zitro',
    title: 'Wörterlisten',
    lead: 'Lade die Wörter aus dem Schulbuch hoch, am besten eine Datei pro Unit. Dann sagt dein Kind zu Zitro zum Beispiel «Unit 3 Englisch üben».',
    stick: 'Stick',
    noCodeLead: 'Die Listen gehören zu einem Zitro. Diese Seite braucht den Code deines Sticks, und den bekommst du so:',
    noCodeSteps: [
      'Sag zu deinem Zitro: «Wörterlisten hochladen».',
      'Zitro zeigt einen QR-Code.',
      'Scanne ihn mit der Kamera deines Handys. Dann öffnet sich diese Seite mit den Listen deines Sticks.',
    ],
    badCode: 'Der Link ist unvollständig: Der Code des Sticks fehlt oder stimmt nicht. Scanne den QR-Code nochmal.',
    loading: 'Listen werden geladen …',
    empty: 'Auf diesem Stick sind noch keine Listen. Lade die erste hoch.',
    lists: 'Deine Listen',
    current: '★ aktuell',
    makeCurrent: 'aktuell machen',
    words: (n) => (n === 1 ? '1 Wort' : `${n} Wörter`),
    rename: 'Umbenennen',
    del: 'Löschen',
    cancel: 'Abbrechen',
    save: 'Speichern',
    deleteAsk: (name) => `«${name}» löschen?`,
    deleted: (name) => `«${name}» ist gelöscht.`,
    renamed: (name) => `Gespeichert: ${name}`,
    nowCurrent: (book) => `«${book}» ist jetzt das aktuelle Buch.`,
    book: 'Buch',
    name: 'Name',
    newList: 'Neue Liste',
    upload: 'Datei hochladen',
    drop: 'oder eine Datei hierher ziehen',
    formats: 'CSV, TSV, Quizlet-Text, Anki (.apkg, .txt)',
    formatsHelp: 'Zwei Spalten: Englisch und Deutsch. Eine Excel-Tabelle speicherst du als «CSV UTF-8».',
    reading: 'Wird gelesen …',
    found: (n) => (n === 1 ? '1 Wort erkannt' : `${n} Wörter erkannt`),
    fileOf: (i, n) => `Datei ${i} von ${n}`,
    colT: 'Englisch',
    colB: 'Deutsch',
    more: (n) => `… und ${n} weitere`,
    swap: 'Spalten vertauschen',
    swappedNote: 'Die erste Spalte sah nach Deutsch aus, darum haben wir die Spalten getauscht.',
    replaces: (name, book, n) => `Ersetzt die Liste «${name}» in «${book}» (${n}).`,
    saved: (name, n) => `Gespeichert: ${name} · ${n}`,
    skip: 'Überspringen',
    next: 'Nächste Datei',
    ok: 'OK',
    needNames: 'Gib dem Buch und der Liste einen Namen.',
    privacy: 'Die Listen liegen nur unter dem Code dieses Sticks, ohne Konto, Name oder E-Mail. Holt ein Jahr lang kein Stick sie ab, werden sie gelöscht.',
    wipe: 'Alle Listen löschen',
    wipeAsk: (n) => (n === 1 ? 'Die Liste dieses Sticks löschen? Das lässt sich nicht rückgängig machen.' : `Alle ${n} Listen dieses Sticks löschen? Das lässt sich nicht rückgängig machen.`),
    wipeYes: 'Ja, alle löschen',
    wiped: 'Alle Listen dieses Sticks sind gelöscht.',
    offline: 'Keine Verbindung zum Server. Prüfe das Internet und versuch es nochmal.',
    failed: 'Das hat nicht geklappt. Lade die Seite neu und versuch es nochmal.',
    tooBig: 'Die Datei ist grösser als 5 MB. Eine Wörterliste ist viel kleiner: Speichere nur die Liste als CSV-Datei und lade diese hoch.',
  },
  en: {
    docTitle: 'Word lists · Zitro',
    title: 'Word lists',
    lead: 'Upload the words from the school book, ideally one file per unit. Then your child says to Zitro, for example, “Practise unit 3 English”.',
    stick: 'Stick',
    noCodeLead: 'The lists belong to a Zitro. This page needs your stick\'s code, and this is how you get it:',
    noCodeSteps: [
      'Say to your Zitro: “Upload vocabulary”.',
      'Zitro shows a QR code.',
      'Scan it with your phone\'s camera. This page then opens with your stick\'s lists.',
    ],
    badCode: 'The link is incomplete: the stick\'s code is missing or wrong. Scan the QR code again.',
    loading: 'Loading the lists …',
    empty: 'There are no lists on this stick yet. Upload the first one.',
    lists: 'Your lists',
    current: '★ current',
    makeCurrent: 'make current',
    words: (n) => (n === 1 ? '1 word' : `${n} words`),
    rename: 'Rename',
    del: 'Delete',
    cancel: 'Cancel',
    save: 'Save',
    deleteAsk: (name) => `Delete “${name}”?`,
    deleted: (name) => `“${name}” is deleted.`,
    renamed: (name) => `Saved: ${name}`,
    nowCurrent: (book) => `“${book}” is now the current book.`,
    book: 'Book',
    name: 'Name',
    newList: 'New list',
    upload: 'Upload a file',
    drop: 'or drag a file here',
    formats: 'CSV, TSV, Quizlet text, Anki (.apkg, .txt)',
    formatsHelp: 'Two columns: English and German. Save an Excel table as “CSV UTF-8”.',
    reading: 'Reading …',
    found: (n) => (n === 1 ? '1 word found' : `${n} words found`),
    fileOf: (i, n) => `File ${i} of ${n}`,
    colT: 'English',
    colB: 'German',
    more: (n) => `… and ${n} more`,
    swap: 'Swap columns',
    swappedNote: 'The first column looked German, so we swapped the columns.',
    replaces: (name, book, n) => `Replaces the list “${name}” in “${book}” (${n}).`,
    saved: (name, n) => `Saved: ${name} · ${n}`,
    skip: 'Skip',
    next: 'Next file',
    ok: 'OK',
    needNames: 'Give the book and the list a name.',
    privacy: 'The lists are kept only under this stick\'s code, without an account, name or e-mail. If no stick fetches them for a year, they are deleted.',
    wipe: 'Delete all lists',
    wipeAsk: (n) => (n === 1 ? 'Delete this stick\'s list? This can\'t be undone.' : `Delete all ${n} lists of this stick? This can't be undone.`),
    wipeYes: 'Yes, delete all',
    wiped: 'All lists of this stick are deleted.',
    offline: 'No connection to the server. Check the internet and try again.',
    failed: 'That didn\'t work. Reload the page and try again.',
    tooBig: 'The file is larger than 5 MB. A word list is much smaller: save just the list as a CSV file and upload that.',
  },
};
const t = T[LANG];

const app = document.getElementById('app');
const stickChip = document.getElementById('stick');

let code = '';
let state = null;

function fresh() {
  return {
    index: null,      // {current, lists} from the server
    loading: true,
    busy: false,
    flash: null,      // {where: 'lists'|'upload'|'privacy', kind: 'ok'|'error', text}
    open: null,       // id of the list whose actions are shown
    mode: 'menu',     // 'menu' | 'rename' | 'delete'
    queue: [],        // files still to preview
    total: 0,
    pos: 0,
    preview: null,    // {file, reading} | {file, error} | {file, entries, serverSwapped, flipped, warnings, book, name}
    wipeAsk: false,
  };
}

// ---------- Helpers ----------

function el(tag, cls, text) {
  const node = document.createElement(tag);
  if (cls) node.className = cls;
  if (text !== undefined && text !== null) node.textContent = text;
  return node;
}

function button(text, cls, onClick) {
  const b = el('button', cls, text);
  b.type = 'button';
  b.disabled = state.busy;
  b.addEventListener('click', onClick);
  return b;
}

function field(label, value, onInput, listId) {
  const wrap = el('div');
  const id = 'f' + Math.random().toString(36).slice(2);
  const lab = el('label', null, label);
  lab.htmlFor = id;
  const input = el('input');
  input.id = id;
  input.type = 'text';
  input.maxLength = 60;
  input.value = value;
  input.autocomplete = 'off';
  if (listId) input.setAttribute('list', listId);
  input.addEventListener('input', () => onInput(input.value));
  wrap.append(lab, input);
  return wrap;
}

function box(kind, content) {
  const b = el('div', 'voci-box voci-' + kind);
  if (Array.isArray(content)) {
    const ul = el('ul');
    content.forEach((line) => ul.append(el('li', null, line)));
    b.append(ul);
  } else {
    b.textContent = content;
  }
  b.setAttribute('role', kind === 'error' ? 'alert' : 'status');
  return b;
}

function flashAt(where) {
  return state.flash && state.flash.where === where ? box(state.flash.kind, state.flash.text) : null;
}

function readCode() {
  let raw = '';
  try {
    raw = decodeURIComponent(location.hash.slice(1));
  } catch (e) {
    raw = location.hash.slice(1);
  }
  raw = raw.replace(/[\s-]/g, '').toUpperCase();
  return { raw, code: /^[A-Z2-7]{26}$/.test(raw) ? raw : '' };
}

// One request to the server. Resolves with its JSON, rejects with a message for the parent.
async function api(op, opts = {}) {
  let url = '../api/voci.php?op=' + op;
  if (opts.id) url += '&id=' + encodeURIComponent(opts.id);
  if (LANG === 'en') url += '&lang=en';
  const init = { method: 'GET', headers: { 'X-Voci-Code': code }, cache: 'no-store', referrerPolicy: 'no-referrer' };
  if (opts.form) {
    init.method = 'POST';
    init.body = opts.form;
  } else if (opts.json) {
    init.method = 'POST';
    init.headers['Content-Type'] = 'application/json';
    init.body = JSON.stringify(opts.json);
  }
  let res;
  try {
    res = await fetch(url, init);
  } catch (e) {
    throw new Error(t.offline);
  }
  let body = null;
  try {
    body = await res.json();
  } catch (e) {
    body = null;
  }
  if (!res.ok || !body) {
    if (res.status === 413) throw new Error(t.tooBig);
    throw new Error((body && body.message) || t.failed);
  }
  return body;
}

function books() {
  const byBook = new Map();
  for (const list of state.index.lists) {
    if (!byBook.has(list.book)) byBook.set(list.book, []);
    byBook.get(list.book).push(list);
  }
  const current = state.index.current;
  const names = [...byBook.keys()].sort((a, b) => (b === current) - (a === current));
  return names.map((name) => ({ name, lists: byBook.get(name), current: name === current }));
}

function sameList(book, name) {
  const b = book.trim().toLowerCase();
  const n = name.trim().toLowerCase();
  return (state.index ? state.index.lists : []).find((l) => l.book.toLowerCase() === b && l.name.toLowerCase() === n) || null;
}

function takeIndex(reply) {
  state.index = { current: reply.current, lists: reply.lists };
}

// Runs a change that answers with the new index, then shows okText(reply) where it happened.
async function change(where, action, okText) {
  if (state.busy) return;
  state.busy = true;
  state.flash = null;
  render();
  try {
    const reply = await action();
    takeIndex(reply);
    state.open = null;
    state.mode = 'menu';
    state.wipeAsk = false;
    if (okText) state.flash = { where, kind: 'ok', text: okText(reply) };
  } catch (e) {
    state.flash = { where, kind: 'error', text: e.message };
  }
  state.busy = false;
  render();
}

// ---------- Lists ----------

function renderLists() {
  const section = el('section', 'voci-section');
  section.setAttribute('aria-label', t.lists);
  const flash = flashAt('lists');
  if (flash) section.append(flash);
  if (state.loading) {
    section.append(el('p', 'voci-note', t.loading));
    return section;
  }
  if (!state.index) return section;
  if (!state.index.lists.length) {
    section.append(el('p', 'voci-note', t.empty));
    return section;
  }
  for (const book of books()) {
    const card = el('article', 'voci-card' + (book.current ? ' is-current' : ''));
    const head = el('div', 'voci-book-head');
    head.append(el('h2', null, book.name));
    if (book.current) {
      head.append(el('span', 'voci-badge', t.current));
    } else {
      head.append(button(t.makeCurrent, 'btn btn-ghost btn-small', () =>
        change('lists', () => api('current', { json: { book: book.name } }), () => t.nowCurrent(book.name))));
    }
    const units = el('div', 'voci-units');
    for (const list of book.lists) {
      const chip = button('', 'voci-unit', () => {
        state.open = state.open === list.id ? null : list.id;
        state.mode = 'menu';
        state.flash = null;
        render();
      });
      chip.append(document.createTextNode(list.name + ' '), el('span', 'voci-count', '· ' + list.count));
      chip.setAttribute('aria-expanded', String(state.open === list.id));
      chip.title = t.words(list.count);
      units.append(chip);
    }
    card.append(head, units);
    const open = book.lists.find((l) => l.id === state.open);
    if (open) card.append(renderActions(open));
    section.append(card);
  }
  return section;
}

function renderActions(list) {
  const wrap = el('div', 'voci-actions');
  if (state.mode === 'rename') {
    const draft = state.draft;
    const fields = el('div', 'voci-fields');
    fields.style.marginTop = '0';
    fields.append(
      field(t.book, draft.book, (v) => { draft.book = v; }, 'voci-books'),
      field(t.name, draft.name, (v) => { draft.name = v; }),
    );
    const row = el('div', 'voci-row');
    row.append(
      button(t.save, 'btn btn-primary btn-small', () => {
        if (!draft.book.trim() || !draft.name.trim()) {
          state.flash = { where: 'lists', kind: 'error', text: t.needNames };
          render();
          return;
        }
        change('lists', () => api('rename', { json: { id: list.id, book: draft.book.trim(), name: draft.name.trim() } }), () => t.renamed(draft.name.trim()));
      }),
      button(t.cancel, 'btn btn-ghost btn-small', () => { state.mode = 'menu'; render(); }),
    );
    wrap.append(fields, row);
    return wrap;
  }
  if (state.mode === 'delete') {
    wrap.append(el('p', null, t.deleteAsk(list.name)));
    const row = el('div', 'voci-row');
    row.append(
      button(t.del, 'btn btn-danger btn-small', () => change('lists', () => api('delete', { json: { id: list.id } }), () => t.deleted(list.name))),
      button(t.cancel, 'btn btn-ghost btn-small', () => { state.mode = 'menu'; render(); }),
    );
    wrap.append(row);
    return wrap;
  }
  wrap.append(el('p', null, `${list.name} · ${t.words(list.count)}`));
  const row = el('div', 'voci-row');
  row.append(
    button(t.rename, 'btn btn-outline btn-small', () => {
      state.mode = 'rename';
      state.draft = { book: list.book, name: list.name };
      render();
    }),
    button(t.del, 'btn btn-danger-line btn-small', () => { state.mode = 'delete'; render(); }),
  );
  wrap.append(row);
  return wrap;
}

// ---------- Upload and preview ----------

function renderUpload() {
  const section = el('section', 'voci-section');
  section.id = 'upload';
  section.append(el('h2', null, t.newList));
  const flash = flashAt('upload');
  if (flash) section.append(flash);
  if (state.preview) {
    section.append(renderPreview(state.preview));
    return section;
  }
  const drop = el('div', 'voci-drop');
  const input = el('input', 'voci-file-input');
  input.type = 'file';
  input.multiple = true;
  input.id = 'voci-file';
  input.tabIndex = -1;
  input.addEventListener('change', () => {
    const files = Array.from(input.files || []);
    input.value = '';
    chooseFiles(files);
  });
  const pick = button(t.upload, 'btn btn-primary', () => input.click());
  drop.append(input, pick, el('p', 'voci-note', t.formats), el('p', 'voci-note', t.formatsHelp));
  if (window.matchMedia && window.matchMedia('(hover: hover)').matches) drop.insertBefore(el('p', 'voci-note', t.drop), drop.children[2]);
  drop.addEventListener('dragover', (e) => { e.preventDefault(); drop.classList.add('is-over'); });
  drop.addEventListener('dragleave', () => drop.classList.remove('is-over'));
  drop.addEventListener('drop', (e) => {
    e.preventDefault();
    drop.classList.remove('is-over');
    chooseFiles(Array.from((e.dataTransfer && e.dataTransfer.files) || []));
  });
  section.append(drop);
  return section;
}

function renderPreview(p) {
  const card = el('div', 'voci-card');
  if (state.total > 1) card.append(el('p', 'voci-note', t.fileOf(state.pos, state.total)));
  card.append(el('p', 'voci-mono', p.file.name));
  const more = state.queue.length > 0;

  if (p.reading) {
    card.append(el('p', 'voci-found', t.reading));
    return card;
  }
  if (p.error) {
    card.append(box('error', p.error));
    const row = el('div', 'voci-row');
    row.append(button(more ? t.next : t.ok, 'btn btn-outline btn-small', () => nextFile()));
    card.append(row);
    return card;
  }

  card.append(el('p', 'voci-found', t.found(p.entries.length)));
  if (p.serverSwapped && !p.flipped) card.append(box('warn', t.swappedNote));
  if (p.warnings.length) card.append(box('warn', p.warnings));

  const table = el('table', 'voci-table');
  const head = el('tr');
  head.append(el('th', null, t.colT), el('th', null, t.colB));
  const thead = el('thead');
  thead.append(head);
  const tbody = el('tbody');
  p.entries.slice(0, 5).forEach((entry) => {
    const tr = el('tr');
    tr.append(el('td', null, entry.t), el('td', null, entry.b));
    tbody.append(tr);
  });
  if (p.entries.length > 5) {
    const tr = el('tr');
    const td = el('td', 'voci-more', t.more(p.entries.length - 5));
    td.colSpan = 2;
    tr.append(td);
    tbody.append(tr);
  }
  table.append(thead, tbody);
  card.append(table);
  const swapRow = el('div', 'voci-row');
  swapRow.style.marginTop = '8px';
  swapRow.append(button(t.swap, 'btn btn-ghost btn-small', () => {
    p.entries = p.entries.map((e) => ({ t: e.b, b: e.t }));
    p.flipped = !p.flipped;
    render();
  }));
  card.append(swapRow);

  const replaceNote = el('p', 'voci-note');
  const updateReplace = () => {
    const same = sameList(p.book, p.name);
    replaceNote.textContent = same ? t.replaces(same.name, same.book, t.words(same.count)) : '';
    replaceNote.hidden = !same;
  };
  const fields = el('div', 'voci-fields');
  fields.append(
    field(t.book, p.book, (v) => { p.book = v; updateReplace(); }, 'voci-books'),
    field(t.name, p.name, (v) => { p.name = v; updateReplace(); }),
  );
  card.append(fields, replaceNote);
  updateReplace();
  replaceNote.style.marginTop = '10px';

  const row = el('div', 'voci-row');
  row.append(
    button(t.save, 'btn btn-primary', () => savePreview()),
    button(more ? t.skip : t.cancel, 'btn btn-ghost btn-small', () => nextFile()),
  );
  card.append(row);
  return card;
}

function chooseFiles(files) {
  if (!files.length || state.busy) return;
  state.queue = files;
  state.total = files.length;
  state.pos = 0;
  state.flash = null;
  nextFile();
}

async function nextFile() {
  const file = state.queue.shift();
  if (!file) {
    state.preview = null;
    state.total = 0;
    render();
    return;
  }
  state.pos += 1;
  if (file.size > MAX_UPLOAD) {
    state.preview = { file, error: t.tooBig };
    render();
    return;
  }
  state.preview = { file, reading: true };
  state.busy = true;
  render();
  scrollToUpload();
  const form = new FormData();
  form.append('file', file, file.name);
  try {
    const data = await api('parse', { form });
    const known = (state.index ? state.index.lists : []).find((l) => l.book.toLowerCase() === data.book.toLowerCase());
    state.preview = {
      file,
      entries: data.entries,
      serverSwapped: data.swapped,
      flipped: false,
      warnings: data.warnings || [],
      book: known ? known.book : data.book,
      name: data.name,
    };
  } catch (e) {
    state.preview = { file, error: e.message };
  }
  state.busy = false;
  render();
}

async function savePreview() {
  const p = state.preview;
  const book = p.book.trim();
  const name = p.name.trim();
  if (!book || !name) {
    state.flash = { where: 'upload', kind: 'error', text: t.needNames };
    render();
    return;
  }
  const same = sameList(book, name);
  const body = { book, name, entries: p.entries };
  if (same) body.id = same.id;
  state.busy = true;
  state.flash = null;
  render();
  try {
    const reply = await api('save', { json: body });
    takeIndex(reply);
    const saved = reply.lists.find((l) => l.id === reply.id);
    const text = [t.saved(saved ? saved.name : name, t.words(saved ? saved.count : p.entries.length))].concat(reply.warnings || []);
    state.flash = { where: 'upload', kind: 'ok', text: text.length > 1 ? text : text[0] };
    state.busy = false;
    await nextFile();
  } catch (e) {
    state.flash = { where: 'upload', kind: 'error', text: e.message };
    state.busy = false;
    render();
  }
}

function scrollToUpload() {
  const target = document.getElementById('upload');
  if (target && target.getBoundingClientRect().top > window.innerHeight * 0.6) target.scrollIntoView({ behavior: 'smooth', block: 'start' });
}

// ---------- Privacy and wipe ----------

function renderPrivacy() {
  const section = el('section', 'voci-privacy');
  section.append(el('p', null, t.privacy));
  const flash = flashAt('privacy');
  if (flash) section.append(flash);
  const count = state.index ? state.index.lists.length : 0;
  if (!count) return section;
  if (state.wipeAsk) {
    const ask = el('div', 'voci-actions');
    ask.append(el('p', null, t.wipeAsk(count)));
    const row = el('div', 'voci-row');
    row.append(
      button(t.wipeYes, 'btn btn-danger btn-small', () => change('privacy', () => api('wipe', { json: {} }), () => t.wiped)),
      button(t.cancel, 'btn btn-ghost btn-small', () => { state.wipeAsk = false; render(); }),
    );
    ask.append(row);
    section.append(ask);
  } else {
    section.append(button(t.wipe, 'btn btn-danger-line btn-small', () => { state.wipeAsk = true; state.flash = null; render(); }));
  }
  return section;
}

// ---------- Page ----------

function render() {
  const keepFocus = document.activeElement && document.activeElement.id === 'voci-file';
  app.replaceChildren();
  app.append(el('h1', null, t.title));
  if (!code) {
    app.append(renderNoCode());
    return;
  }
  app.append(el('p', 'voci-lead', t.lead));
  const datalist = el('datalist');
  datalist.id = 'voci-books';
  if (state.index) [...new Set(state.index.lists.map((l) => l.book))].forEach((b) => { const o = el('option'); o.value = b; datalist.append(o); });
  app.append(datalist, renderLists(), renderUpload(), renderPrivacy());
  if (keepFocus) document.getElementById('voci-file')?.focus();
}

function renderNoCode() {
  const card = el('div', 'voci-card');
  const { raw } = readCode();
  if (raw) card.append(box('error', t.badCode));
  card.append(el('p', null, t.noCodeLead));
  const steps = el('ol', 'voci-steps');
  t.noCodeSteps.forEach((s) => steps.append(el('li', null, s)));
  card.append(steps, el('p', 'voci-note', t.privacy));
  return card;
}

async function start() {
  document.documentElement.lang = LANG;
  document.title = t.docTitle;
  code = readCode().code;
  state = fresh();
  stickChip.hidden = !code;
  stickChip.textContent = code ? `${t.stick} ••${code.slice(-3)}` : '';
  render();
  if (!code) return;
  const mine = state;
  let reply = null;
  let error = null;
  try {
    reply = await api('index');
  } catch (e) {
    error = e.message;
  }
  if (state !== mine) return;  // The address changed to another stick meanwhile
  if (reply) takeIndex(reply);
  else state.flash = { where: 'lists', kind: 'error', text: error };
  state.loading = false;
  render();
}

window.addEventListener('hashchange', start);
start();

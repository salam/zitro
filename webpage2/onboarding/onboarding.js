// zitro.spot/onboarding: one set of steps, shown one at a time (assistant) or all together (page).
// Press names match the stick held with the M5 button on the left.

const UI = {
  de: {
    title: 'Los geht\'s mit Zitro', intro: 'Nimm deinen Zitro in die Hand und mach mit. Halte ihn so, dass der M5-Knopf links ist. Knöpfe, die du drücken sollst, leuchten gelb.',
    assistant: 'Assistent', page: 'Eine Seite', step: 'Schritt', of: 'von', next: 'Weiter', back: 'Zurück', last: 'Fertig', again: 'Nochmal von vorn',
    help: 'Klappt nicht?', docTitle: 'Los geht\'s mit Zitro',
  },
  en: {
    title: 'Get started with Zitro', intro: 'Take your Zitro in your hand and follow along. Hold it with the M5 button on the left. Buttons to press light up yellow.',
    assistant: 'Assistant', page: 'One page', step: 'Step', of: 'of', next: 'Next', back: 'Back', last: 'Done', again: 'Start over',
    help: 'Not working?', docTitle: 'Get started with Zitro',
  },
};

const TILES = [
  ['Drucken', 'Print', 'Bild zeigen, Knopf B 1 Sekunde halten', 'Show a picture, hold B for 1 second', true],
  ['Zeitstrahl', 'Timeline', '«Zeitstrahl der Raumfahrt»', '“Timeline of spaceflight”'],
  ['Moleküle', 'Molecule', '«Wie ist Koffein aufgebaut?»', '“How is caffeine built?”'],
  ['Körper', 'Body', '«Was ist im Herz?»', '“What\'s inside the heart?”'],
  ['Globus', 'Globe', '«Wo liegt das Matterhorn?»', '“Where is the Matterhorn?”'],
  ['ISS live', 'ISS live', '«Wo ist die ISS gerade?»', '“Where is the ISS?”'],
  ['Physik', 'Physics', '«Wie funktioniert ein Hebel?»', '“How does a lever work?”'],
  ['Kurven', 'Curves', '«Zeichne sin(x)»', '“Draw sin(x)”'],
  ['Rechnen', 'Math', '«Was ist 12 mal 34?»', '“What is 12 times 34?”'],
  ['Timer', 'Timer', '«Timer auf 5 Minuten»', '“Timer for 5 minutes”'],
  ['Rezepte', 'Recipes', '«Wie mache ich Pfannkuchen?»', '“How do I make pancakes?”'],
  ['Weltuhr', 'Clock', '«Wie viel Uhr ist es in Tokio?»', '“What time is it in Tokyo?”'],
  ['Vorlesen', 'Speak', 'M5 zweimal schnell, dann sprechen', 'Press M5 twice quickly, then speak'],
  ['Schütteln', 'Shake', 'Zitro überrascht dich', 'Zitro surprises you'],
  ['KI-Bilder', 'AI art', '«Male einen Drachen mit Hut»', '“Paint a dragon with a hat”'],
  ['Kamera', 'Camera', 'M5 dreimal, mit Kamera-Modul', 'M5 three times, with camera module'],
];

// shot: a real capture from the stick. keys: which buttons light up. move: how the picture of the stick moves.
const STEPS = [
  {
    id: 'charge', shot: 'splash', keys: ['c'],
    de: { h: 'Einschalten', do: 'Drück den Knopf oben an der Seite, bis das Logo erscheint.', expect: 'Zwei kleine Plopp-Töne, dann verbindet sich Zitro mit dem WLAN.',
      help: 'Bleibt der Bildschirm dunkel, lade den Stick ein paar Minuten über USB-C und versuch es nochmal.' },
    en: { h: 'Switch it on', do: 'Press the button on the upper side until the logo appears.', expect: 'Two little pops, then Zitro connects to your WiFi.',
      help: 'If the screen stays dark, charge the stick over USB-C for a few minutes and try again.' },
  },
  {
    id: 'setup', shot: 'setup', keys: [],
    de: { h: 'Nur beim ersten Mal: einrichten', do: 'Steht «SETUP» auf dem Bildschirm, verbinde dein Handy mit dem WLAN <b>zitro</b> und öffne <b>zitro.local</b>.', expect: 'Dort wählst du deine KI, trägst den Schlüssel ein und fügst dein WLAN hinzu. Siehst du schon «Hallo, ich bin Zitro!», ist alles eingerichtet: weiter zum nächsten Schritt.',
      help: 'Öffnet sich die Seite nicht von selbst, gib 192.168.4.1 im Browser ein. Fürs Internet musst du dich danach wieder mit deinem normalen WLAN verbinden, um hier weiterzulesen.' },
    en: { h: 'First time only: set up', do: 'If the screen says “SETUP”, join the WiFi called <b>zitro</b> on your phone and open <b>zitro.local</b>.', expect: 'Choose your AI, enter its key and add your WiFi. If you already see “Hi, I\'m Zitro!”, you\'re set up: go to the next step.',
      help: 'If the page doesn\'t open by itself, type 192.168.4.1 in your browser. To keep reading here, switch your phone back to your normal WiFi afterwards.' },
  },
  {
    id: 'ask', shot: 'ask', keys: ['a'],
    de: { h: 'Deine erste Frage', do: 'Drück <b>M5</b> einmal. Wenn die Kästchen laufen, sag:', say: '«Wie weit ist der Mond weg?»', expect: 'Du hast 5 Sekunden. Zitro zeigt, was es gehört hat, dann kommt die Antwort.',
      help: 'Sprich aus etwa 20 cm Entfernung. Steht da «nicht verstanden», drück M5 und versuch es nochmal. Die Tour startet nicht? Sag «Was kannst du?».' },
    en: { h: 'Your first question', do: 'Press <b>M5</b> once. When the boxes start filling, say:', say: '“How far away is the Moon?”', expect: 'You have 5 seconds. Zitro shows what it heard, then it answers.',
      help: 'Speak from about 20 cm away. If it says “I didn\'t catch that”, press M5 and try again. Tour didn\'t start? Say “What can you do?”.' },
  },
  {
    id: 'answer', shot: 'answer', keys: ['c'],
    de: { h: 'Das ist die Antwort', do: 'Lies die Antwort. Dann drück den Knopf oben (▶).', expect: 'Du kannst alles fragen. Ab jetzt zeigt dir Zitro drei besondere Antworten.' },
    en: { h: 'There\'s your answer', do: 'Read the answer. Then press the upper button (▶).', expect: 'You can ask anything. Now Zitro shows you three special kinds of answer.' },
  },
  {
    id: 'photo', shot: 'photo', keys: ['c'], move: 'upright',
    de: { h: 'Ein Foto', do: 'Stell den Stick hochkant und wieder quer.', expect: 'Zitro wechselt zwischen Bild und Text. Weiter geht es mit ▶.', say: null },
    en: { h: 'A photo', do: 'Stand the stick upright, then lay it flat again.', expect: 'Zitro switches between picture and text. Press ▶ to go on.' },
  },
  {
    id: 'stars', shot: 'sky', keys: ['c'], move: 'turn',
    de: { h: 'Die Sterne', do: 'Dreh den Stick langsam und schau dich um.', expect: 'Orion ist gelb eingezeichnet. Verlierst du ihn, zeigt ein gelber Pfeil den Weg. Weiter mit ▶.' },
    en: { h: 'The stars', do: 'Turn the stick slowly and look around.', expect: 'Orion is drawn in yellow. If you lose it, a yellow arrow shows the way. Press ▶ to go on.' },
  },
  {
    id: 'music', shot: 'music', keys: ['c'],
    de: { h: 'Ein Lied', do: 'Hör zu: Die Note auf der gelben Linie erklingt.', expect: 'Jeder Ton hat seine Farbe. Weiter mit ▶.', help: 'Nichts zu hören? Sag später «Lautstärke auf 200».' },
    en: { h: 'A song', do: 'Listen: the note on the yellow line is the one playing.', expect: 'Every note has its own colour. Press ▶ to go on.', help: 'Can\'t hear anything? Later, say “Set volume to 200”.' },
  },
  {
    id: 'more', shot: 'grid1', shots: ['grid2', 'grid3', 'grid4'], keys: ['c'], move: 'tilt',
    de: { h: 'Was sonst noch geht', do: 'Drück ▶ oder kipp den Stick, um durch die vier Seiten zu blättern.', expect: 'Das kannst du später einfach fragen:', tiles: true },
    en: { h: 'What else it can do', do: 'Press ▶ or tip the stick to slide through the four pages.', expect: 'You can just ask for these later:', tiles: true },
  },
  {
    id: 'menu', shot: 'menu', keys: ['a', 'b'],
    de: { h: 'Etwas zurück? Aufhören?', do: 'Mitten in der Tour fragen <b>M5</b> und der <b>untere Knopf</b> zuerst, was du willst.', expect: 'Weiter, ein Schritt zurück oder Tour beenden. Mit ▼ wählst du aus, mit ✓ bestätigst du.' },
    en: { h: 'Go back? Stop?', do: 'During the tour, <b>M5</b> and the <b>lower button</b> first ask what you want.', expect: 'Continue, one step back or end the tour. ▼ moves, ✓ confirms.' },
  },
  {
    id: 'yourturn', shot: 'done', keys: ['a'],
    de: { h: 'Jetzt du!', do: 'Drück <b>M5</b> und frag, was du willst.', expect: 'Tour nochmal sehen? Sag «Was kannst du?».' },
    en: { h: 'Your turn!', do: 'Press <b>M5</b> and ask anything.', expect: 'Want the tour again? Say “What can you do?”.' },
  },
];

const store = {
  get(key) { try { return localStorage.getItem('zitro-guide-' + key); } catch (e) { return null; } },
  set(key, value) { try { localStorage.setItem('zitro-guide-' + key, value); } catch (e) { /* private window */ } },
};

const root = document.getElementById('guide');
const params = new URLSearchParams(location.search);
const state = {
  lang: ['de', 'en'].includes(params.get('lang')) ? params.get('lang') : (store.get('lang') || ((navigator.language || '').toLowerCase().startsWith('de') ? 'de' : 'en')),
  mode: ['assistant', 'page'].includes(params.get('view')) ? params.get('view') : (store.get('mode') || 'assistant'),
  index: Math.min(Number(store.get('step')) || 0, STEPS.length - 1),
};

function fromHash() {
  const hit = STEPS.findIndex((s) => '#' + s.id === location.hash);
  if (hit >= 0) state.index = hit;
}

function el(tag, className, html) {
  const node = document.createElement(tag);
  if (className) node.className = className;
  if (html !== undefined) node.innerHTML = html;
  return node;
}

function shotUrl(name) {
  return '../images/onboarding/' + state.lang + '-' + name + '.webp';
}

function stickFor(step) {
  const stick = el('div', 'stick' + (step.move ? ' move-' + step.move : ''));
  [['a', 'M5'], ['c', 'C'], ['b', 'B']].forEach(([key, label]) => {
    stick.append(el('span', 'key key-' + key + (step.keys.includes(key) ? ' is-on' : ''), label));
  });
  const img = el('img');
  img.src = shotUrl(step.shot);
  img.alt = step[state.lang].h;
  img.width = 480;
  img.height = 270;
  stick.append(img);
  return stick;
}

function textFor(step, i) {
  const t = step[state.lang];
  const ui = UI[state.lang];
  const box = el('div', 'step-text');
  box.append(el('p', 'step-kicker', ui.step + ' ' + (i + 1) + ' ' + ui.of + ' ' + STEPS.length));
  box.append(el('h2', '', t.h));
  box.append(el('p', 'do', t.do));
  if (t.say) box.append(el('p', 'do', '<span class="say">' + t.say + '</span>'));
  if (t.expect) box.append(el('p', 'expect', t.expect));
  if (t.tiles) {
    const list = el('ul', 'tiles');
    TILES.forEach((tile) => {
      const de = state.lang === 'de';
      const item = el('li', tile[4] ? 'is-key' : '');
      item.append(el('b', '', de ? tile[0] : tile[1]));
      item.append(el('span', '', de ? tile[2] : tile[3]));
      list.append(item);
    });
    box.append(list);
  }
  if (t.help) {
    const help = el('details', 'help');
    help.append(el('summary', '', ui.help));
    help.append(el('p', '', t.help));
    box.append(help);
  }
  return box;
}

function stepNode(step, i) {
  const node = el('article', 'step');
  node.id = step.id;
  const left = el('div');
  left.append(stickFor(step));
  if (step.shots) {
    const more = el('div', 'screens');
    step.shots.forEach((name) => {
      const img = el('img');
      img.src = shotUrl(name);
      img.alt = '';
      more.append(img);
    });
    left.append(more);
  }
  node.append(left, textFor(step, i));
  return node;
}

function renderAssistant() {
  const ui = UI[state.lang];
  root.className = 'guide assistant';
  const bar = el('div', 'progress');
  const fill = el('div');
  fill.style.width = ((state.index + 1) / STEPS.length * 100) + '%';
  bar.append(fill);
  root.append(bar, stepNode(STEPS[state.index], state.index));

  const nav = el('nav', 'guide-nav');
  nav.setAttribute('aria-label', ui.step);
  const back = el('button', 'btn btn-ghost', ui.back);
  back.type = 'button';
  back.disabled = state.index === 0;
  back.addEventListener('click', () => go(state.index - 1));
  const atEnd = state.index === STEPS.length - 1;
  const next = el('button', 'btn btn-primary', atEnd ? ui.again : ui.next);
  next.type = 'button';
  next.addEventListener('click', () => go(atEnd ? 0 : state.index + 1));
  nav.append(back, next);
  root.append(nav);
}

function renderPage() {
  const ui = UI[state.lang];
  root.className = 'guide page';
  const head = el('div', 'page-head');
  head.append(el('h1', '', ui.title), el('p', '', ui.intro));
  const toc = el('ul', 'toc');
  STEPS.forEach((step) => {
    const li = el('li');
    const a = el('a', '', step[state.lang].h);
    a.href = '#' + step.id;
    li.append(a);
    toc.append(li);
  });
  head.append(toc);
  root.append(head);
  STEPS.forEach((step, i) => root.append(stepNode(step, i)));
}

function render() {
  const ui = UI[state.lang];
  document.documentElement.lang = state.lang;
  document.title = ui.docTitle;
  document.getElementById('modeAssistant').textContent = ui.assistant;
  document.getElementById('modePage').textContent = ui.page;
  root.querySelectorAll(':scope > :not(noscript)').forEach((n) => n.remove());
  if (state.mode === 'page') renderPage(); else renderAssistant();
  document.querySelectorAll('[data-mode]').forEach((b) => b.setAttribute('aria-pressed', String(b.dataset.mode === state.mode)));
  document.querySelectorAll('[data-lang]').forEach((b) => b.setAttribute('aria-pressed', String(b.dataset.lang === state.lang)));
}

function go(index) {
  state.index = Math.max(0, Math.min(STEPS.length - 1, index));
  store.set('step', String(state.index));
  history.replaceState(null, '', '#' + STEPS[state.index].id);
  render();
  window.scrollTo(0, 0);
}

function visibleStep() {
  const y = 90;
  let current = state.index;
  document.querySelectorAll('.page .step').forEach((node, i) => {
    if (node.getBoundingClientRect().top <= y) current = i;
  });
  return current;
}

function setMode(mode) {
  if (mode === state.mode) return;
  if (state.mode === 'page') state.index = visibleStep();
  state.mode = mode;
  store.set('mode', mode);
  store.set('step', String(state.index));
  render();
  if (mode === 'page') {
    const node = document.getElementById(STEPS[state.index].id);
    if (node && state.index > 0) node.scrollIntoView();
    else window.scrollTo(0, 0);
  } else {
    window.scrollTo(0, 0);
  }
  keepScreenOn();
}

function setLang(lang) {
  if (lang === state.lang) return;
  const before = state.mode === 'page' ? visibleStep() : state.index;
  state.lang = lang;
  store.set('lang', lang);
  state.index = before;
  render();
  if (state.mode === 'page' && before > 0) document.getElementById(STEPS[before].id).scrollIntoView();
}

// Your hands are on the stick: the phone shouldn't go dark while you read
let wakeLock = null;
async function keepScreenOn() {
  if (state.mode !== 'assistant' || !('wakeLock' in navigator) || wakeLock) return;
  try {
    wakeLock = await navigator.wakeLock.request('screen');
    wakeLock.addEventListener('release', () => { wakeLock = null; });
  } catch (e) { /* Not allowed here: the guide works without it */ }
}

document.querySelectorAll('[data-mode]').forEach((b) => b.addEventListener('click', () => setMode(b.dataset.mode)));
document.querySelectorAll('[data-lang]').forEach((b) => b.addEventListener('click', () => setLang(b.dataset.lang)));
document.addEventListener('keydown', (e) => {
  if (state.mode !== 'assistant' || e.altKey || e.metaKey || e.ctrlKey) return;
  if (e.key === 'ArrowRight' && state.index < STEPS.length - 1) go(state.index + 1);
  if (e.key === 'ArrowLeft' && state.index > 0) go(state.index - 1);
});
document.addEventListener('visibilitychange', () => { if (!document.hidden) keepScreenOn(); });
window.addEventListener('hashchange', () => {
  fromHash();
  if (state.mode === 'assistant') render();
});

fromHash();
render();
if (state.mode === 'page' && location.hash) {
  const node = document.getElementById(location.hash.slice(1));
  if (node) node.scrollIntoView();
}
if (state.mode === 'assistant') window.scrollTo(0, 0);
keepScreenOn();

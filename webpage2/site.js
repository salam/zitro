// Shared behavior for Zitro subpages: languages, mobile nav, scroll reveals,
// and the compare-table column toggle. All lookups are guarded so
// this file works on any page.

// ---------- Languages ----------
// The pages are written in English. i18n/<lang>.json holds the translation of every text block (data-i18n)
// and attribute (data-i18n-a), keyed by its English text; only the chosen language's file is downloaded.
// A block without a translation stays English, and so does a block whose text or links changed since the
// keys were made (its key no longer matches). Choice: ?lang=xx, then the saved choice, then the browser.
// The list below is written by scripts/site_i18n.py: only languages with a file.
// LANGUAGES-BEGIN
const ZITRO_LANGUAGES = [
    ['en', 'English', 'ltr'],
    ['de', 'Deutsch', 'ltr'],
    ['fr', 'Français', 'ltr'],
    ['it', 'Italiano', 'ltr'],
    ['rm', 'Rumantsch', 'ltr'],
    ['es', 'Español', 'ltr'],
    ['pt', 'Português', 'ltr'],
    ['nl', 'Nederlands', 'ltr'],
    ['da', 'Dansk', 'ltr'],
    ['sv', 'Svenska', 'ltr'],
    ['nb', 'Norsk', 'ltr'],
    ['fi', 'Suomi', 'ltr'],
    ['is', 'Íslenska', 'ltr'],
    ['et', 'Eesti', 'ltr'],
    ['lv', 'Latviešu', 'ltr'],
    ['lt', 'Lietuvių', 'ltr'],
    ['pl', 'Polski', 'ltr'],
    ['cs', 'Čeština', 'ltr'],
    ['sk', 'Slovenčina', 'ltr'],
    ['hu', 'Magyar', 'ltr'],
    ['ro', 'Română', 'ltr'],
    ['bg', 'Български', 'ltr'],
    ['el', 'Ελληνικά', 'ltr'],
    ['hr', 'Hrvatski', 'ltr'],
    ['sl', 'Slovenščina', 'ltr'],
    ['bs', 'Bosanski', 'ltr'],
    ['sr', 'Srpski', 'ltr'],
    ['mk', 'Македонски', 'ltr'],
    ['sq', 'Shqip', 'ltr'],
    ['tr', 'Türkçe', 'ltr'],
    ['uk', 'Українська', 'ltr'],
    ['be', 'Беларуская', 'ltr'],
    ['ru', 'Русский', 'ltr'],
    ['ga', 'Gaeilge', 'ltr'],
    ['mt', 'Malti', 'ltr'],
    ['lb', 'Lëtzebuergesch', 'ltr'],
    ['zh', '中文', 'ltr'],
    ['hi', 'हिन्दी', 'ltr'],
    ['ar', 'العربية', 'rtl'],
    ['bn', 'বাংলা', 'ltr']
];
// LANGUAGES-END

(function () {
    const root = document.documentElement;
    const scriptSrc = document.currentScript ? document.currentScript.src : location.href;
    const base = new URL('i18n/', scriptSrc);
    const known = new Map(ZITRO_LANGUAGES.map(l => [l[0], l]));
    const remember = (code) => { try { localStorage.setItem('zitroLang', code); } catch (e) { /* private mode */ } };
    const remembered = () => { try { return localStorage.getItem('zitroLang'); } catch (e) { return null; } };

    const choose = () => {
        const asked = new URLSearchParams(location.search).get('lang');
        if (asked && known.has(asked)) { remember(asked); return asked; }
        const saved = remembered();
        if (saved && known.has(saved)) return saved;
        const wanted = navigator.languages && navigator.languages.length ? navigator.languages : [navigator.language || 'en'];
        for (const tag of wanted) {
            let code = String(tag).toLowerCase().split('-')[0];
            if (code === 'no' || code === 'nn') code = 'nb';
            if (known.has(code)) return code;
        }
        return 'en';
    };

    let dict = null;  // {t: {key: html}, js: {english: translation}}
    const englishHtml = new Map();
    const englishAttr = new Map();
    const decoder = document.createElement('textarea');
    const decode = (s) => { decoder.innerHTML = s; return decoder.value; };

    window.zitroT = (s) => (dict && dict.js && dict.js[s]) || s;

    // Same key as scripts/site_i18n.py: FNV-1a over the visible text and the link targets
    const fnv = (s) => {
        let h = 0x811c9dc5;
        for (let i = 0; i < s.length; i++) { h ^= s.charCodeAt(i); h = Math.imul(h, 0x01000193) >>> 0; }
        return ('0000000' + h.toString(16)).slice(-8);
    };
    const norm = (s) => s.replace(/\s+/g, ' ').trim();
    const blockKey = (el) => fnv([norm(el.textContent)]
        .concat(Array.from(el.querySelectorAll('a[href]'), a => a.getAttribute('href'))).join('\u0001'));

    const apply = (code, data) => {
        dict = data;
        const t = (data && data.t) || {};
        document.querySelectorAll('[data-i18n]').forEach(el => {
            const key = el.getAttribute('data-i18n');
            const tr = t[key];
            if (!englishHtml.has(el)) {
                if (!tr || blockKey(el) !== key) return;  // Changed since the keys were made: stays English
                englishHtml.set(el, el.innerHTML);
            }
            el.innerHTML = tr || englishHtml.get(el);
        });
        document.querySelectorAll('[data-i18n-a]').forEach(el => {
            el.getAttribute('data-i18n-a').split(',').forEach(pair => {
                const [name, key] = pair.split(':');
                const id = name + '\u0000' + key;
                if (!englishAttr.has(el)) englishAttr.set(el, {});
                const orig = englishAttr.get(el);
                if (!(id in orig)) orig[id] = el.getAttribute(name);
                const current = orig[id] !== null && fnv(norm(orig[id])) === key;
                el.setAttribute(name, t[key] && current ? decode(t[key]) : orig[id]);
            });
        });
        const lang = known.get(code) || known.get('en');
        root.lang = code;
        root.dir = lang ? lang[2] : 'ltr';
        root.classList.remove('i18n-wait');
        const pick = document.getElementById('langPick');
        if (pick) {
            pick.querySelector('select').value = code;
            pick.querySelector('.lang-code').textContent = code.toUpperCase();
        }
    };

    const load = (code) => {
        if (code === 'en') { apply('en', null); return; }
        fetch(new URL(code + '.json', base), { cache: 'no-cache' })
            .then(r => { if (!r.ok) throw new Error(r.status); return r.json(); })
            .then(data => apply(code, data))
            .catch(() => apply('en', null));
    };

    // The picker: a globe with the language code, a native select on top for the choice
    const actions = document.querySelector('.header-actions');
    if (actions && ZITRO_LANGUAGES.length > 1) {
        const pick = document.createElement('label');
        pick.className = 'lang-pick';
        pick.id = 'langPick';
        pick.innerHTML = '<span class="lang-globe" aria-hidden="true">🌐</span><span class="lang-code" aria-hidden="true">EN</span>';
        const select = document.createElement('select');
        select.setAttribute('aria-label', 'Language / Sprache');
        ZITRO_LANGUAGES.forEach(([code, name]) => select.add(new Option(name, code)));
        select.addEventListener('change', () => { remember(select.value); load(select.value); });
        pick.appendChild(select);
        actions.insertBefore(pick, actions.firstChild);
    }

    load(choose());
})();


// Mobile navigation
const navToggle = document.getElementById('navToggle');
const siteNav = document.getElementById('siteNav');

if (navToggle && siteNav) {
    navToggle.addEventListener('click', () => {
        const open = siteNav.classList.toggle('open');
        navToggle.setAttribute('aria-expanded', open ? 'true' : 'false');
    });

    siteNav.querySelectorAll('a').forEach(link => {
        link.addEventListener('click', () => {
            siteNav.classList.remove('open');
            navToggle.setAttribute('aria-expanded', 'false');
        });
    });
}

// Compare table: show/hide extra columns
const toggleExtras = document.getElementById('toggleExtras');
if (toggleExtras) {
    const table = document.getElementById('aiWearableCompare');
    const extras = table.querySelectorAll('.extra-col');
    toggleExtras.addEventListener('click', () => {
        const expanded = toggleExtras.getAttribute('aria-expanded') === 'true';
        extras.forEach(el => { el.style.display = expanded ? 'none' : 'table-cell'; });
        toggleExtras.setAttribute('aria-expanded', expanded ? 'false' : 'true');
        toggleExtras.textContent = expanded ? window.zitroT('+ Full list') : window.zitroT('– Hide extras');
    });
}

// Reveal on scroll
const revealEls = document.querySelectorAll('.reveal');
const prefersReducedMotion = window.matchMedia('(prefers-reduced-motion: reduce)').matches;

if (prefersReducedMotion || !('IntersectionObserver' in window)) {
    revealEls.forEach(el => el.classList.add('is-visible'));
} else {
    const revealObserver = new IntersectionObserver((entries) => {
        entries.forEach(entry => {
            if (entry.isIntersecting) {
                entry.target.classList.add('is-visible');
                revealObserver.unobserve(entry.target);
            }
        });
    }, { threshold: 0.1, rootMargin: '0px 0px -60px 0px' });

    revealEls.forEach(el => revealObserver.observe(el));
}

// Widget parallax: cards drift a few pixels against each other while
// scrolling. The inline transform takes over from the .reveal translate,
// so those cards fade in via opacity only.
const parallaxEls = document.querySelectorAll('.feature-card, .edge-card, .how-step, .testimonial-card, .device-card');

if (!prefersReducedMotion && parallaxEls.length && window.matchMedia('(min-width: 561px)').matches) {
    const speeds = [0.055, -0.04, 0.025, -0.055, 0.04, -0.025];
    const items = Array.from(parallaxEls).map((el, i) => ({
        el,
        factor: speeds[i % speeds.length],
        cur: 0
    }));

    items.forEach(item => {
        item.el.style.transition = 'opacity 0.7s cubic-bezier(0.22, 1, 0.36, 1), box-shadow 0.35s cubic-bezier(0.22, 1, 0.36, 1)';
    });

    let ticking = false;

    const update = () => {
        const mid = window.innerHeight / 2;
        items.forEach(item => {
            const rect = item.el.getBoundingClientRect();
            // subtract the current offset so the drift doesn't feed back
            const center = rect.top + rect.height / 2 - item.cur;
            const off = Math.max(-32, Math.min(32, (center - mid) * item.factor));
            item.cur = off;
            item.el.style.transform = 'translateY(' + off.toFixed(1) + 'px)';
        });
        ticking = false;
    };

    window.addEventListener('scroll', () => {
        if (!ticking) {
            ticking = true;
            requestAnimationFrame(update);
        }
    }, { passive: true });

    window.addEventListener('resize', () => {
        if (!ticking) {
            ticking = true;
            requestAnimationFrame(update);
        }
    }, { passive: true });

    update();
}

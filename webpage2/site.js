// Shared behavior for Zitro subpages: mobile nav, scroll reveals,
// and the compare-table column toggle. All lookups are guarded so
// this file works on any page.

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
        toggleExtras.textContent = expanded ? '+ Full list' : '– Hide extras';
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

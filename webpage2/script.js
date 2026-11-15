// Translated text for the chosen language (site.js loads it); English when there is none
const T = (s) => (window.zitroT ? window.zitroT(s) : s);

// DOM Elements
const downloadBtn = document.getElementById('downloadBtn');
const downloadBtnTop = document.getElementById('downloadBtnTop');
const deviceQuestion = document.getElementById('deviceQuestion');
const hasDeviceS3 = document.getElementById('hasDeviceS3');
const hasDevicePlus2 = document.getElementById('hasDevicePlus2');
const hasDeviceNo = document.getElementById('hasDeviceNo');
const emailForm = document.getElementById('emailForm');
const downloadForm = document.getElementById('downloadForm');
const emailInput = document.getElementById('emailInput');
const successMessage = document.getElementById('successMessage');
const platformBtns = document.querySelectorAll('.platform-btn');
const githubLink = document.getElementById('githubLink');
const starCount = document.getElementById('starCount');
const newsletterForm = document.getElementById('newsletterForm');
const newsletterEmail = document.getElementById('newsletterEmail');
// State
let selectedPlatform = 'mac';
let hasDevice = 'unknown';

// Get Zitro flow
function startDownloadFlow() {
    downloadBtn.style.display = 'none';
    deviceQuestion.classList.remove('hidden');
    deviceQuestion.scrollIntoView({ behavior: 'smooth', block: 'center' });
}

downloadBtn.addEventListener('click', startDownloadFlow);
downloadBtnTop.addEventListener('click', () => {
    if (successMessage.classList.contains('hidden') && emailForm.classList.contains('hidden')) {
        startDownloadFlow();
    } else {
        deviceQuestion.scrollIntoView({ behavior: 'smooth', block: 'center' });
    }
});

// Device question handlers: the stick goes with the request, as the StickS3 gets its own installer
function chooseDevice(device) {
    hasDevice = device;
    deviceQuestion.classList.add('hidden');
    emailForm.classList.remove('hidden');
}

hasDeviceS3.addEventListener('click', () => chooseDevice('s3'));
hasDevicePlus2.addEventListener('click', () => chooseDevice('plus2'));

hasDeviceNo.addEventListener('click', () => {
    hasDevice = 'no';
    // Redirect to shop page
    window.location.href = 'shop.html';
});

// Platform toggle
platformBtns.forEach(btn => {
    btn.addEventListener('click', () => {
        platformBtns.forEach(b => b.classList.remove('active'));
        btn.classList.add('active');
        selectedPlatform = btn.dataset.platform;
    });
});

// Form submission
downloadForm.addEventListener('submit', async (e) => {
    e.preventDefault();

    const email = emailInput.value;
    const submitBtn = downloadForm.querySelector('button[type="submit"]');
    const originalText = submitBtn.textContent;

    // Disable button and show loading state
    submitBtn.disabled = true;
    submitBtn.textContent = T('Sending...');

    try {
        const response = await fetch('send-download.php', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                email,
                platform: selectedPlatform,
                hasDevice: hasDevice
            })
        });

        const data = await response.json();

        if (data.success) {
            // Show success message
            emailForm.classList.add('hidden');
            successMessage.classList.remove('hidden');
        } else {
            alert(T('Error: {message}').replace('{message}', data.message));
            submitBtn.disabled = false;
            submitBtn.textContent = originalText;
        }
    } catch (error) {
        console.error('Error submitting download request:', error);
        alert(T('Failed to send download link. Please try again.'));
        submitBtn.disabled = false;
        submitBtn.textContent = originalText;
    }
});

// Newsletter subscription
newsletterForm.addEventListener('submit', async (e) => {
    e.preventDefault();

    const email = newsletterEmail.value;
    const submitBtn = newsletterForm.querySelector('button[type="submit"]');
    const originalText = submitBtn.textContent;

    // Disable button and show loading state
    submitBtn.disabled = true;
    submitBtn.textContent = T('Subscribing...');

    try {
        const response = await fetch('subscribe.php', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ email })
        });

        const data = await response.json();

        if (data.success) {
            // Show success feedback
            newsletterEmail.value = '';
            newsletterEmail.placeholder = T('Thanks for subscribing!');

            setTimeout(() => {
                newsletterEmail.placeholder = 'your@email.com';
                submitBtn.disabled = false;
                submitBtn.textContent = originalText;
            }, 3000);
        } else {
            alert(T('Error: {message}').replace('{message}', data.message));
            submitBtn.disabled = false;
            submitBtn.textContent = originalText;
        }
    } catch (error) {
        console.error('Error subscribing to newsletter:', error);
        alert(T('Failed to subscribe. Please try again.'));
        submitBtn.disabled = false;
        submitBtn.textContent = originalText;
    }
});

// Fetch GitHub stars
async function fetchGitHubStars() {
    try {
        // Zitro repository
        const response = await fetch('https://api.github.com/repos/salam/zitro');

        if (response.ok) {
            const data = await response.json();
            const stars = data.stargazers_count;
            starCount.textContent = `★ ${stars.toLocaleString()}`;
        } else {
            starCount.textContent = T('★ Star us!');
        }
    } catch (error) {
        console.error('Error fetching GitHub stars:', error);
        starCount.textContent = T('★ Star us!');
    }
}

// Mobile navigation, scroll reveals, and widget parallax live in site.js.

// Show the hero figure immediately — don't wait for the scroll reveal.
const heroFigure = document.querySelector('.hero-figure.reveal');
if (heroFigure) {
    heroFigure.classList.add('is-visible');
}

// Hero slideshow: start on a random photo, crossfade every 7 seconds.
const heroSlides = document.querySelectorAll('.hero-slide');
if (heroSlides.length > 1) {
    let current = Math.floor(Math.random() * heroSlides.length);
    heroSlides.forEach((slide, i) => slide.classList.toggle('is-active', i === current));

    if (!window.matchMedia('(prefers-reduced-motion: reduce)').matches) {
        setInterval(() => {
            current = (current + 1) % heroSlides.length;
            heroSlides.forEach((slide, i) => slide.classList.toggle('is-active', i === current));
        }, 7000);
    }
}

// Initialize
document.addEventListener('DOMContentLoaded', () => {
    fetchGitHubStars();

    // Check for download deep link
    const urlParams = new URLSearchParams(window.location.search);
    if (urlParams.get('download') === 'true') {
        // "Get Zitro" on the other pages: ask which stick first, like the button here
        downloadBtn.style.display = 'none';
        deviceQuestion.classList.remove('hidden');

        const heroSection = document.querySelector('.hero');
        if (heroSection) {
            heroSection.scrollIntoView({ behavior: 'smooth', block: 'start' });
        }
    }
});

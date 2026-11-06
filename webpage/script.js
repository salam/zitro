// DOM Elements
const downloadBtn = document.getElementById('downloadBtn');
const deviceQuestion = document.getElementById('deviceQuestion');
const hasDeviceYes = document.getElementById('hasDeviceYes');
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

// Download button click handler
downloadBtn.addEventListener('click', () => {
    downloadBtn.style.display = 'none';
    deviceQuestion.classList.remove('hidden');
});

// Device question handlers
hasDeviceYes.addEventListener('click', () => {
    hasDevice = 'yes';
    deviceQuestion.classList.add('hidden');
    emailForm.classList.remove('hidden');
});

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
    submitBtn.textContent = 'Sending...';

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
            alert('Error: ' + data.message);
            submitBtn.disabled = false;
            submitBtn.textContent = originalText;
        }
    } catch (error) {
        console.error('Error submitting download request:', error);
        alert('Failed to send download link. Please try again.');
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
    submitBtn.textContent = 'Subscribing...';

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
            newsletterEmail.placeholder = 'Thanks for subscribing!';

            setTimeout(() => {
                newsletterEmail.placeholder = 'your@email.com';
                submitBtn.disabled = false;
                submitBtn.textContent = originalText;
            }, 3000);
        } else {
            alert('Error: ' + data.message);
            submitBtn.disabled = false;
            submitBtn.textContent = originalText;
        }
    } catch (error) {
        console.error('Error subscribing to newsletter:', error);
        alert('Failed to subscribe. Please try again.');
        submitBtn.disabled = false;
        submitBtn.textContent = originalText;
    }
});

// Fetch GitHub stars
async function fetchGitHubStars() {
    try {
        // Replace 'yourusername/firebird' with your actual GitHub repo
        const response = await fetch('https://api.github.com/repos/yourusername/firebird');

        if (response.ok) {
            const data = await response.json();
            const stars = data.stargazers_count;
            starCount.textContent = `★ ${stars.toLocaleString()}`;
        } else {
            starCount.textContent = '★ Star us!';
        }
    } catch (error) {
        console.error('Error fetching GitHub stars:', error);
        starCount.textContent = '★ Star us!';
    }
}

// Initialize
document.addEventListener('DOMContentLoaded', () => {
    fetchGitHubStars();

    // Check for download deep link
    const urlParams = new URLSearchParams(window.location.search);
    if (urlParams.get('download') === 'true') {
        // Show email form directly without device question
        downloadBtn.style.display = 'none';
        emailForm.classList.remove('hidden');

        // Scroll to hero section
        const heroSection = document.querySelector('.hero');
        if (heroSection) {
            heroSection.scrollIntoView({ behavior: 'smooth', block: 'start' });
        }
    }

    // Duplicate carousel items for infinite scroll
    const carouselTrack = document.querySelector('.carousel-track');
    const items = Array.from(carouselTrack.children);
    items.forEach(item => {
        const clone = item.cloneNode(true);
        carouselTrack.appendChild(clone);
    });
});

// Smooth scroll for navigation links
document.querySelectorAll('a[href^="#"]').forEach(anchor => {
    anchor.addEventListener('click', function (e) {
        e.preventDefault();
        const target = document.querySelector(this.getAttribute('href'));
        if (target) {
            const headerOffset = 80;
            const elementPosition = target.getBoundingClientRect().top;
            const offsetPosition = elementPosition + window.pageYOffset - headerOffset;

            window.scrollTo({
                top: offsetPosition,
                behavior: 'smooth'
            });
        }
    });
});

// Add scroll animation for features
const observerOptions = {
    threshold: 0.1,
    rootMargin: '0px 0px -100px 0px'
};

const observer = new IntersectionObserver((entries) => {
    entries.forEach(entry => {
        if (entry.isIntersecting) {
            entry.target.style.opacity = '0';
            entry.target.style.transform = 'translateY(20px)';

            setTimeout(() => {
                entry.target.style.transition = 'opacity 0.6s ease, transform 0.6s ease';
                entry.target.style.opacity = '1';
                entry.target.style.transform = 'translateY(0)';
            }, 100);

            observer.unobserve(entry.target);
        }
    });
}, observerOptions);

// Observe all feature cards
document.querySelectorAll('.feature-card, .testimonial-card').forEach(card => {
    observer.observe(card);
});

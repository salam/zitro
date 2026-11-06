# Firebird Website

A bold, vibrant landing page for the Firebird AI Stick project with huge colored backgrounds, grotesk typography, and masonry layouts.

## Design Features

- **Bold Brutalist Aesthetic**: Huge colored section backgrounds (hot pink, electric blue, acid green, etc.)
- **Grotesk Typography**: Space Grotesk and Bebas Neue fonts for maximum impact
- **Massive Headers**: 7-8rem titles with uppercase, wide letter-spacing
- **Hard Shadows**: Offset box shadows (8-20px) for depth and dimension
- **True Masonry Layout**: CSS columns for authentic Pinterest-style grid
- **Vibrant Color Blocks**: Each section and card uses bold, neon-inspired colors
- **Thick Borders**: 4-6px borders everywhere for strong visual hierarchy
- **Rotated Elements**: Carousel items with slight rotation for playful feel
- **Interactive Shadows**: Hover effects that shift shadow positions

## Functionality

- **Device Ownership Check**: Asks if user has M5StickC PLUS2 before download
- **Email Capture**: Download links with platform toggle (macOS/Windows)
  - Deep link support: `index.html?download=true` skips device question and shows installer form directly
- **Shop Page**: Pre-flashed device purchase with address form and shipping calculator
  - Dynamic shipping cost calculation by country
  - US shipping restriction: Shows Amazon link and installer download for US customers
  - Stripe checkout integration (requires API key configuration)
  - Complete order form with validation
- **Newsletter Subscription**: Stay updated widget above footer
- Auto-scrolling photo carousel with rotation effects
- Pinterest-style masonry feature grid (3 columns)
- GitHub integration with live star count
- Testimonials with large quote marks
- About section for Matthias
- Fully responsive design

## Setup

### 1. Local Development

**Static HTML (no backend):**
Simply open `index.html` in your browser to view the site locally.

**With PHP Backend:**
To test the full functionality with database storage, run the PHP development server:

```bash
cd webpage
php -S localhost:8000
```

Then open http://localhost:8000 in your browser.

See [PHP_SETUP.md](PHP_SETUP.md) for complete backend setup instructions.

### 2. Customize Content

**GitHub Repository:**
- Update the GitHub URL in `index.html` (search for `yourusername/firebird`)
- Update the API endpoint in `script.js` to match your actual repo

**Images:**
- Hero background image: `../designs/M5StickC.webp` (full-width background)
- Logo: `../designs/logo_optimized.png` (header logo)
- Update testimonial and about section images as desired

**Content:**
- Edit feature cards in the "Features Section"
- Update testimonials with real user quotes
- Customize the "About Matthias" section

### 3. Stripe Integration (Shop Page)

To enable actual payments on the shop page:

1. Sign up for a Stripe account at https://stripe.com
2. Get your publishable key from the Stripe dashboard
3. Update `shop.js` line 24:
   ```javascript
   const stripe = Stripe('pk_live_YOUR_ACTUAL_PUBLISHABLE_KEY');
   ```
4. Set up a backend endpoint to create Stripe Checkout sessions
5. Uncomment the API call in `shop.js` (lines 83-100)

Example backend implementation (Node.js/Express):

```javascript
app.post('/api/create-checkout-session', async (req, res) => {
    const { customer, amount, shippingCost } = req.body;

    const session = await stripe.checkout.sessions.create({
        payment_method_types: ['card'],
        line_items: [{
            price_data: {
                currency: 'usd',
                product_data: { name: 'Firebird AI Stick' },
                unit_amount: amount,
            },
            quantity: 1,
        }],
        mode: 'payment',
        success_url: 'https://yoursite.com/success',
        cancel_url: 'https://yoursite.com/shop',
        customer_email: customer.email,
    });

    res.json({ id: session.id });
});
```

### 4. Email Collection Backend

The current implementation logs emails to the console. To actually send download links:

1. Set up a backend API endpoint (e.g., `/api/send-download-link`)
2. Uncomment and configure the fetch call in `script.js`
3. Implement email sending logic on your server

Example backend implementation (Node.js/Express):

```javascript
app.post('/api/send-download-link', async (req, res) => {
    const { email, platform } = req.body;

    // Store email in database
    // Send download link via email service (SendGrid, Mailgun, etc.)

    const downloadLink = platform === 'mac'
        ? 'https://example.com/downloads/firebird-mac.dmg'
        : 'https://example.com/downloads/firebird-windows.exe';

    // Send email with download link
    await sendEmail(email, downloadLink);

    res.json({ success: true });
});
```

### 4. Deploy

Deploy to any static hosting service:

- **Netlify**: Drag and drop the webpage folder
- **Vercel**: Connect your git repository
- **GitHub Pages**: Push to a gh-pages branch
- **Cloudflare Pages**: Connect to your repo

## Customization

### Colors

Edit CSS variables in `styles.css`:

```css
:root {
    --color-1: #FF6B6B;
    --color-2: #4ECDC4;
    --color-3: #45B7D1;
    /* ... etc */
}
```

### Carousel Images

Replace the Unsplash placeholder URLs in `index.html` with your own portrait-format photos of users.

### Features

Add or remove feature cards in the `.masonry-grid` section. Each card should have:
- A color class (color-1 through color-6)
- An emoji icon
- A 2-3 word title
- A one-sentence description

## License

Same license as the main Firebird project.

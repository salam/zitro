# Firebird Website PHP Backend Setup

This guide explains how to set up and use the PHP backend for handling email subscriptions, download requests, and orders.

## Features

The PHP backend provides:
- **Newsletter subscriptions** - Stores email addresses for newsletter updates
- **Download requests** - Tracks installer download requests with platform and device ownership info
- **Order submissions** - Manages pre-flashed device orders with shipping information
- **Email notifications** - Automatic notifications to matthias+firebird@sala.ch for all submissions
- **Admin dashboard** - View all submissions and statistics

All data is stored in a local SQLite database (`firebird.db`).

## Requirements

- PHP 7.4+ with PDO SQLite extension
- Web server (Apache, Nginx, or PHP built-in server)
- Write permissions for the webpage directory (for SQLite database)

## Installation

### 1. Verify PHP Installation

```bash
php -v
php -m | grep pdo_sqlite  # Should show pdo_sqlite
```

### 2. Test Locally

Navigate to the webpage directory and start PHP's built-in server:

```bash
cd /Users/matthias/Development/ai-stick/webpage
php -S localhost:8000
```

Then open http://localhost:8000 in your browser.

### 3. Deploy to Web Server

Upload all files to your web server:
- `index.html`
- `shop.html`
- `styles.css`
- `script.js`
- `shop.js`
- `db.php`
- `subscribe.php`
- `send-download.php`
- `submit-order.php`
- `admin.php`
- `.htaccess` (for Apache)
- `images/` folder

Ensure the web server has write permissions:

```bash
chmod 755 /path/to/webpage
chmod 644 /path/to/webpage/*.php
```

## Database

The SQLite database (`firebird.db`) is created automatically on first use with three tables:

### newsletter_subscriptions
- `id` - Auto-increment primary key
- `email` - Subscriber email (unique)
- `subscribed_at` - Timestamp
- `ip_address` - Client IP
- `user_agent` - Browser info

### download_requests
- `id` - Auto-increment primary key
- `email` - User email
- `platform` - mac or windows
- `has_device` - yes, no, or unknown
- `requested_at` - Timestamp
- `ip_address` - Client IP
- `user_agent` - Browser info

### orders
- `id` - Auto-increment primary key
- `full_name` - Customer name
- `email` - Customer email
- `address`, `city`, `state`, `zip`, `country` - Shipping address
- `product_price`, `shipping_cost`, `total_cost` - Pricing
- `order_status` - pending, paid, shipped, etc.
- `ordered_at` - Timestamp
- `ip_address` - Client IP
- `user_agent` - Browser info

## API Endpoints

### POST /subscribe.php
Subscribe to newsletter.

**Request:**
```json
{
  "email": "user@example.com"
}
```

**Response:**
```json
{
  "success": true,
  "message": "Thank you for subscribing!",
  "data": { "email": "user@example.com" }
}
```

### POST /send-download.php
Request installer download link.

**Request:**
```json
{
  "email": "user@example.com",
  "platform": "mac",
  "hasDevice": "yes"
}
```

**Response:**
```json
{
  "success": true,
  "message": "Download link sent to your email!",
  "data": {
    "email": "user@example.com",
    "platform": "mac"
  }
}
```

### POST /submit-order.php
Submit device order.

**Request:**
```json
{
  "fullName": "John Doe",
  "email": "john@example.com",
  "address": "123 Main St",
  "city": "Zurich",
  "state": "ZH",
  "zip": "8000",
  "country": "CH",
  "productPrice": 50.00,
  "shippingCost": 5.00,
  "totalCost": 55.00
}
```

**Response:**
```json
{
  "success": true,
  "message": "Order received! Redirecting to payment...",
  "data": {
    "orderId": 1,
    "email": "john@example.com"
  }
}
```

## Admin Dashboard

Access the admin dashboard at `/admin.php`

**Default credentials:**
- Username: `admin`
- Password: `changeme123`

**IMPORTANT:** Change the default password in `admin.php`:

```php
define('ADMIN_USERNAME', 'your_username');
define('ADMIN_PASSWORD', 'your_secure_password');
```

The dashboard displays:
- Total counts for subscriptions, downloads, and orders
- Recent submissions in organized tables
- Order details with status tracking

## Email Notifications

The backend automatically sends notification emails to **matthias+firebird@sala.ch** for all submissions:

- **Newsletter subscriptions** - Includes email and IP address
- **Download requests** - Includes email, platform (mac/windows), device ownership, and IP
- **Orders** - Includes full order details, customer info, shipping address, and pricing

### Email Configuration

Email settings are defined in `db.php`:

```php
define('NOTIFICATION_EMAIL', 'matthias+firebird@sala.ch');
define('FROM_EMAIL', 'noreply@firebird.ai');
define('SITE_NAME', 'Firebird');
```

To change the notification recipient, edit the `NOTIFICATION_EMAIL` constant.

### Email Delivery Requirements

PHP's `mail()` function requires a working mail server. Options:

1. **Shared hosting** - Usually configured automatically
2. **VPS/Dedicated server** - Install and configure sendmail or postfix
3. **SMTP service** - Use a library like PHPMailer with SendGrid, Mailgun, or AWS SES

### Testing Email Notifications

After deployment, test by submitting a form and checking if you receive the notification email.

## User Email Integration

To send download links to users, uncomment and configure the email logic in:

### send-download.php (line ~70)
```php
$downloadLink = $platform === 'mac'
    ? 'https://yoursite.com/downloads/AI-Stick-Flasher-1.0.dmg'
    : 'https://yoursite.com/downloads/AI-Stick-Flasher-1.0.exe';

$subject = 'Your Firebird AI Stick Download Link';
$message = "Download link: $downloadLink";

// Using PHP mail()
mail($email, $subject, $message, 'From: noreply@firebird.ai');

// Or use a service like SendGrid, Mailgun, etc.
```

## Stripe Payment Integration

To enable actual payments in `submit-order.php`:

1. Sign up for Stripe account
2. Update `shop.js` with your publishable key
3. Add backend logic in `submit-order.php`:

```php
require_once 'vendor/autoload.php';
\Stripe\Stripe::setApiKey('sk_live_YOUR_SECRET_KEY');

$session = \Stripe\Checkout\Session::create([
    'payment_method_types' => ['card'],
    'line_items' => [[
        'price_data' => [
            'currency' => 'usd',
            'product_data' => ['name' => 'Firebird AI Stick'],
            'unit_amount' => $totalCost * 100,
        ],
        'quantity' => 1,
    ]],
    'mode' => 'payment',
    'success_url' => 'https://yoursite.com/success',
    'cancel_url' => 'https://yoursite.com/shop',
]);

jsonResponse(true, 'Checkout session created', [
    'sessionId' => $session->id
]);
```

## Security Considerations

1. **Change admin password** immediately
2. **Use HTTPS** in production
3. **Validate all inputs** (already implemented)
4. **Rate limiting** - Add rate limiting to prevent spam
5. **Database backups** - Regularly backup `firebird.db`
6. **Environment variables** - Store API keys in environment variables, not in code

## Backup Database

```bash
# Create backup
cp firebird.db firebird_backup_$(date +%Y%m%d).db

# Restore backup
cp firebird_backup_20260130.db firebird.db
```

## Export Data

Access the database directly:

```bash
sqlite3 firebird.db

# List all tables
.tables

# Export to CSV
.mode csv
.output newsletter.csv
SELECT * FROM newsletter_subscriptions;
.output orders.csv
SELECT * FROM orders;
.quit
```

## Troubleshooting

### Database not writable
```bash
chmod 666 firebird.db
chmod 777 /path/to/webpage  # Temporary, fix permissions properly
```

### PHP not processing
- Ensure `.htaccess` is uploaded (for Apache)
- Check if `mod_php` is enabled
- Verify file permissions

### CORS errors (local testing)
The PHP files include CORS headers for local testing. Remove in production:
```php
// Remove these lines in production:
header('Access-Control-Allow-Origin: *');
header('Access-Control-Allow-Methods: POST, OPTIONS');
header('Access-Control-Allow-Headers: Content-Type');
```

## Production Checklist

- [ ] Change admin password
- [ ] Configure email sending
- [ ] Set up Stripe (optional)
- [ ] Enable HTTPS
- [ ] Remove CORS headers
- [ ] Set up database backups
- [ ] Test all forms
- [ ] Monitor error logs
- [ ] Add rate limiting
- [ ] Configure proper file permissions

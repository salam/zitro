<?php
// Database configuration
define('DB_PATH', __DIR__ . '/firebird.db');

// Email configuration
define('NOTIFICATION_EMAIL', 'matthias+firebird@sala.ch');
define('FROM_EMAIL', 'noreply@firebird.ai');
define('SITE_NAME', 'Firebird');

/**
 * Initialize SQLite database and create tables if they don't exist
 */
function initDatabase() {
    try {
        $db = new PDO('sqlite:' . DB_PATH);
        $db->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);

        // Create newsletter subscriptions table
        $db->exec("CREATE TABLE IF NOT EXISTS newsletter_subscriptions (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            email TEXT NOT NULL UNIQUE,
            subscribed_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            ip_address TEXT,
            user_agent TEXT
        )");

        // Create download requests table
        $db->exec("CREATE TABLE IF NOT EXISTS download_requests (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            email TEXT NOT NULL,
            platform TEXT NOT NULL,
            has_device TEXT,
            requested_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            ip_address TEXT,
            user_agent TEXT
        )");

        // Create orders table
        $db->exec("CREATE TABLE IF NOT EXISTS orders (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            full_name TEXT NOT NULL,
            email TEXT NOT NULL,
            address TEXT NOT NULL,
            city TEXT NOT NULL,
            state TEXT NOT NULL,
            zip TEXT NOT NULL,
            country TEXT NOT NULL,
            product_price REAL NOT NULL,
            shipping_cost REAL NOT NULL,
            total_cost REAL NOT NULL,
            order_status TEXT DEFAULT 'pending',
            ordered_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            ip_address TEXT,
            user_agent TEXT
        )");

        // Create indexes for better performance
        $db->exec("CREATE INDEX IF NOT EXISTS idx_newsletter_email ON newsletter_subscriptions(email)");
        $db->exec("CREATE INDEX IF NOT EXISTS idx_downloads_email ON download_requests(email)");
        $db->exec("CREATE INDEX IF NOT EXISTS idx_orders_email ON orders(email)");

        return $db;
    } catch (PDOException $e) {
        error_log("Database error: " . $e->getMessage());
        throw $e;
    }
}

/**
 * Get database connection
 */
function getDb() {
    static $db = null;
    if ($db === null) {
        $db = initDatabase();
    }
    return $db;
}

/**
 * Validate email address
 */
function validateEmail($email) {
    return filter_var($email, FILTER_VALIDATE_EMAIL) !== false;
}

/**
 * Get client IP address
 */
function getClientIp() {
    if (!empty($_SERVER['HTTP_CLIENT_IP'])) {
        return $_SERVER['HTTP_CLIENT_IP'];
    } elseif (!empty($_SERVER['HTTP_X_FORWARDED_FOR'])) {
        return $_SERVER['HTTP_X_FORWARDED_FOR'];
    } else {
        return $_SERVER['REMOTE_ADDR'] ?? 'unknown';
    }
}

/**
 * Get user agent
 */
function getUserAgent() {
    return $_SERVER['HTTP_USER_AGENT'] ?? 'unknown';
}

/**
 * Send JSON response
 */
function jsonResponse($success, $message, $data = []) {
    header('Content-Type: application/json');
    echo json_encode([
        'success' => $success,
        'message' => $message,
        'data' => $data
    ]);
    exit;
}

/**
 * Send notification email to admin
 */
function sendNotificationEmail($subject, $body) {
    $headers = [
        'From: ' . FROM_EMAIL,
        'Reply-To: ' . FROM_EMAIL,
        'X-Mailer: PHP/' . phpversion(),
        'Content-Type: text/plain; charset=UTF-8'
    ];

    $fullSubject = '[' . SITE_NAME . '] ' . $subject;

    // Add timestamp to body
    $fullBody = "Notification from " . SITE_NAME . "\n";
    $fullBody .= "Time: " . date('Y-m-d H:i:s') . "\n";
    $fullBody .= str_repeat('-', 50) . "\n\n";
    $fullBody .= $body;

    try {
        $result = mail(
            NOTIFICATION_EMAIL,
            $fullSubject,
            $fullBody,
            implode("\r\n", $headers)
        );

        if ($result) {
            error_log("Notification email sent: $subject");
        } else {
            error_log("Failed to send notification email: $subject");
        }

        return $result;
    } catch (Exception $e) {
        error_log("Email error: " . $e->getMessage());
        return false;
    }
}

<?php
require_once 'db.php';

// Enable CORS for local testing
header('Access-Control-Allow-Origin: *');
header('Access-Control-Allow-Methods: POST, OPTIONS');
header('Access-Control-Allow-Headers: Content-Type');

// Handle preflight requests
if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    http_response_code(200);
    exit;
}

// Only accept POST requests
if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    jsonResponse(false, 'Method not allowed');
}

// Get JSON input
$input = json_decode(file_get_contents('php://input'), true);

if (!$input) {
    // Try form data as fallback
    $input = $_POST;
}

// Validate email
$email = trim($input['email'] ?? '');

if (empty($email)) {
    jsonResponse(false, 'Email is required');
}

if (!validateEmail($email)) {
    jsonResponse(false, 'Invalid email address');
}

try {
    $db = getDb();

    // Check if email already subscribed
    $stmt = $db->prepare("SELECT id FROM newsletter_subscriptions WHERE email = ?");
    $stmt->execute([$email]);

    if ($stmt->fetch()) {
        jsonResponse(true, 'You are already subscribed!');
    }

    // Insert new subscription
    $stmt = $db->prepare("
        INSERT INTO newsletter_subscriptions (email, ip_address, user_agent)
        VALUES (?, ?, ?)
    ");

    $stmt->execute([
        $email,
        getClientIp(),
        getUserAgent()
    ]);

    // Log subscription
    error_log("New newsletter subscription: $email");

    // Send notification email
    $notificationBody = "New newsletter subscription!\n\n";
    $notificationBody .= "Email: $email\n";
    $notificationBody .= "IP Address: " . getClientIp() . "\n";
    $notificationBody .= "User Agent: " . getUserAgent() . "\n";

    sendNotificationEmail('New Newsletter Subscription', $notificationBody);

    jsonResponse(true, 'Thank you for subscribing!', [
        'email' => $email
    ]);

} catch (PDOException $e) {
    error_log("Newsletter subscription error: " . $e->getMessage());
    jsonResponse(false, 'Failed to subscribe. Please try again later.');
}

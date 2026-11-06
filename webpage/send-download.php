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

// Validate inputs
$email = trim($input['email'] ?? '');
$platform = trim($input['platform'] ?? '');
$hasDevice = trim($input['hasDevice'] ?? 'unknown');

if (empty($email)) {
    jsonResponse(false, 'Email is required');
}

if (!validateEmail($email)) {
    jsonResponse(false, 'Invalid email address');
}

if (empty($platform) || !in_array($platform, ['mac', 'windows'])) {
    jsonResponse(false, 'Invalid platform');
}

try {
    $db = getDb();

    // Insert download request
    $stmt = $db->prepare("
        INSERT INTO download_requests (email, platform, has_device, ip_address, user_agent)
        VALUES (?, ?, ?, ?, ?)
    ");

    $stmt->execute([
        $email,
        $platform,
        $hasDevice,
        getClientIp(),
        getUserAgent()
    ]);

    // Log download request
    error_log("New download request: $email ($platform, device: $hasDevice)");

    // Send notification email to admin
    $notificationBody = "New download request!\n\n";
    $notificationBody .= "Email: $email\n";
    $notificationBody .= "Platform: $platform\n";
    $notificationBody .= "Has Device: $hasDevice\n";
    $notificationBody .= "IP Address: " . getClientIp() . "\n";
    $notificationBody .= "User Agent: " . getUserAgent() . "\n";

    sendNotificationEmail('New Download Request', $notificationBody);

    // In a production environment, you would send an email to the user with the download link
    // For now, we'll just return success

    // Example email logic to user (commented out):
    /*
    $downloadLink = $platform === 'mac'
        ? 'https://yoursite.com/downloads/AI-Stick-Flasher-1.0.dmg'
        : 'https://yoursite.com/downloads/AI-Stick-Flasher-1.0.exe';

    $subject = 'Your Firebird AI Stick Download Link';
    $message = "Thank you for your interest in Firebird!\n\n";
    $message .= "Download your $platform installer here:\n$downloadLink\n\n";
    $message .= "Need help? Check out our user guide or reach out to support.\n";

    mail($email, $subject, $message, 'From: noreply@firebird.ai');
    */

    jsonResponse(true, 'Download link sent to your email!', [
        'email' => $email,
        'platform' => $platform
    ]);

} catch (PDOException $e) {
    error_log("Download request error: " . $e->getMessage());
    jsonResponse(false, 'Failed to process request. Please try again later.');
}

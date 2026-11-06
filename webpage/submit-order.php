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

// Validate required fields
$requiredFields = [
    'fullName', 'email', 'address', 'city', 'state', 'zip', 'country',
    'productPrice', 'shippingCost', 'totalCost'
];

$errors = [];

foreach ($requiredFields as $field) {
    if (empty($input[$field])) {
        $errors[] = ucfirst(preg_replace('/([A-Z])/', ' $1', $field)) . ' is required';
    }
}

if (!empty($errors)) {
    jsonResponse(false, implode(', ', $errors));
}

// Validate email
if (!validateEmail($input['email'])) {
    jsonResponse(false, 'Invalid email address');
}

// Validate prices
$productPrice = floatval($input['productPrice']);
$shippingCost = floatval($input['shippingCost']);
$totalCost = floatval($input['totalCost']);

if ($productPrice <= 0 || $totalCost <= 0) {
    jsonResponse(false, 'Invalid pricing');
}

// Verify total calculation
if (abs(($productPrice + $shippingCost) - $totalCost) > 0.01) {
    jsonResponse(false, 'Price calculation mismatch');
}

// Check if US order (which should be blocked)
if (strtoupper($input['country']) === 'US') {
    jsonResponse(false, 'We currently don\'t ship to the US. Please check the alternative options on the shop page.');
}

try {
    $db = getDb();

    // Insert order
    $stmt = $db->prepare("
        INSERT INTO orders (
            full_name, email, address, city, state, zip, country,
            product_price, shipping_cost, total_cost, order_status,
            ip_address, user_agent
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    ");

    $stmt->execute([
        trim($input['fullName']),
        trim($input['email']),
        trim($input['address']),
        trim($input['city']),
        trim($input['state']),
        trim($input['zip']),
        trim($input['country']),
        $productPrice,
        $shippingCost,
        $totalCost,
        'pending',
        getClientIp(),
        getUserAgent()
    ]);

    $orderId = $db->lastInsertId();

    // Log order
    error_log("New order #$orderId: {$input['email']} - {$input['country']} - \${$totalCost}");

    // Send notification email to admin
    $notificationBody = "New order received!\n\n";
    $notificationBody .= "ORDER DETAILS:\n";
    $notificationBody .= "Order ID: #$orderId\n";
    $notificationBody .= "Status: pending\n\n";

    $notificationBody .= "CUSTOMER:\n";
    $notificationBody .= "Name: {$input['fullName']}\n";
    $notificationBody .= "Email: {$input['email']}\n\n";

    $notificationBody .= "SHIPPING ADDRESS:\n";
    $notificationBody .= "{$input['address']}\n";
    $notificationBody .= "{$input['city']}, {$input['state']} {$input['zip']}\n";
    $notificationBody .= "{$input['country']}\n\n";

    $notificationBody .= "PRICING:\n";
    $notificationBody .= "Product: $" . number_format($productPrice, 2) . "\n";
    $notificationBody .= "Shipping: $" . number_format($shippingCost, 2) . "\n";
    $notificationBody .= "Total: $" . number_format($totalCost, 2) . "\n\n";

    $notificationBody .= "TECHNICAL INFO:\n";
    $notificationBody .= "IP Address: " . getClientIp() . "\n";
    $notificationBody .= "User Agent: " . getUserAgent() . "\n";

    sendNotificationEmail("New Order #$orderId - $" . number_format($totalCost, 2), $notificationBody);

    // In production, you would:
    // 1. Create a Stripe checkout session here
    // 2. Send confirmation email to customer
    // 3. Return the Stripe session ID to redirect to checkout

    jsonResponse(true, 'Order received! Redirecting to payment...', [
        'orderId' => $orderId,
        'email' => $input['email'],
        // In production, return Stripe session ID:
        // 'sessionId' => $stripeSession->id
    ]);

} catch (PDOException $e) {
    error_log("Order submission error: " . $e->getMessage());
    jsonResponse(false, 'Failed to process order. Please try again later.');
}

<?php
/**
 * Backend Test Script
 * Run this to verify PHP and database setup
 * Access: http://localhost:8000/test-backend.php
 */

header('Content-Type: text/html; charset=utf-8');
?>
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Backend Test - Firebird</title>
    <style>
        body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; padding: 2rem; max-width: 800px; margin: 0 auto; background: #f5f5f5; }
        h1 { color: #1a1a1a; }
        .test { background: white; padding: 1.5rem; margin: 1rem 0; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
        .test h3 { margin-top: 0; color: #333; }
        .success { color: #2e7d32; font-weight: bold; }
        .error { color: #c62828; font-weight: bold; }
        .warning { color: #e65100; font-weight: bold; }
        pre { background: #f9f9f9; padding: 1rem; border-radius: 4px; overflow-x: auto; }
        .badge { display: inline-block; padding: 0.25rem 0.75rem; border-radius: 12px; font-size: 0.875rem; margin: 0.25rem; }
        .badge-success { background: #e8f5e9; color: #2e7d32; }
        .badge-error { background: #ffebee; color: #c62828; }
    </style>
</head>
<body>
    <h1>🔥 Firebird Backend Test</h1>

    <?php
    $allGood = true;

    // Test 1: PHP Version
    echo '<div class="test">';
    echo '<h3>1. PHP Version</h3>';
    $phpVersion = phpversion();
    if (version_compare($phpVersion, '7.4.0', '>=')) {
        echo '<span class="success">✓ PHP ' . htmlspecialchars($phpVersion) . ' (OK)</span>';
    } else {
        echo '<span class="error">✗ PHP ' . htmlspecialchars($phpVersion) . ' (Need 7.4+)</span>';
        $allGood = false;
    }
    echo '</div>';

    // Test 2: PDO SQLite Extension
    echo '<div class="test">';
    echo '<h3>2. PDO SQLite Extension</h3>';
    if (extension_loaded('pdo_sqlite')) {
        echo '<span class="success">✓ PDO SQLite is available</span>';
    } else {
        echo '<span class="error">✗ PDO SQLite extension not found</span>';
        $allGood = false;
    }
    echo '</div>';

    // Test 3: Database Initialization
    echo '<div class="test">';
    echo '<h3>3. Database Initialization</h3>';
    try {
        require_once 'db.php';
        $db = getDb();
        echo '<span class="success">✓ Database initialized successfully</span><br>';
        echo '<small>Location: ' . htmlspecialchars(DB_PATH) . '</small>';
    } catch (Exception $e) {
        echo '<span class="error">✗ Database error: ' . htmlspecialchars($e->getMessage()) . '</span>';
        $allGood = false;
    }
    echo '</div>';

    // Test 4: Tables
    if (isset($db)) {
        echo '<div class="test">';
        echo '<h3>4. Database Tables</h3>';
        $tables = ['newsletter_subscriptions', 'download_requests', 'orders'];
        foreach ($tables as $table) {
            try {
                $result = $db->query("SELECT COUNT(*) FROM $table")->fetchColumn();
                echo '<span class="badge badge-success">✓ ' . htmlspecialchars($table) . ' (' . $result . ' rows)</span>';
            } catch (Exception $e) {
                echo '<span class="badge badge-error">✗ ' . htmlspecialchars($table) . '</span>';
                $allGood = false;
            }
        }
        echo '</div>';
    }

    // Test 5: File Permissions
    echo '<div class="test">';
    echo '<h3>5. File Permissions</h3>';
    $dbFile = __DIR__ . '/firebird.db';
    if (file_exists($dbFile)) {
        if (is_writable($dbFile)) {
            echo '<span class="success">✓ Database file is writable</span>';
        } else {
            echo '<span class="warning">⚠ Database file exists but may not be writable</span>';
        }
    } else {
        if (is_writable(__DIR__)) {
            echo '<span class="success">✓ Directory is writable (database will be created)</span>';
        } else {
            echo '<span class="error">✗ Directory is not writable</span>';
            $allGood = false;
        }
    }
    echo '</div>';

    // Test 6: API Endpoints
    echo '<div class="test">';
    echo '<h3>6. API Endpoints</h3>';
    $endpoints = [
        'subscribe.php' => 'Newsletter subscription',
        'send-download.php' => 'Download requests',
        'submit-order.php' => 'Order submission',
        'admin.php' => 'Admin dashboard'
    ];
    foreach ($endpoints as $file => $desc) {
        if (file_exists(__DIR__ . '/' . $file)) {
            echo '<span class="badge badge-success">✓ ' . htmlspecialchars($file) . '</span> ';
            echo '<small>' . htmlspecialchars($desc) . '</small><br>';
        } else {
            echo '<span class="badge badge-error">✗ ' . htmlspecialchars($file) . ' missing</span><br>';
            $allGood = false;
        }
    }
    echo '</div>';

    // Test 7: Test Data Insert
    if (isset($db)) {
        echo '<div class="test">';
        echo '<h3>7. Database Write Test</h3>';
        try {
            $testEmail = 'test_' . time() . '@example.com';
            $stmt = $db->prepare("INSERT INTO newsletter_subscriptions (email, ip_address, user_agent) VALUES (?, ?, ?)");
            $stmt->execute([$testEmail, '127.0.0.1', 'Test Script']);

            // Verify insert
            $stmt = $db->prepare("SELECT id FROM newsletter_subscriptions WHERE email = ?");
            $stmt->execute([$testEmail]);
            $result = $stmt->fetch();

            if ($result) {
                echo '<span class="success">✓ Successfully wrote and read test data</span><br>';
                echo '<small>Test email: ' . htmlspecialchars($testEmail) . '</small><br>';

                // Clean up test data
                $db->exec("DELETE FROM newsletter_subscriptions WHERE email LIKE 'test_%@example.com'");
                echo '<small class="success">✓ Cleaned up test data</small>';
            } else {
                echo '<span class="error">✗ Could not verify test data</span>';
                $allGood = false;
            }
        } catch (Exception $e) {
            echo '<span class="error">✗ Write test failed: ' . htmlspecialchars($e->getMessage()) . '</span>';
            $allGood = false;
        }
        echo '</div>';
    }

    // Summary
    echo '<div class="test" style="background: ' . ($allGood ? '#e8f5e9' : '#ffebee') . ';">';
    echo '<h3>Summary</h3>';
    if ($allGood) {
        echo '<p class="success">✓ All tests passed! Your backend is ready to use.</p>';
        echo '<p>Next steps:</p>';
        echo '<ul>';
        echo '<li>Access the admin dashboard at <a href="admin.php">admin.php</a></li>';
        echo '<li>Change the admin password in admin.php</li>';
        echo '<li>Test the website forms on <a href="index.html">index.html</a> and <a href="shop.html">shop.html</a></li>';
        echo '<li>Configure email sending in send-download.php (optional)</li>';
        echo '<li>Set up Stripe for payments (optional)</li>';
        echo '</ul>';
    } else {
        echo '<p class="error">✗ Some tests failed. Please fix the issues above.</p>';
        echo '<p>Common solutions:</p>';
        echo '<ul>';
        echo '<li>Ensure PHP 7.4+ is installed with PDO SQLite extension</li>';
        echo '<li>Check file permissions: <code>chmod 755 ' . htmlspecialchars(__DIR__) . '</code></li>';
        echo '<li>Verify all PHP files are uploaded correctly</li>';
        echo '</ul>';
    }
    echo '</div>';

    // Configuration Info
    echo '<div class="test">';
    echo '<h3>Configuration Information</h3>';
    echo '<pre>';
    echo 'PHP Version: ' . phpversion() . "\n";
    echo 'Document Root: ' . htmlspecialchars(__DIR__) . "\n";
    echo 'Database Path: ' . htmlspecialchars(DB_PATH ?? 'N/A') . "\n";
    echo 'Server Software: ' . htmlspecialchars($_SERVER['SERVER_SOFTWARE'] ?? 'Unknown') . "\n";
    echo '</pre>';
    echo '</div>';
    ?>

    <p style="text-align: center; color: #999; margin-top: 2rem;">
        <small>Delete this test file (test-backend.php) before deploying to production</small>
    </p>
</body>
</html>

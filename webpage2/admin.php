<?php
require_once 'db.php';

// Authentication via environment (no credentials in code).
// Set ADMIN_PASSWORD_HASH to the output of: php -r "echo password_hash('your-password', PASSWORD_BCRYPT);"
define('ADMIN_USERNAME', getenv('ADMIN_USERNAME') ?: 'admin');
define('ADMIN_PASSWORD_HASH', getenv('ADMIN_PASSWORD_HASH') ?: '');

session_start();

// Handle login
if ($_SERVER['REQUEST_METHOD'] === 'POST' && isset($_POST['login'])) {
    if (ADMIN_PASSWORD_HASH === '') {
        $loginError = 'Admin login is not configured. Set the ADMIN_PASSWORD_HASH environment variable.';
    } elseif (hash_equals(ADMIN_USERNAME, (string)$_POST['username'])
        && password_verify((string)$_POST['password'], ADMIN_PASSWORD_HASH)) {
        session_regenerate_id(true);
        $_SESSION['admin_logged_in'] = true;
        header('Location: admin.php');
        exit;
    } else {
        $loginError = 'Invalid credentials';
    }
}

// Handle logout
if (isset($_GET['logout'])) {
    session_destroy();
    header('Location: admin.php');
    exit;
}

// Check authentication
if (!isset($_SESSION['admin_logged_in'])) {
    ?>
    <!DOCTYPE html>
    <html lang="en">
    <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <title>Admin Login - Zitro</title>
        <style>
            body { font-family: Arial, sans-serif; background: #f5f5f5; padding: 2rem; }
            .login-container { max-width: 400px; margin: 4rem auto; background: white; padding: 2rem; border-radius: 8px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
            h1 { margin-top: 0; }
            input { width: 100%; padding: 0.75rem; margin: 0.5rem 0; border: 1px solid #ddd; border-radius: 4px; }
            button { width: 100%; padding: 0.75rem; background: #FF1493; color: white; border: none; border-radius: 4px; cursor: pointer; font-weight: bold; }
            button:hover { background: #ff006e; }
            .error { background: #ffebee; color: #c62828; padding: 0.75rem; border-radius: 4px; margin-bottom: 1rem; }
        </style>
    </head>
    <body>
        <div class="login-container">
            <h1>Zitro Admin</h1>
            <?php if (isset($loginError)): ?>
                <div class="error"><?php echo htmlspecialchars($loginError); ?></div>
            <?php endif; ?>
            <form method="POST">
                <input type="text" name="username" placeholder="Username" required>
                <input type="password" name="password" placeholder="Password" required>
                <button type="submit" name="login">Login</button>
            </form>
        </div>
    </body>
    </html>
    <?php
    exit;
}

// Fetch data
try {
    $db = getDb();

    // Get counts
    $newsletterCount = $db->query("SELECT COUNT(*) FROM newsletter_subscriptions")->fetchColumn();
    $downloadsCount = $db->query("SELECT COUNT(*) FROM download_requests")->fetchColumn();
    $ordersCount = $db->query("SELECT COUNT(*) FROM orders")->fetchColumn();

    // Get recent data
    $newsletters = $db->query("SELECT * FROM newsletter_subscriptions ORDER BY subscribed_at DESC LIMIT 50")->fetchAll(PDO::FETCH_ASSOC);
    $downloads = $db->query("SELECT * FROM download_requests ORDER BY requested_at DESC LIMIT 50")->fetchAll(PDO::FETCH_ASSOC);
    $orders = $db->query("SELECT * FROM orders ORDER BY ordered_at DESC LIMIT 50")->fetchAll(PDO::FETCH_ASSOC);

} catch (PDOException $e) {
    die("Database error: " . $e->getMessage());
}
?>
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Admin Dashboard - Zitro</title>
    <style>
        * { box-sizing: border-box; }
        body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: #f5f5f5; padding: 0; margin: 0; }
        .header { background: #1a1a1a; color: white; padding: 1.5rem 2rem; display: flex; justify-content: space-between; align-items: center; }
        .header h1 { margin: 0; }
        .logout-btn { background: #FF1493; color: white; padding: 0.5rem 1.5rem; text-decoration: none; border-radius: 4px; }
        .logout-btn:hover { background: #ff006e; }
        .container { max-width: 1400px; margin: 0 auto; padding: 2rem; }
        .stats { display: grid; grid-template-columns: repeat(auto-fit, minmax(250px, 1fr)); gap: 1.5rem; margin-bottom: 2rem; }
        .stat-card { background: white; padding: 1.5rem; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
        .stat-card h3 { margin: 0 0 0.5rem 0; color: #666; font-size: 0.875rem; text-transform: uppercase; }
        .stat-card .number { font-size: 2.5rem; font-weight: bold; color: #FF1493; }
        .section { background: white; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); margin-bottom: 2rem; overflow: hidden; }
        .section-header { background: #1a1a1a; color: white; padding: 1rem 1.5rem; font-weight: bold; }
        table { width: 100%; border-collapse: collapse; }
        th, td { text-align: left; padding: 1rem 1.5rem; border-bottom: 1px solid #e0e0e0; }
        th { background: #f9f9f9; font-weight: 600; font-size: 0.875rem; text-transform: uppercase; color: #666; }
        tr:hover { background: #f9f9f9; }
        .empty { padding: 2rem; text-align: center; color: #999; }
        .badge { display: inline-block; padding: 0.25rem 0.75rem; border-radius: 12px; font-size: 0.75rem; font-weight: bold; text-transform: uppercase; }
        .badge-pending { background: #fff3e0; color: #e65100; }
        .badge-mac { background: #e3f2fd; color: #1565c0; }
        .badge-windows { background: #f3e5f5; color: #6a1b9a; }
        .badge-yes { background: #e8f5e9; color: #2e7d32; }
        .badge-no { background: #ffebee; color: #c62828; }
    </style>
</head>
<body>
    <div class="header">
        <h1>🍋 Zitro Admin Dashboard</h1>
        <a href="?logout" class="logout-btn">Logout</a>
    </div>

    <div class="container">
        <!-- Statistics -->
        <div class="stats">
            <div class="stat-card">
                <h3>Newsletter Subscriptions</h3>
                <div class="number"><?php echo number_format($newsletterCount); ?></div>
            </div>
            <div class="stat-card">
                <h3>Download Requests</h3>
                <div class="number"><?php echo number_format($downloadsCount); ?></div>
            </div>
            <div class="stat-card">
                <h3>Orders</h3>
                <div class="number"><?php echo number_format($ordersCount); ?></div>
            </div>
        </div>

        <!-- Orders -->
        <div class="section">
            <div class="section-header">Recent Orders (<?php echo $ordersCount; ?>)</div>
            <?php if (empty($orders)): ?>
                <div class="empty">No orders yet</div>
            <?php else: ?>
                <table>
                    <thead>
                        <tr>
                            <th>ID</th>
                            <th>Name</th>
                            <th>Email</th>
                            <th>Country</th>
                            <th>Total</th>
                            <th>Status</th>
                            <th>Date</th>
                        </tr>
                    </thead>
                    <tbody>
                        <?php foreach ($orders as $order): ?>
                            <tr>
                                <td>#<?php echo $order['id']; ?></td>
                                <td><?php echo htmlspecialchars($order['full_name']); ?></td>
                                <td><?php echo htmlspecialchars($order['email']); ?></td>
                                <td><?php echo htmlspecialchars($order['country']); ?></td>
                                <td>$<?php echo number_format($order['total_cost'], 2); ?></td>
                                <td><span class="badge badge-pending"><?php echo htmlspecialchars($order['order_status']); ?></span></td>
                                <td><?php echo date('M j, Y H:i', strtotime($order['ordered_at'])); ?></td>
                            </tr>
                        <?php endforeach; ?>
                    </tbody>
                </table>
            <?php endif; ?>
        </div>

        <!-- Downloads -->
        <div class="section">
            <div class="section-header">Download Requests (<?php echo $downloadsCount; ?>)</div>
            <?php if (empty($downloads)): ?>
                <div class="empty">No download requests yet</div>
            <?php else: ?>
                <table>
                    <thead>
                        <tr>
                            <th>Email</th>
                            <th>Platform</th>
                            <th>Has Device</th>
                            <th>Date</th>
                        </tr>
                    </thead>
                    <tbody>
                        <?php foreach ($downloads as $download): ?>
                            <tr>
                                <td><?php echo htmlspecialchars($download['email']); ?></td>
                                <td><span class="badge badge-<?php echo $download['platform']; ?>"><?php echo htmlspecialchars($download['platform']); ?></span></td>
                                <td><span class="badge badge-<?php echo $download['has_device'] === 'yes' ? 'yes' : 'no'; ?>"><?php echo htmlspecialchars($download['has_device']); ?></span></td>
                                <td><?php echo date('M j, Y H:i', strtotime($download['requested_at'])); ?></td>
                            </tr>
                        <?php endforeach; ?>
                    </tbody>
                </table>
            <?php endif; ?>
        </div>

        <!-- Newsletter -->
        <div class="section">
            <div class="section-header">Newsletter Subscriptions (<?php echo $newsletterCount; ?>)</div>
            <?php if (empty($newsletters)): ?>
                <div class="empty">No newsletter subscriptions yet</div>
            <?php else: ?>
                <table>
                    <thead>
                        <tr>
                            <th>Email</th>
                            <th>Date</th>
                        </tr>
                    </thead>
                    <tbody>
                        <?php foreach ($newsletters as $newsletter): ?>
                            <tr>
                                <td><?php echo htmlspecialchars($newsletter['email']); ?></td>
                                <td><?php echo date('M j, Y H:i', strtotime($newsletter['subscribed_at'])); ?></td>
                            </tr>
                        <?php endforeach; ?>
                    </tbody>
                </table>
            <?php endif; ?>
        </div>
    </div>
</body>
</html>

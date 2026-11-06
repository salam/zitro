// Shipping rates by country
const shippingRates = {
    'US': 99.00,
    'CA': 20.00,
    'GB': 15.00,
    'DE': 15.00,
    'FR': 15.00,
    'IT': 15.00,
    'ES': 15.00,
    'NL': 15.00,
    'BE': 15.00,
    'CH': 5.00,
    'AT': 15.00,
    'AU': 20.00,
    'NZ': 20.00,
    'JP': 20.00
};

const productPrice = 50.00;

// DOM elements
const countrySelect = document.getElementById('country');
const shippingCostEl = document.getElementById('shippingCost');
const shippingCountryEl = document.getElementById('shippingCountry');
const totalCostEl = document.getElementById('totalCost');
const orderForm = document.getElementById('orderForm');
const checkoutButton = document.getElementById('checkoutButton');
const usShippingMessage = document.getElementById('usShippingMessage');
const newsletterForm = document.getElementById('newsletterForm');
const newsletterEmail = document.getElementById('newsletterEmail');

// Initialize Stripe (replace with your publishable key)
const stripe = Stripe('pk_test_YOUR_PUBLISHABLE_KEY_HERE');

// Calculate shipping
function calculateShipping() {
    const country = countrySelect.value;

    if (!country) {
        shippingCostEl.textContent = '$0.00';
        shippingCountryEl.textContent = '-';
        totalCostEl.textContent = '$50.00';
        checkoutButton.classList.remove('hidden');
        usShippingMessage.classList.add('hidden');
        return;
    }

    // Check if US is selected
    if (country === 'US') {
        // Hide checkout button and show US message
        checkoutButton.classList.add('hidden');
        usShippingMessage.classList.remove('hidden');
        shippingCostEl.textContent = '$0.00';
        shippingCountryEl.textContent = 'United States';
        totalCostEl.textContent = '$50.00';
        return;
    }

    // Show checkout button and hide US message for other countries
    checkoutButton.classList.remove('hidden');
    usShippingMessage.classList.add('hidden');

    const shippingCost = shippingRates[country] || 25.00; // Default for unlisted countries
    const total = productPrice + shippingCost;

    shippingCostEl.textContent = `$${shippingCost.toFixed(2)}`;
    shippingCountryEl.textContent = countrySelect.options[countrySelect.selectedIndex].text;
    totalCostEl.textContent = `$${total.toFixed(2)}`;
}

// Country selection handler
countrySelect.addEventListener('change', calculateShipping);

// Form submission
orderForm.addEventListener('submit', async (e) => {
    e.preventDefault();

    // Disable button while processing
    checkoutButton.disabled = true;
    checkoutButton.textContent = 'Processing...';

    // Collect form data
    const formData = {
        fullName: document.getElementById('fullName').value,
        email: document.getElementById('email').value,
        address: document.getElementById('address').value,
        city: document.getElementById('city').value,
        state: document.getElementById('state').value,
        zip: document.getElementById('zip').value,
        country: countrySelect.value
    };

    const shippingCost = shippingRates[formData.country] || 25.00;
    const totalCost = productPrice + shippingCost;

    try {
        // Submit order to backend
        const response = await fetch('submit-order.php', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                ...formData,
                productPrice: productPrice,
                shippingCost: shippingCost,
                totalCost: totalCost
            })
        });

        const data = await response.json();

        if (data.success) {
            // Order saved successfully
            console.log('Order submitted:', data.data);

            // In production with Stripe configured, you would:
            // 1. Use the sessionId from the response to redirect to Stripe
            // if (data.data.sessionId) {
            //     const result = await stripe.redirectToCheckout({
            //         sessionId: data.data.sessionId
            //     });
            //     if (result.error) {
            //         alert(result.error.message);
            //     }
            // }

            // For now, show success message
            alert(`Order received!\n\nOrder ID: #${data.data.orderId}\nTotal: $${totalCost.toFixed(2)}\n\nWe'll contact you at ${formData.email} for payment details.`);

            // Reset form
            orderForm.reset();
            calculateShipping();
        } else {
            alert('Error: ' + data.message);
        }

        // Reset button
        checkoutButton.disabled = false;
        checkoutButton.textContent = 'Proceed to Payment';

    } catch (error) {
        console.error('Error submitting order:', error);
        alert('An error occurred. Please try again.');

        // Reset button
        checkoutButton.disabled = false;
        checkoutButton.textContent = 'Proceed to Payment';
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

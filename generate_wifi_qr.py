#!/usr/bin/env python3
"""
Generate QR code for WiFi setup
"""

import qrcode
import os

# WiFi QR code format for open network
WIFI_QR_DATA = "WIFI:T:nopass;S:zitro;P:;;"

# Output path
OUTPUT_DIR = "designs"
OUTPUT_PATH = os.path.join(OUTPUT_DIR, "wifi_qr.png")

def generate_qr_code(data, output_path, box_size=3, border=1):
    """Generate a QR code and save it as PNG"""
    qr = qrcode.QRCode(
        version=3,  # Fixed 29x29 modules: 93x93 px, the size the setup screen expects
        error_correction=qrcode.constants.ERROR_CORRECT_L,
        box_size=box_size,
        border=border,
    )
    qr.add_data(data)
    qr.make(fit=False)

    # Create image with white background and black QR code
    img = qr.make_image(fill_color="black", back_color="white")
    img.save(output_path)

    print(f"Generated: {output_path} ({img.size[0]}x{img.size[1]})")
    return img.size

if __name__ == "__main__":
    # Create output directory if it doesn't exist
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    print("Generating WiFi QR code...")
    size = generate_qr_code(WIFI_QR_DATA, OUTPUT_PATH)

    print("\nQR code generated successfully!")
    print(f"Output: {OUTPUT_PATH} - {size[0]}x{size[1]}")
    print(f"Scan to connect to WiFi: zitro")
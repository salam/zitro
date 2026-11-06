#!/usr/bin/env python3
"""
IPP (Internet Printing Protocol) Test Client

This script tests printing to a Bonjour/IPP printer by:
1. Querying printer capabilities (Get-Printer-Attributes)
2. Sending a test JPEG image (Print-Job)
3. Reporting results for Arduino integration planning

Requirements:
    pip install requests pillow

Usage:
    python test_ipp_printer.py <printer_ip> [port] [test_image.jpg]

Example:
    python test_ipp_printer.py 192.168.1.100
    python test_ipp_printer.py 192.168.1.100 631 test_image.jpg
"""

import sys
import struct
import requests
from pathlib import Path
from typing import Dict, List, Tuple

# IPP Version (1.1)
IPP_VERSION_MAJOR = 1
IPP_VERSION_MINOR = 1

# IPP Operation IDs
IPP_OP_GET_PRINTER_ATTRIBUTES = 0x000B
IPP_OP_PRINT_JOB = 0x0002

# IPP Status Codes
IPP_STATUS_OK = 0x0000
IPP_STATUS_CLIENT_ERROR = 0x0400
IPP_STATUS_SERVER_ERROR = 0x0500

# IPP Tag Values
IPP_TAG_OPERATION = 0x01
IPP_TAG_JOB = 0x02
IPP_TAG_END = 0x03
IPP_TAG_INTEGER = 0x21
IPP_TAG_ENUM = 0x23
IPP_TAG_URI = 0x45
IPP_TAG_CHARSET = 0x47
IPP_TAG_NATURAL_LANGUAGE = 0x48
IPP_TAG_MIME_TYPE = 0x49
IPP_TAG_KEYWORD = 0x44
IPP_TAG_NAME_WITHOUT_LANGUAGE = 0x42


def create_ipp_request(operation_id: int, request_id: int, attributes: Dict[str, any]) -> bytes:
    """
    Create a binary IPP request.

    This is a simplified implementation for testing purposes.
    Real-world IPP is more complex.
    """
    data = bytearray()

    # IPP version (2 bytes)
    data.append(IPP_VERSION_MAJOR)
    data.append(IPP_VERSION_MINOR)

    # Operation ID (2 bytes)
    data.extend(struct.pack('>H', operation_id))

    # Request ID (4 bytes)
    data.extend(struct.pack('>I', request_id))

    # Operation attributes group
    data.append(IPP_TAG_OPERATION)

    # Add required attributes
    # attributes-charset
    add_attribute(data, IPP_TAG_CHARSET, "attributes-charset", "utf-8")

    # attributes-natural-language
    add_attribute(data, IPP_TAG_NATURAL_LANGUAGE, "attributes-natural-language", "en")

    # Add custom attributes
    for name, value in attributes.items():
        if isinstance(value, str) and value.startswith("ipp://"):
            add_attribute(data, IPP_TAG_URI, name, value)
        elif isinstance(value, str):
            add_attribute(data, IPP_TAG_NAME_WITHOUT_LANGUAGE, name, value)
        elif isinstance(value, int):
            add_attribute_int(data, IPP_TAG_INTEGER, name, value)

    # End of attributes
    data.append(IPP_TAG_END)

    return bytes(data)


def add_attribute(data: bytearray, tag: int, name: str, value: str):
    """Add a string attribute to IPP request."""
    data.append(tag)

    # Name length and name
    name_bytes = name.encode('utf-8')
    data.extend(struct.pack('>H', len(name_bytes)))
    data.extend(name_bytes)

    # Value length and value
    value_bytes = value.encode('utf-8')
    data.extend(struct.pack('>H', len(value_bytes)))
    data.extend(value_bytes)


def add_attribute_int(data: bytearray, tag: int, name: str, value: int):
    """Add an integer attribute to IPP request."""
    data.append(tag)

    # Name length and name
    name_bytes = name.encode('utf-8')
    data.extend(struct.pack('>H', len(name_bytes)))
    data.extend(name_bytes)

    # Value length (always 4 for int) and value
    data.extend(struct.pack('>H', 4))
    data.extend(struct.pack('>I', value))


def parse_ipp_response(response_data: bytes) -> Dict:
    """
    Parse IPP response (simplified).

    This is a basic parser for demonstration.
    Real IPP parsing is more complex.
    """
    if len(response_data) < 8:
        return {"error": "Response too short"}

    version_major = response_data[0]
    version_minor = response_data[1]
    status_code = struct.unpack('>H', response_data[2:4])[0]
    request_id = struct.unpack('>I', response_data[4:8])[0]

    result = {
        "version": f"{version_major}.{version_minor}",
        "status_code": status_code,
        "status": "success" if status_code == IPP_STATUS_OK else "error",
        "request_id": request_id,
        "attributes": {}
    }

    # Try to extract some attributes (simplified)
    # Full IPP parsing would decode all attributes properly

    return result


def get_printer_attributes(printer_ip: str, port: int = 631) -> Dict:
    """Query printer capabilities via Get-Printer-Attributes."""

    printer_uri = f"ipp://{printer_ip}:{port}/ipp/print"

    print(f"\n{'='*60}")
    print(f"Querying Printer Attributes")
    print(f"{'='*60}")
    print(f"Printer URI: {printer_uri}\n")

    # Create IPP request
    request_data = create_ipp_request(
        IPP_OP_GET_PRINTER_ATTRIBUTES,
        1,
        {
            "printer-uri": printer_uri,
            "requested-attributes": "all"
        }
    )

    # Send request
    url = f"http://{printer_ip}:{port}/ipp/print"

    try:
        response = requests.post(
            url,
            data=request_data,
            headers={'Content-Type': 'application/ipp'},
            timeout=10
        )

        print(f"HTTP Status: {response.status_code}")

        if response.status_code == 200:
            result = parse_ipp_response(response.content)
            print(f"IPP Status: {result['status']} (code: {result['status_code']})")
            print(f"Response length: {len(response.content)} bytes")

            if result['status'] == 'success':
                print("\n✓ Printer responded successfully to Get-Printer-Attributes")
                print("  This printer supports IPP!")
            else:
                print("\n✗ Printer returned error status")

            return result
        else:
            print(f"\n✗ HTTP error: {response.status_code}")
            return {"error": f"HTTP {response.status_code}"}

    except requests.exceptions.ConnectionError:
        print(f"\n✗ Connection failed - printer may not be at this address")
        return {"error": "Connection failed"}
    except requests.exceptions.Timeout:
        print(f"\n✗ Request timed out")
        return {"error": "Timeout"}
    except Exception as e:
        print(f"\n✗ Error: {e}")
        return {"error": str(e)}


def print_test_image(printer_ip: str, port: int = 631, image_path: str = None) -> Dict:
    """Send a test JPEG image to the printer."""

    printer_uri = f"ipp://{printer_ip}:{port}/ipp/print"

    print(f"\n{'='*60}")
    print(f"Testing Print Job")
    print(f"{'='*60}")
    print(f"Printer URI: {printer_uri}\n")

    # Create or use test image
    if image_path and Path(image_path).exists():
        print(f"Using image: {image_path}")
        with open(image_path, 'rb') as f:
            image_data = f.read()
    else:
        print("No image provided - creating test pattern...")
        try:
            from PIL import Image, ImageDraw, ImageFont

            # Create a simple test image
            img = Image.new('RGB', (800, 600), color='white')
            draw = ImageDraw.Draw(img)

            # Draw test pattern
            draw.rectangle([50, 50, 750, 550], outline='black', width=5)
            draw.text((400, 300), "IPP Test Print", fill='black', anchor='mm')
            draw.text((400, 350), f"From: {printer_ip}", fill='black', anchor='mm')

            # Save to bytes
            from io import BytesIO
            buffer = BytesIO()
            img.save(buffer, format='JPEG', quality=85)
            image_data = buffer.getvalue()

            print(f"Generated test image: {len(image_data)} bytes")
        except ImportError:
            print("✗ PIL/Pillow not installed - cannot create test image")
            print("  Install with: pip install pillow")
            return {"error": "No image available"}

    # Create IPP Print-Job request
    request_data = create_ipp_request(
        IPP_OP_PRINT_JOB,
        2,
        {
            "printer-uri": printer_uri,
            "requesting-user-name": "test",
            "job-name": "IPP Test Print",
            "document-format": "image/jpeg"
        }
    )

    # Append image data after IPP request
    full_request = request_data + image_data

    print(f"Request size: {len(request_data)} bytes IPP + {len(image_data)} bytes image")

    # Send request
    url = f"http://{printer_ip}:{port}/ipp/print"

    try:
        response = requests.post(
            url,
            data=full_request,
            headers={'Content-Type': 'application/ipp'},
            timeout=30
        )

        print(f"\nHTTP Status: {response.status_code}")

        if response.status_code == 200:
            result = parse_ipp_response(response.content)
            print(f"IPP Status: {result['status']} (code: {result['status_code']})")

            if result['status'] == 'success':
                print("\n✓ Print job submitted successfully!")
                print("  Check your printer - it should print the test image.")
            else:
                print("\n✗ Print job failed")

            return result
        else:
            print(f"\n✗ HTTP error: {response.status_code}")
            print(f"Response: {response.text[:200]}")
            return {"error": f"HTTP {response.status_code}"}

    except Exception as e:
        print(f"\n✗ Error: {e}")
        return {"error": str(e)}


def test_alternative_endpoints(printer_ip: str, port: int = 631):
    """Test alternative printer endpoints that might accept simpler formats."""

    print(f"\n{'='*60}")
    print(f"Testing Alternative Endpoints")
    print(f"{'='*60}\n")

    endpoints = [
        f"http://{printer_ip}:{port}/",
        f"http://{printer_ip}:{port}/ipp/print",
        f"http://{printer_ip}:{port}/ipp/printer",
        f"http://{printer_ip}/",  # Sometimes printers use port 80
    ]

    for endpoint in endpoints:
        try:
            print(f"Testing: {endpoint}")
            response = requests.get(endpoint, timeout=5)
            print(f"  ✓ HTTP {response.status_code} - Endpoint accessible")

            if response.status_code == 200 and len(response.text) > 0:
                print(f"    Content-Type: {response.headers.get('Content-Type', 'unknown')}")
                print(f"    Response length: {len(response.text)} bytes")
        except Exception as e:
            print(f"  ✗ Failed: {e}")
        print()


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)

    printer_ip = sys.argv[1]
    port = int(sys.argv[2]) if len(sys.argv) > 2 else 631
    image_path = sys.argv[3] if len(sys.argv) > 3 else None

    print("\n" + "="*60)
    print("IPP Printer Test Client")
    print("="*60)
    print(f"Target: {printer_ip}:{port}")
    print("="*60)

    # Test 1: Get printer attributes
    result1 = get_printer_attributes(printer_ip, port)

    # Test 2: Alternative endpoints
    test_alternative_endpoints(printer_ip, port)

    # Test 3: Try to print test image
    if result1.get('status') == 'success':
        input("\nPress Enter to send test print job (or Ctrl+C to cancel)...")
        result2 = print_test_image(printer_ip, port, image_path)

    print("\n" + "="*60)
    print("Test Complete")
    print("="*60)
    print("\nNext Steps:")
    print("1. Record which operations succeeded")
    print("2. Note exact endpoints and ports that worked")
    print("3. Check printer output if print job succeeded")
    print("4. Use findings to implement Arduino code")
    print("="*60 + "\n")


if __name__ == "__main__":
    main()

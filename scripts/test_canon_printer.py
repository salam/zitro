#!/usr/bin/env python3
"""
Canon TS Printer IPP Test Script
Tests communication with Canon printer to debug document format issues.
Run this on your Mac to quickly test different approaches without Arduino upload cycle.

Usage:
    python3 test_canon_printer.py <printer_ip> [port]

Example:
    python3 test_canon_printer.py 192.168.1.150
    python3 test_canon_printer.py 192.168.1.150 631
"""

import socket
import struct
import sys
import time
from typing import List, Tuple, Optional

# ANSI color codes for pretty output
class Colors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKCYAN = '\033[96m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'

def print_header(text: str):
    print(f"\n{Colors.HEADER}{Colors.BOLD}{'='*60}{Colors.ENDC}")
    print(f"{Colors.HEADER}{Colors.BOLD}{text:^60}{Colors.ENDC}")
    print(f"{Colors.HEADER}{Colors.BOLD}{'='*60}{Colors.ENDC}\n")

def print_success(text: str):
    print(f"{Colors.OKGREEN}✓ {text}{Colors.ENDC}")

def print_error(text: str):
    print(f"{Colors.FAIL}✗ {text}{Colors.ENDC}")

def print_warning(text: str):
    print(f"{Colors.WARNING}⚠ {text}{Colors.ENDC}")

def print_info(text: str):
    print(f"{Colors.OKCYAN}ℹ {text}{Colors.ENDC}")


def create_ipp_string(tag: int, name: str, value: str) -> bytes:
    """Create an IPP attribute (string type)"""
    data = bytearray()
    data.append(tag)

    # Name
    name_bytes = name.encode('utf-8')
    data.extend(struct.pack('>H', len(name_bytes)))
    data.extend(name_bytes)

    # Value
    value_bytes = value.encode('utf-8')
    data.extend(struct.pack('>H', len(value_bytes)))
    data.extend(value_bytes)

    return bytes(data)


def decode_chunked_http(response: bytes) -> Tuple[bytes, bytes]:
    """
    Decode chunked HTTP response.
    Returns: (headers, body)
    """
    # Find end of headers
    header_end = response.find(b'\r\n\r\n')
    if header_end == -1:
        return response, b''

    headers = response[:header_end + 4]

    # Check if chunked
    if b'chunked' not in headers.lower():
        # Not chunked, body is everything after headers
        return headers, response[header_end + 4:]

    # Decode chunks
    body = bytearray()
    pos = header_end + 4

    while pos < len(response):
        # Find chunk size line
        line_end = response.find(b'\r\n', pos)
        if line_end == -1:
            break

        chunk_size_str = response[pos:line_end].decode('ascii', errors='ignore')
        try:
            chunk_size = int(chunk_size_str, 16)
        except ValueError:
            print_warning(f"Invalid chunk size: '{chunk_size_str}'")
            break

        if chunk_size == 0:
            break  # Last chunk

        # Read chunk data
        pos = line_end + 2
        if pos + chunk_size > len(response):
            print_warning(f"Chunk extends beyond response (need {chunk_size}, have {len(response) - pos})")
            break

        body.extend(response[pos:pos + chunk_size])
        pos += chunk_size + 2  # Skip data + trailing \r\n

    return headers, bytes(body)


def parse_ipp_attributes(ipp_data: bytes) -> dict:
    """
    Parse IPP binary response to extract attributes.
    Returns dict with attribute names as keys.
    """
    if len(ipp_data) < 8:
        return {}

    attributes = {}
    pos = 8  # Skip IPP header (version + status + request-id)

    current_group = None

    while pos < len(ipp_data):
        tag = ipp_data[pos]
        pos += 1

        # End of attributes
        if tag == 0x03:
            break

        # Group tags
        if tag in [0x01, 0x02, 0x03, 0x04, 0x05]:
            current_group = tag
            continue

        # Value tags
        if tag in [0x44, 0x45, 0x47, 0x48, 0x49]:  # String types
            # Read name length
            if pos + 2 > len(ipp_data):
                break
            name_len = struct.unpack('>H', ipp_data[pos:pos+2])[0]
            pos += 2

            # Read name
            if pos + name_len > len(ipp_data):
                break
            name = ipp_data[pos:pos+name_len].decode('utf-8', errors='ignore')
            pos += name_len

            # Read value length
            if pos + 2 > len(ipp_data):
                break
            value_len = struct.unpack('>H', ipp_data[pos:pos+2])[0]
            pos += 2

            # Read value
            if pos + value_len > len(ipp_data):
                break
            value = ipp_data[pos:pos+value_len].decode('utf-8', errors='ignore')
            pos += value_len

            # Store attribute
            if name:
                if name not in attributes:
                    attributes[name] = []
                attributes[name].append(value)
        else:
            # Skip unknown tags
            pos += 1

    return attributes


def test_get_printer_attributes(printer_ip: str, port: int = 631) -> List[str]:
    """
    Query printer for supported document formats using Get-Printer-Attributes.
    Returns list of supported MIME types.
    """
    print_header("TEST 1: Get-Printer-Attributes")

    printer_uri = f"ipp://{printer_ip}:{port}/ipp/print"
    print_info(f"Querying: {printer_uri}")

    # Build IPP request
    ipp_request = bytearray()

    # IPP version 1.1
    ipp_request.extend([0x01, 0x01])

    # Operation: Get-Printer-Attributes (0x000B)
    ipp_request.extend([0x00, 0x0B])

    # Request ID
    ipp_request.extend([0x00, 0x00, 0x00, 0x01])

    # Operation attributes group
    ipp_request.append(0x01)

    # attributes-charset
    ipp_request.extend(create_ipp_string(0x47, "attributes-charset", "utf-8"))

    # attributes-natural-language
    ipp_request.extend(create_ipp_string(0x48, "attributes-natural-language", "en"))

    # printer-uri
    ipp_request.extend(create_ipp_string(0x45, "printer-uri", printer_uri))

    # requested-attributes
    ipp_request.extend(create_ipp_string(0x44, "requested-attributes", "document-format-supported"))

    # End of attributes
    ipp_request.append(0x03)

    # Send HTTP POST
    http_request = (
        f"POST /ipp/print HTTP/1.1\r\n"
        f"Host: {printer_ip}\r\n"
        f"Content-Type: application/ipp\r\n"
        f"Content-Length: {len(ipp_request)}\r\n"
        f"\r\n"
    ).encode('utf-8') + bytes(ipp_request)

    print_info(f"Sending {len(http_request)} bytes...")

    # Connect and send
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(10)

    try:
        sock.connect((printer_ip, port))
        sock.sendall(http_request)

        # Read response
        response = b''
        while True:
            chunk = sock.recv(4096)
            if not chunk:
                break
            response += chunk

            # Check if we got complete response
            if b'0\r\n\r\n' in response:  # End of chunked encoding
                break

        sock.close()

        print_success(f"Received {len(response)} bytes")

        # Decode response
        headers, body = decode_chunked_http(response)

        # Print headers for debugging
        print_info("HTTP Response Headers:")
        header_lines = headers.decode('utf-8', errors='ignore').split('\r\n')
        for line in header_lines[:10]:  # First 10 lines
            if line:
                print(f"  {line}")

        print_info(f"IPP Body: {len(body)} bytes")

        # Parse IPP attributes
        attributes = parse_ipp_attributes(body)

        # Extract document formats
        formats = attributes.get('document-format-supported', [])

        if formats:
            print_success(f"Found {len(formats)} supported formats:")
            for fmt in formats:
                print(f"  • {fmt}")
        else:
            print_warning("No document-format-supported found in response")
            print_info("Searching for MIME types in raw body...")

            # Fallback: search for common MIME types
            formats = []
            mime_types = [
                b'image/jpeg',
                b'image/png',
                b'image/gif',
                b'application/pdf',
                b'application/octet-stream',
                b'image/pwg-raster',
                b'image/urf',
                b'application/postscript',
                b'application/vnd.hp-PCL'
            ]

            for mime in mime_types:
                if mime in body:
                    fmt = mime.decode('utf-8')
                    formats.append(fmt)
                    print(f"  • {fmt}")

        return formats

    except socket.timeout:
        print_error("Connection timeout!")
        return []
    except Exception as e:
        print_error(f"Error: {e}")
        import traceback
        traceback.print_exc()
        return []


def create_test_image() -> bytes:
    """
    Create a minimal valid JPEG image for testing.
    This is a 1x1 pixel red JPEG.
    """
    # Minimal JPEG: 1x1 red pixel
    jpeg_data = bytes([
        0xFF, 0xD8,  # SOI
        0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46, 0x49, 0x46, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00,  # JFIF
        0xFF, 0xDB, 0x00, 0x43, 0x00, 0x08, 0x06, 0x06, 0x07, 0x06, 0x05, 0x08, 0x07, 0x07, 0x07, 0x09, 0x09, 0x08, 0x0A, 0x0C, 0x14, 0x0D, 0x0C, 0x0B, 0x0B, 0x0C, 0x19, 0x12, 0x13, 0x0F, 0x14, 0x1D, 0x1A, 0x1F, 0x1E, 0x1D, 0x1A, 0x1C, 0x1C, 0x20, 0x24, 0x2E, 0x27, 0x20, 0x22, 0x2C, 0x23, 0x1C, 0x1C, 0x28, 0x37, 0x29, 0x2C, 0x30, 0x31, 0x34, 0x34, 0x34, 0x1F, 0x27, 0x39, 0x3D, 0x38, 0x32, 0x3C, 0x2E, 0x33, 0x34, 0x32,  # DQT
        0xFF, 0xC0, 0x00, 0x0B, 0x08, 0x00, 0x01, 0x00, 0x01, 0x01, 0x01, 0x11, 0x00,  # SOF
        0xFF, 0xC4, 0x00, 0x14, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09,  # DHT
        0xFF, 0xC4, 0x00, 0x14, 0x10, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  # DHT
        0xFF, 0xDA, 0x00, 0x08, 0x01, 0x01, 0x00, 0x00, 0x3F, 0x00, 0xD2, 0xCF, 0x20,  # SOS + data
        0xFF, 0xD9  # EOI
    ])
    return jpeg_data


def test_print_job(printer_ip: str, port: int, document_format: str, image_data: bytes) -> bool:
    """
    Test Print-Job operation with specified document format.
    Returns True if print succeeded.
    """
    print_header(f"TEST 2: Print-Job with {document_format}")

    printer_uri = f"ipp://{printer_ip}:{port}/ipp/print"
    print_info(f"Printer: {printer_uri}")
    print_info(f"Format: {document_format}")
    print_info(f"Image size: {len(image_data)} bytes")

    # Build IPP request
    ipp_request = bytearray()

    # IPP version 1.1
    ipp_request.extend([0x01, 0x01])

    # Operation: Print-Job (0x0002)
    ipp_request.extend([0x00, 0x02])

    # Request ID
    ipp_request.extend([0x00, 0x00, 0x00, 0x01])

    # Operation attributes group
    ipp_request.append(0x01)

    # attributes-charset
    ipp_request.extend(create_ipp_string(0x47, "attributes-charset", "utf-8"))

    # attributes-natural-language
    ipp_request.extend(create_ipp_string(0x48, "attributes-natural-language", "en"))

    # printer-uri
    ipp_request.extend(create_ipp_string(0x45, "printer-uri", printer_uri))

    # requesting-user-name
    ipp_request.extend(create_ipp_string(0x42, "requesting-user-name", "ai-stick-test"))

    # job-name
    ipp_request.extend(create_ipp_string(0x42, "job-name", "Canon Test Print"))

    # document-format
    ipp_request.extend(create_ipp_string(0x49, "document-format", document_format))

    # Job attributes group
    ipp_request.append(0x02)

    # End of attributes
    ipp_request.append(0x03)

    # Append image data
    ipp_request.extend(image_data)

    # Send HTTP POST
    http_request = (
        f"POST /ipp/print HTTP/1.1\r\n"
        f"Host: {printer_ip}\r\n"
        f"Content-Type: application/ipp\r\n"
        f"Content-Length: {len(ipp_request)}\r\n"
        f"\r\n"
    ).encode('utf-8') + bytes(ipp_request)

    print_info(f"Sending {len(http_request)} bytes...")

    # Connect and send
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(30)

    try:
        sock.connect((printer_ip, port))
        sock.sendall(http_request)

        # Read response
        response = b''
        start_time = time.time()

        while time.time() - start_time < 30:
            try:
                chunk = sock.recv(4096)
                if not chunk:
                    break
                response += chunk

                # Check if we got complete response
                if b'0\r\n\r\n' in response or (b'HTTP/1.1 200' in response and len(response) > 200):
                    break
            except socket.timeout:
                if len(response) > 0:
                    break

        sock.close()

        print_success(f"Received {len(response)} bytes")

        # Decode response
        headers, body = decode_chunked_http(response)

        # Check HTTP status
        response_text = response.decode('utf-8', errors='ignore')

        if 'HTTP/1.1 200' in response_text or 'HTTP/1.0 200' in response_text:
            print_success("HTTP 200 OK received")

            # Check for IPP errors in body
            if b'successful' in body.lower() or len(body) < 100:
                print_success("Print job submitted successfully!")
                return True
            else:
                # Parse IPP response for errors
                if b'Invalid' in body or b'error' in body.lower():
                    print_error("IPP error in response")
                    print_info("Response body preview:")
                    print(body[:200].decode('utf-8', errors='ignore'))
                    return False
                else:
                    print_success("Print job likely succeeded (no error detected)")
                    return True
        else:
            print_error("HTTP error response")
            print_info("First 500 bytes:")
            print(response_text[:500])
            return False

    except socket.timeout:
        print_error("Connection timeout!")
        return False
    except Exception as e:
        print_error(f"Error: {e}")
        import traceback
        traceback.print_exc()
        return False


def main():
    if len(sys.argv) < 2:
        print("Usage: python3 test_canon_printer.py <printer_ip> [port]")
        print("\nExample:")
        print("  python3 test_canon_printer.py 192.168.1.150")
        print("  python3 test_canon_printer.py 192.168.1.150 631")
        sys.exit(1)

    printer_ip = sys.argv[1]
    port = int(sys.argv[2]) if len(sys.argv) > 2 else 631

    print_header(f"Canon TS Printer Test Suite")
    print_info(f"Target: {printer_ip}:{port}")
    print()

    # Test 1: Get supported formats
    supported_formats = test_get_printer_attributes(printer_ip, port)

    if not supported_formats:
        print_error("Could not determine supported formats. Aborting tests.")
        return

    # Create test image
    test_image = create_test_image()
    print_info(f"\nCreated test image: {len(test_image)} bytes (1x1 red JPEG)")

    # Test 2: Try printing with each supported format
    print_header("Testing Print-Job with Each Format")

    results = {}

    for fmt in supported_formats:
        result = test_print_job(printer_ip, port, fmt, test_image)
        results[fmt] = result
        time.sleep(2)  # Wait between tests

    # Summary
    print_header("Test Results Summary")

    for fmt, success in results.items():
        if success:
            print_success(f"{fmt}: SUCCESS")
        else:
            print_error(f"{fmt}: FAILED")

    print()
    print_info("Recommended format for AI-Stick:")

    # Recommend best format
    if results.get('application/octet-stream'):
        print_success("  → application/octet-stream (auto-detect)")
    elif results.get('application/pdf'):
        print_success("  → application/pdf (universal)")
    elif results.get('image/jpeg'):
        print_success("  → image/jpeg (direct)")
    else:
        print_warning("  → No clearly working format found. May need format conversion.")


if __name__ == '__main__':
    main()

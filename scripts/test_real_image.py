#!/usr/bin/env python3
"""
Test printing a real image to Canon printer.
Tests with actual JPEG from AI-Stick to verify formats work with real data.
"""

import socket
import struct
import sys

def create_ipp_string(tag: int, name: str, value: str) -> bytes:
    """Create an IPP attribute (string type)"""
    data = bytearray()
    data.append(tag)
    name_bytes = name.encode('utf-8')
    data.extend(struct.pack('>H', len(name_bytes)))
    data.extend(name_bytes)
    value_bytes = value.encode('utf-8')
    data.extend(struct.pack('>H', len(value_bytes)))
    data.extend(value_bytes)
    return bytes(data)

def print_image(printer_ip: str, port: int, image_path: str, document_format: str):
    """Print an image file"""
    
    # Read image
    with open(image_path, 'rb') as f:
        image_data = f.read()
    
    print(f"\n{'='*60}")
    print(f"Printing Test")
    print(f"{'='*60}")
    print(f"Image: {image_path}")
    print(f"Size: {len(image_data)} bytes")
    print(f"Format: {document_format}")
    print()
    
    printer_uri = f"ipp://{printer_ip}:{port}/ipp/print"
    
    # Build IPP request
    ipp_request = bytearray()
    ipp_request.extend([0x01, 0x01])  # IPP 1.1
    ipp_request.extend([0x00, 0x02])  # Print-Job
    ipp_request.extend([0x00, 0x00, 0x00, 0x01])  # Request ID
    ipp_request.append(0x01)  # Operation attributes
    
    ipp_request.extend(create_ipp_string(0x47, "attributes-charset", "utf-8"))
    ipp_request.extend(create_ipp_string(0x48, "attributes-natural-language", "en"))
    ipp_request.extend(create_ipp_string(0x45, "printer-uri", printer_uri))
    ipp_request.extend(create_ipp_string(0x42, "requesting-user-name", "ai-stick-real-test"))
    ipp_request.extend(create_ipp_string(0x42, "job-name", "Real Image Test"))
    ipp_request.extend(create_ipp_string(0x49, "document-format", document_format))
    
    ipp_request.append(0x02)  # Job attributes
    ipp_request.append(0x03)  # End of attributes
    ipp_request.extend(image_data)
    
    # HTTP POST
    http_request = (
        f"POST /ipp/print HTTP/1.1\r\n"
        f"Host: {printer_ip}\r\n"
        f"Content-Type: application/ipp\r\n"
        f"Content-Length: {len(ipp_request)}\r\n"
        f"\r\n"
    ).encode('utf-8') + bytes(ipp_request)
    
    print(f"Sending {len(http_request)} bytes to printer...")
    
    # Connect and send
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(30)
    
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
            if b'0\r\n\r\n' in response or len(response) > 500:
                break
        
        sock.close()
        
        response_text = response.decode('utf-8', errors='ignore')
        
        print(f"\nReceived {len(response)} bytes")
        
        if 'HTTP/1.1 200' in response_text or 'HTTP/1.0 200' in response_text:
            if 'Invalid' in response_text or 'error' in response_text.lower():
                print("❌ FAILED - Printer rejected the job")
                print("\nError details:")
                print(response_text[:500])
                return False
            else:
                print("✅ SUCCESS - Print job accepted!")
                return True
        else:
            print("❌ FAILED - HTTP error")
            print(response_text[:500])
            return False
            
    except Exception as e:
        print(f"❌ ERROR: {e}")
        return False

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print("Usage: python3 test_real_image.py <printer_ip> <image_file>")
        print("\nExample:")
        print("  python3 test_real_image.py 192.168.1.150 test/extracted.jpg")
        sys.exit(1)
    
    printer_ip = sys.argv[1]
    image_file = sys.argv[2]
    
    # Test formats
    formats_to_test = [
        "image/jpeg",
        "application/octet-stream",
    ]
    
    print(f"\nTesting real image with Canon printer at {printer_ip}")
    print(f"Image: {image_file}")
    
    results = {}
    for fmt in formats_to_test:
        success = print_image(printer_ip, 631, image_file, fmt)
        results[fmt] = success
        if success:
            print(f"\n⭐ {fmt} works! Use this in Arduino.")
            break
        print("\nWaiting 3 seconds before next test...")
        import time
        time.sleep(3)
    
    print(f"\n{'='*60}")
    print("Summary:")
    print(f"{'='*60}")
    for fmt, success in results.items():
        status = "✅ WORKS" if success else "❌ FAILED"
        print(f"{status}: {fmt}")

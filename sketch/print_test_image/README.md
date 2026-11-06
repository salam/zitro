# Quick Print Test

Quick test sketch for debugging printer functionality without the full AI-Stick workflow.

**Purpose**: Print a test image with minimal interaction - no voice recording, no GPT, just discover → print.

### Setup

1. **Update WiFi credentials** in the sketch:
   ```cpp
   const char* WIFI_SSID = "YOUR_WIFI_SSID";
   const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
   ```

2. **Prepare test image**:
   - The test image is already in `data/extracted.jpg` (27KB JPEG)
   - Or replace with your own image (must be in `data/` folder)

3. **Upload SPIFFS data** (one time):
   - In Arduino IDE: Tools → ESP32 Sketch Data Upload
   - This uploads files from `data/` folder to the device's filesystem
   - Wait for "SPIFFS Image Uploaded" message

4. **Upload sketch**:
   - Select board: M5StickC Plus2
   - Upload normally

5. **Watch Serial Monitor**:
   - Baud rate: 115200
   - You'll see the entire process: WiFi → Discovery → Capabilities → Print

### What It Does

```
1. Load test image from SPIFFS (extracted.jpg)
2. Connect to WiFi
3. Discover first printer via mDNS
4. Query printer capabilities (with proper chunked decoding)
5. Print the image
6. Done!
```

### Expected Output

```
========================================
  Quick Printer Test - Print Image
========================================

✓ SPIFFS initialized

--- Loading Test Image ---
Path: /extracted.jpg
File size: 27254 bytes
Magic bytes: FF D8 FF E0
Format: JPEG
✓ Image loaded successfully

--- Connecting to WiFi ---
SSID: YourNetwork
..........
✓ WiFi connected
IP: 192.168.1.31

--- Discovering Printer ---
Querying _ipp._tcp... found 1

✓ Printer found!
  Name: 16B50F000000
  IP: 192.168.1.150
  Port: 631

--- Querying Printer Capabilities ---
Printer: 192.168.1.150:631
Request sent, waiting for response...
  Got 100 Continue, waiting for final response...
Response received: 378 bytes
  Skipping 100 Continue...
Headers end: 173, Chunked: YES
Decoding chunks...
  Chunk: 0xcc = 204 bytes
  Chunk: 0x0 = 0 bytes
IPP data size: 204 bytes
  ✓ image/jpeg
  ✓ application/octet-stream
  ✓ image/pwg-raster
  ✓ image/urf

--- Printing Image ---
Selected format: image/jpeg
Image size: 27254 bytes
Sending print job...
  Sent: 27254 / 27254 bytes (100%)
Waiting for response...
  Got 100 Continue...

Response received:
HTTP/1.1 200 OK
[IPP response...]

✓ Print job submitted successfully!

========================================
Test complete! Check your printer.
========================================
```

### Troubleshooting

**"SPIFFS initialization failed"**
- You forgot to upload SPIFFS data
- Do: Tools → ESP32 Sketch Data Upload

**"Could not open file"**
- File is not in `data/` folder
- Or file has wrong name (must be `extracted.jpg`)
- The sketch will list available files

**"No printer found"**
- Printer is off or not on same network
- Try running `printer_discovery_test.ino` first to debug discovery

**"Connection failed"**
- Printer IP/port incorrect
- Firewall blocking port 631
- Check printer is responding: `ping <printer_ip>`

**Print job rejected**
- Format not supported
- Check Serial Monitor for error details
- Try different image format

### Advantages Over Full Sketch

✅ **Much faster** - No voice recording, transcription, GPT, image generation
✅ **Reproducible** - Same image every time
✅ **Simpler debugging** - Fewer variables
✅ **Iterate quickly** - Upload and test in 30 seconds

### Testing Different Scenarios

**Test with different image formats:**
1. Put different images in `data/` folder: test.jpg, test.png, test.gif
2. Update `TEST_IMAGE_PATH` in sketch
3. Upload SPIFFS data again
4. Upload sketch and test

**Test format selection:**
- The sketch automatically selects best format from printer capabilities
- Prefers: JPEG > octet-stream > PDF
- Watch Serial Monitor to see which format is selected

**Force specific format:**
Edit the format selection code:
```cpp
String documentFormat = "image/jpeg";  // Force JPEG
// or
String documentFormat = "application/octet-stream";  // Force octet-stream
```

## printer_discovery_test.ino - Discovery Only

**Purpose**: Just discover and list printers (no printing).

Simpler test to verify mDNS discovery is working.

## Related Files

- `../test/extracted.jpg` - Original test image
- `../test/quick_extract2.jpg` - Larger test image
- `../../scripts/test_canon_printer.py` - Python test script (no Arduino needed)

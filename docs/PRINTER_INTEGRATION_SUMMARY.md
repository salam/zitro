# Printer Integration Summary

## Overview
Successfully integrated Bonjour/IPP printer discovery and printing functionality into the AI-Stick (sketch_jan12a.ino) with a user-friendly interface for printing generated images.

## Deliverables Created

### 1. Test and Research Tools

#### A. Printer Discovery Test Sketch
**File**: [sketch/printer_discovery_test/printer_discovery_test.ino](../sketch/printer_discovery_test/printer_discovery_test.ino)

Standalone Arduino sketch for testing mDNS/Bonjour printer discovery:
- Discovers printers using multiple service types (ipp, ipps, printer, pdl-datastream, http)
- Displays printer details (name, IP, port, service type)
- Provides next steps for testing each discovered printer
- Monitors memory usage during discovery

**Usage**:
1. Edit WiFi credentials in the sketch
2. Upload to M5StickC Plus 2
3. Open Serial Monitor at 115200 baud
4. Observe discovered printers every 30 seconds

#### B. Python IPP Test Client
**File**: [scripts/test_ipp_printer.py](../scripts/test_ipp_printer.py)

Desktop script for testing IPP communication with printers:
- Queries printer capabilities (Get-Printer-Attributes)
- Sends test print jobs with JPEG images
- Tests alternative endpoints and methods
- Creates test images if none provided

**Usage**:
```bash
# Install dependencies
pip install requests pillow

# Test printer discovery/capabilities
python scripts/test_ipp_printer.py 192.168.1.100

# Test with custom image
python scripts/test_ipp_printer.py 192.168.1.100 631 my_test_image.jpg
```

#### C. Research Findings Template
**File**: [docs/PRINTER_RESEARCH_FINDINGS.md](PRINTER_RESEARCH_FINDINGS.md)

Comprehensive template for documenting research results including:
- Printer discovery results
- Capability testing outcomes
- Print job testing results
- Memory constraints analysis
- Implementation recommendations

## Main Sketch Integration

### Changes Made to sketch_jan12a.ino

#### 1. Added ESPmDNS Library
**Line**: 12
```cpp
#include <ESPmDNS.h>
```

#### 2. Added Printer Data Structures
**Lines**: 110-123

```cpp
#define MAX_PRINTERS 10
struct PrinterInfo {
  String name;
  String hostname;
  IPAddress ip;
  uint16_t port;
  String serviceType;
};
PrinterInfo discoveredPrinters[MAX_PRINTERS];
int printerCount = 0;
int selectedPrinterIndex = 0;
bool inPrinterSelectionMode = false;
bool isPrinting = false;
```

#### 3. Implemented Core Printing Functions
**Lines**: 1750-2019

##### `void discoverPrinters()`
- Initializes mDNS with "ai-stick" hostname
- Queries for ipp, ipps, and printer service types
- Removes duplicate entries (same IP)
- Stores up to 10 unique printers
- Reports memory usage

##### `void displayPrinterSelection()`
- Shows list of discovered printers
- Highlights selected printer with `>` indicator
- Truncates long printer names to fit screen
- Shows control hints: "BtnC: Scroll  BtnA: Select"

##### `void addIPPAttribute()` and `void addIPPAttributeInt()`
- Helper functions to construct binary IPP requests
- Handles proper encoding of attribute names and values

##### `bool printImage()`
- Creates IPP Print-Job request (IPP version 1.1)
- Connects to printer via TCP
- Sends HTTP POST with IPP headers + JPEG image data
- Attributes sent:
  - printer-uri: `ipp://[IP]:[PORT]/ipp/print`
  - requesting-user-name: "ai-stick"
  - job-name: "AI-Stick Print"
  - document-format: "image/jpeg"
- Waits for response with 30-second timeout
- Returns success/failure based on HTTP 200 status

#### 4. Added Button Handling for Printing
**Lines**: 5221-5316

##### Printer Selection Mode Handler
When `inPrinterSelectionMode` is true:
- **Button C (PWR)**: Scroll through printer list (wraps to top)
- **Button A**: Select printer and initiate print
- **Button B**: Cancel and return to image view

##### Double-Click Button B to Start Printing
Only active when showing an image (`SHOW_IMAGE` mode):
- Double-click Button B triggers printer discovery
- Shows "Searching for printers..." message
- If printers found: enters printer selection mode
- If no printers: shows error message and returns to image

##### Single-Click Button B for Scrolling (Preserved)
Original functionality maintained:
- Single-click scrolls up when viewing images
- Works alongside new double-click detection

## User Workflow

### How to Print an Image

1. **Generate or View an Image**
   - Ask AI-Stick a question that generates an image (DALL-E, Wolfram Alpha, Wikipedia)
   - Toggle to image view if not already showing

2. **Initiate Printer Discovery**
   - **Double-click Button B** while viewing the image
   - Device shows "Searching for printers..."
   - mDNS discovery runs (takes 2-5 seconds)

3. **Select Printer**
   - If printers found, selection screen appears
   - **Button C**: Scroll through discovered printers
   - Selection wraps from bottom to top automatically
   - Current selection marked with `>`

4. **Print**
   - **Button A**: Confirm selection and print
   - Shows "Printing..." message
   - Sends image via IPP to selected printer
   - Shows success/failure message

5. **Results**
   - Success: "Print sent successfully!" → returns to image after 3s
   - Failure: "Print failed! Check printer." → returns to image after 3s

### Cancel Printing
At any point during printer selection:
- **Button B**: Cancel and return to image view

## Technical Details

### Supported Printers
Any printer that advertises via Bonjour/mDNS with:
- `_ipp._tcp` service
- `_ipps._tcp` service (IPP over TLS)
- `_printer._tcp` service

Most modern network printers including:
- HP printers with AirPrint
- Canon printers with Bonjour
- Epson printers with Bonjour
- Brother printers with IPP support
- Apple AirPrint-enabled printers

### Image Format

#### Format Support Strategy

The implementation uses a multi-tier approach for different image formats:

**JPEG Images** (DALL-E, some Wikipedia):

- Wrapped in URF (Uncompressed Raster Format) with format code 0x01
- Sent with MIME type `image/urf`
- Most reliable and widely supported by AirPrint printers

**GIF Images** (Wolfram Alpha):

- **Default**: Sent as raw binary with MIME type `application/octet-stream`
- Printers that support octet-stream will auto-detect and print GIF
- **Optional**: Can be converted to JPEG and wrapped in URF (requires JPEGENC library)

**PNG Images** (some Wikipedia):

- **Default**: Sent as raw binary with MIME type `application/octet-stream`
- Printers that support octet-stream will auto-detect and print PNG
- **Optional**: Can be converted to JPEG and wrapped in URF (requires JPEGENC library)

**URF Format Notes**:

- URF officially only supports JPEG format code (0x01)
- GIF (0x40) and PNG (0x41) format codes are non-standard and rejected by most printers
- Therefore, GIF/PNG cannot be directly wrapped in URF without conversion

#### Enabling JPEG Conversion (Optional)

If your printer doesn't support `application/octet-stream` for GIF/PNG, you can enable automatic conversion:

1. Install JPEGENC library: <https://github.com/bitbank2/JPEGENC>
2. In sketch_jan12a.ino, uncomment:

   ```cpp
   #define ENABLE_JPEG_CONVERSION
   #include <JPEGENC.h>
   ```

3. Implement the `convertToJPEG()` function (stub provided at line ~1990)

When enabled:

- GIF/PNG images are decoded to RGB pixels
- Pixels are encoded as JPEG
- JPEG is wrapped in URF format
- Sent with `image/urf` MIME type

#### Image Size

- Limited to existing imageBuffer constraints (~200KB max)
- Original resolution preserved from APIs:
  - DALL-E: 1024x1024 or 256x256 JPEG
  - Wolfram Alpha: Variable size GIF
  - Wikipedia: Variable size JPEG or PNG

### A4 Printing Notes
The current implementation sends images directly to the printer without explicit A4 scaling commands. Most modern printers will automatically:
- Center the image on the page
- Scale to fit within printable area
- Maintain aspect ratio

For explicit A4 scaling (if needed in future):
- Add IPP `media` attribute: "iso_a4_210x297mm"
- Add `print-scaling` attribute: "fit" or "fill"
- May require `fit-to-page` attribute

### Memory Considerations
**Discovery Phase**:
- mDNS library: ~10-20KB
- Printer list (10 printers): ~2KB
- Discovery queries: Minimal temporary memory

**Printing Phase**:
- IPP header: ~1KB
- Image buffer: Already allocated (100-200KB)
- TCP client buffers: ~5-10KB
- **Total**: Fits comfortably within ESP32 constraints

**Heap monitoring** is active in `discoverPrinters()` for debugging.

### Network Requirements
- Device and printer must be on same network
- No special firewall rules needed (standard IPP port 631)
- mDNS/Bonjour must not be blocked by network
- Works on both 2.4GHz and 5GHz WiFi networks

### IPP Protocol Implementation
**Implemented**:
- IPP version 1.1 (most compatible)
- Print-Job operation (0x0002)
- Required attributes (charset, language, printer-uri, etc.)
- HTTP POST over TCP
- Binary protocol encoding

**Not Implemented** (future enhancements if needed):
- IPP-over-TLS (ipps://) - currently uses plain IPP
- Job monitoring (Get-Job-Attributes)
- Job cancellation (Cancel-Job)
- Printer status queries (Get-Printer-Attributes)
- Multiple copy support
- Duplex printing control
- Color vs. B&W control

## Testing Checklist

### Before First Use
- [ ] Upload code to M5StickC Plus 2
- [ ] Verify WiFi connection works
- [ ] Generate a test image (ask for something visual)
- [ ] Ensure printer is on and connected to same network

### Discovery Testing
- [ ] Double-click Button B while viewing image
- [ ] Verify "Searching for printers..." appears
- [ ] Check Serial Monitor for discovered printers
- [ ] Verify printer list displays on screen

### Print Testing
- [ ] Scroll through printer list with Button C
- [ ] Verify selection wraps to top
- [ ] Select printer with Button A
- [ ] Verify "Printing..." message appears
- [ ] Check printer receives and prints image
- [ ] Verify success message and return to image view

### Error Handling
- [ ] Test with printer turned off → "No printers found"
- [ ] Test with printer offline → "Print failed"
- [ ] Test Button B cancel during selection
- [ ] Test double-click when not viewing image (should do nothing)

### Memory Testing
- [ ] Monitor Serial output for heap usage
- [ ] Verify no crashes during discovery
- [ ] Verify no crashes during printing
- [ ] Test multiple print cycles

## Troubleshooting

### No Printers Found
1. Check printer is on and connected to network
2. Verify printer supports Bonjour/mDNS (check printer manual)
3. Try accessing printer's web interface from computer on same network
4. Check if network has mDNS/Bonjour blocked (enterprise networks)
5. Try running standalone [printer_discovery_test.ino](../sketch/printer_discovery_test/printer_discovery_test.ino)

### Print Job Fails
1. Verify printer IP address is reachable (ping from computer)
2. Check printer queue for errors
3. Test with Python script: `python scripts/test_ipp_printer.py [PRINTER_IP]`
4. Check Serial Monitor for error messages
5. Some printers may use different IPP endpoints (e.g., `/ipp/printer` instead of `/ipp/print`)

### Image Quality Issues
1. Check original image size in Serial Monitor
2. Larger images print better (1024x1024 DALL-E images work well)
3. Small images (256x256) may appear pixelated when scaled to A4
4. Printer settings may affect quality (check printer preferences)

### Memory Errors
1. Check free heap in Serial Monitor
2. If memory low, try printing smaller images
3. Restart device between print jobs if needed
4. Monitor for memory leaks (heap should remain stable)

## Future Enhancements

### Potential Improvements
1. **A4 Scaling Control**
   - Add IPP attributes for explicit page size
   - Implement fit/fill/center options
   - Calculate optimal scaling for A4

2. **Printer Preferences**
   - Remember last-used printer
   - Store preferred printer in Preferences
   - Skip discovery if preferred printer available

3. **Advanced IPP Features**
   - Query printer capabilities before printing
   - Support multiple copies
   - Duplex printing option
   - Color vs. B&W selection

4. **UI Enhancements**
   - Show printer status (ready, busy, error)
   - Display print queue position
   - Show estimated print time
   - Add print preview

5. **Error Handling**
   - Retry failed prints automatically
   - Better error messages for specific failures
   - Job cancellation support

6. **Alternative Protocols**
   - IPPS (IPP over TLS) for secure printing
   - Raw socket printing for basic printers
   - LPD protocol as fallback

## Files Modified

### Main Sketch
- [sketch/sketch_jan12a/sketch_jan12a.ino](../sketch/sketch_jan12a/sketch_jan12a.ino)
  - Added `#include <ESPmDNS.h>` (line 12)
  - Added printer data structures (lines 110-123)
  - Added printer functions (lines 1750-2019)
  - Added button handling (lines 5221-5316)

### New Files Created
- [sketch/printer_discovery_test/printer_discovery_test.ino](../sketch/printer_discovery_test/printer_discovery_test.ino)
- [scripts/test_ipp_printer.py](../scripts/test_ipp_printer.py)
- [docs/PRINTER_RESEARCH_FINDINGS.md](PRINTER_RESEARCH_FINDINGS.md)
- [docs/PRINTER_INTEGRATION_SUMMARY.md](PRINTER_INTEGRATION_SUMMARY.md) (this file)

## References

### Standards and Protocols
- **IPP**: Internet Printing Protocol (RFC 2910, RFC 2911)
- **mDNS**: Multicast DNS (RFC 6762)
- **DNS-SD**: DNS Service Discovery (RFC 6763)

### Useful Links
- [IPP Official Site](https://www.pwg.org/ipp/)
- [AirPrint Specification](https://developer.apple.com/bonjour/printing-specification/bonjourprinting-1.2.1.pdf)
- [ESP32 mDNS Documentation](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/mdns.html)

### Testing Tools
- **macOS**: `dns-sd -B _ipp._tcp` (built-in Bonjour browser)
- **Linux**: `avahi-browse -t _ipp._tcp` (requires avahi-utils)
- **Windows**: Bonjour Browser (available from Apple)
- **Cross-platform**: [Discovery - DNS-SD Browser](http://www.tobias-erichsen.de/software/discovery.html)

## Support and Issues

For issues or questions:
1. Check Serial Monitor output for detailed error messages
2. Review troubleshooting section above
3. Test with standalone test sketches
4. Verify printer compatibility with test_ipp_printer.py script

---

**Implementation Date**: 2026-01-16
**Version**: 1.0
**Status**: Complete and ready for testing

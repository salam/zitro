# Canon Printer Testing Tool

Quick Python script to test IPP communication with your Canon TS printer directly from your Mac, without needing to upload code to the Arduino.

## Quick Start

```bash
# Make sure your Canon printer is on and connected to the network
python3 scripts/test_canon_printer.py 192.168.1.150
```

## What It Does

The script performs two tests:

### Test 1: Get-Printer-Attributes
- Queries the printer for supported document formats
- Decodes chunked HTTP responses properly
- Parses IPP binary payload
- Lists all MIME types the printer accepts

### Test 2: Print-Job Test
- Creates a minimal 1x1 pixel red JPEG test image
- Tries printing with each supported format
- Reports which formats succeed vs fail
- Recommends the best format for your AI-Stick

## Output Example

When successful, you'll see:

```
============================================================
                Canon TS Printer Test Suite
============================================================

ℹ Target: 192.168.1.150:631

============================================================
               TEST 1: Get-Printer-Attributes
============================================================

✓ Found 5 supported formats:
  • application/octet-stream
  • application/pdf
  • image/pwg-raster
  • image/jpeg
  • application/postscript

============================================================
              TEST 2: Print-Job with application/octet-stream
============================================================

✓ HTTP 200 OK received
✓ Print job submitted successfully!

============================================================
                  Test Results Summary
============================================================

✓ application/octet-stream: SUCCESS
✓ application/pdf: SUCCESS
✗ image/pwg-raster: FAILED
✓ image/jpeg: SUCCESS
✓ application/postscript: SUCCESS

ℹ Recommended format for AI-Stick:
  → application/octet-stream (auto-detect)
```

## Troubleshooting

### Connection Timeout
```
✗ Connection timeout!
```

**Solutions:**
- Make sure printer is powered on
- Check printer is on the same network as your Mac
- Verify IP address is correct: `ping 192.168.1.150`
- Try accessing printer web interface: `http://192.168.1.150/`

### No Formats Found
```
⚠ No recognized formats found in response
```

**Solutions:**
- Check the raw response in debug output
- Printer might use non-standard IPP endpoint
- Try different URI path (e.g., `/ipp/printer` instead of `/ipp/print`)

### Print Job Fails
```
✗ application/octet-stream: FAILED
```

**Solutions:**
- Check the error message in the response
- Printer might reject test image size
- Try with different test image
- Some formats may require specific IPP attributes

## Advanced Usage

### Custom Port
```bash
python3 scripts/test_canon_printer.py 192.168.1.150 8631
```

### Debug Mode
Edit the script and uncomment the debug print statements to see raw IPP data:

```python
# Around line 250, uncomment:
print("Raw IPP body:", body[:500])
```

### Test with Real Image

Replace the `create_test_image()` function with your own image:

```python
def create_test_image() -> bytes:
    with open('/path/to/test.jpg', 'rb') as f:
        return f.read()
```

## Next Steps

Once you identify the working format:

1. Update Arduino code to use that format
2. Remove URF wrapping if not needed
3. Test with actual images from AI-Stick

## Script Features

- ✅ Chunked transfer decoding
- ✅ IPP binary parsing
- ✅ Multiple format testing
- ✅ Color-coded output
- ✅ HTTP 100 Continue handling
- ✅ Detailed error messages
- ✅ Format recommendation

## Common Canon TS Printer Formats

Based on testing with various Canon models:

| Format | Description | Usually Works? |
|--------|-------------|----------------|
| `application/octet-stream` | Raw binary, auto-detect | ✅ Usually Yes |
| `application/pdf` | PDF document | ✅ Usually Yes |
| `image/jpeg` | Direct JPEG | ⚠️ Sometimes |
| `image/pwg-raster` | PWG raster format | ❌ Often No |
| `image/urf` | Apple URF format | ❌ Often No |
| `application/postscript` | PostScript | ⚠️ Sometimes |

## Related Files

- `test_ipp_printer.py` - Older test script with different approach
- `extract_urf.py` - URF file extraction tool
- `sketch_jan12a.ino` - Main Arduino code

## Help

If the script doesn't work:

1. Check printer is responding: `ping 192.168.1.150`
2. Check printer web UI: `http://192.168.1.150/`
3. Try the printer from your Mac normally (to verify it works)
4. Check firewall isn't blocking port 631
5. Look at the raw output for clues

## References

- IPP RFC: https://datatracker.ietf.org/doc/html/rfc2910
- IPP Attributes: https://www.pwg.org/ipp/ipp-registrations.xml
- Canon IPP Implementation: Varies by model

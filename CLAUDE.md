Check ./REQUIREMENTS.md.

Keep ./USERGUIDE.md up to date.

If a new prompt breaks with the currently outlined behaviour in ./REQUIREMENTS.md or ./USERGUIDE.md, tell the user first and ask for confirmation.

Highlight contradictions and inconsistencies implied by the user's requests.

Always compile the code with verify.sh to check if the code is valid.

Always keep an updated ./RELEASE_NOTES.md file that contains a bullet point list with brief descriptions of updates, targeted to end-user, simple, activating language. Group bullet points by the date you have implemented it, within 4 hours in the same subtitle section.

## Arduino Sketch Verification

When modifying Arduino sketch files (*.ino), always run the verification script before completing the response:

```bash
./verify.sh
```

This ensures the code compiles successfully before presenting changes to the user.

## Website Cache Busting

When modifying JavaScript files in the webpage/ directory (script.js, shop.js), ALWAYS update the version parameter in the corresponding HTML files to force browser cache refresh:

1. **Increment the version number** in the script tag (e.g., `?v=1.0.0` to `?v=1.0.1`)
2. **Update both files**:
   - webpage/index.html: `<script src="script.js?v=X.X.X"></script>`
   - webpage/shop.html: `<script src="shop.js?v=X.X.X"></script>`

**Version numbering:**
- Patch version (X.X.1): Bug fixes, minor changes
- Minor version (X.1.0): New features, significant updates
- Major version (1.0.0): Breaking changes, major rewrites

This prevents users from seeing cached outdated JavaScript after updates.

## Printer Integration

Before attempting printer fixes, **always check ./docs/PRINTER_FINDINGS.md** for documented test results and known issues.

**Critical findings for Canon TS5151:**
- JPEGENC library with 4:2:0 subsampling creates JPEGs that Canon cannot render (blank pages)
- JPEGENC library does NOT support 4:4:4 subsampling with RGB888 (encoding error)
- application/octet-stream with PNG has been tested and does NOT work
- Native properly-encoded JPEG files DO work (see test_image_data.h)

**Potential solutions to explore:**
- PWG-raster format (Canon supports it, uncompressed, no subsampling issues)
- Alternative JPEG encoder libraries
- External conversion server
- Analyze working JPEG parameters from test_image_data.h
# Font Generation Quick Start

Since I cannot access web-based tools directly, here's exactly what you need to do:

## Quick Steps (5 minutes)

### Step 1: Download Roboto Font
1. Open browser: https://fonts.google.com/specimen/Roboto
2. Click "Download family" button
3. Extract `Roboto-Regular.ttf` from the ZIP file

### Step 2: Generate PrimaryFont12pt.h
1. Open browser: https://rop.nl/truetype2gfx/
2. Click "Select font file" → Choose `Roboto-Regular.ttf`
3. **Font size**: Type `12`
4. **Font name**: Type `PrimaryFont12pt`
5. **Character ranges** - Enter these lines (copy/paste):
   ```
   0x20-0x7F
   0x80-0xFF
   0x100-0x17F
   0x370-0x3FF
   0x400-0x4FF
   0x590-0x5FF
   0x2013-0x2026
   ```
6. Click **"Create"** button
7. Download the generated file
8. Save it as: `sketch/sketch_jan12a/fonts/PrimaryFont12pt.h`

### Step 3: Generate PrimaryFont16pt.h
1. Stay on https://rop.nl/truetype2gfx/
2. Click "Select font file" → Choose `Roboto-Regular.ttf` again
3. **Font size**: Type `16`
4. **Font name**: Type `PrimaryFont16pt`
5. **Character ranges** - Same as above:
   ```
   0x20-0x7F
   0x80-0xFF
   0x100-0x17F
   0x370-0x3FF
   0x400-0x4FF
   0x590-0x5FF
   0x2013-0x2026
   ```
6. Click **"Create"** button
7. Download the generated file
8. Save it as: `sketch/sketch_jan12a/fonts/PrimaryFont16pt.h`

### Step 4: Verify and Compile
```bash
cd /Users/matthias/Development/ai-stick
./verify.sh
```

Look for this message:
```
FontManager: Using custom primary fonts
```

### Step 5: Upload
Upload the sketch to your M5StickC Plus 2 and enjoy native umlauts!

---

## Expected File Sizes

- `PrimaryFont12pt.h`: ~20-25 KB
- `PrimaryFont16pt.h`: ~30-35 KB

## Troubleshooting

**"Font name is required"**
→ Make sure you typed the font name exactly: `PrimaryFont12pt` or `PrimaryFont16pt`

**"No character ranges specified"**
→ Make sure you entered all 7 hex ranges, one per line

**Web tool times out**
→ These are small character sets, shouldn't timeout. Try refreshing the page.

**Files not found during compilation**
→ Check files are in exact location: `sketch/sketch_jan12a/fonts/`
→ File names must be exact: `PrimaryFont12pt.h` and `PrimaryFont16pt.h`

---

## Alternative: Use Helper Script

I've created a guided script:
```bash
./generate_fonts.sh
```

It will walk you through the process step-by-step!

#!/bin/bash
#
# AI Stick - Arduino Deployment Script
# Build and Upload Script
#
# This script automates the compilation and upload process
# for the AI Stick firmware.
#
# Usage:
#   ./deploy.sh           # Build, upload, and monitor
#   ./deploy.sh --clean   # Clean build first
#   ./deploy.sh --build   # Build only (no upload)
#   ./deploy.sh --monitor # Monitor only (no build/upload)
#

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
SKETCH_PATH="sketch/sketch_jan12a/sketch_jan12a.ino"
FQBN="esp32:esp32:m5stack_stickc_plus2"
BUILD_CACHE_DIR=".arduino-build-cache"

# Script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_DIR="$SCRIPT_DIR"

# Change to project directory
cd "$PROJECT_DIR"

# Print colored message
print_header() {
    echo -e "${BLUE}================================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}================================================${NC}"
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}"
}

print_info() {
    echo -e "${YELLOW}→ $1${NC}"
}

# Check if arduino-cli is installed
if ! command -v arduino-cli &> /dev/null; then
    print_error "arduino-cli not found!"
    echo ""
    echo "Install with:"
    echo "  brew install arduino-cli"
    echo ""
    echo "Or download from: https://arduino.github.io/arduino-cli/"
    exit 1
fi

# Parse arguments
CLEAN_BUILD=false
BUILD_ONLY=false
MONITOR_ONLY=false
AUTO_YES=false
PORT=""

for arg in "$@"; do
    case $arg in
        --clean)
            CLEAN_BUILD=true
            shift
            ;;
        --build)
            BUILD_ONLY=true
            shift
            ;;
        --monitor|-m)
            MONITOR_ONLY=true
            shift
            ;;
        -y|--yes)
            AUTO_YES=true
            shift
            ;;
        --port=*)
            PORT="${arg#*=}"
            shift
            ;;
        --help|-h)
            echo "AI Stick - Build & Upload Script"
            echo ""
            echo "Usage:"
            echo "  ./deploy.sh                  Build, upload, and monitor"
            echo "  ./deploy.sh --clean          Clean build first"
            echo "  ./deploy.sh --build          Build only (no upload)"
            echo "  ./deploy.sh --monitor        Monitor only (no build/upload)"
            echo "  ./deploy.sh -m               Same as --monitor"
            echo "  ./deploy.sh -y               Automatically say yes to prompts"
            echo "  ./deploy.sh --yes            Same as -y"
            echo "  ./deploy.sh --port=/dev/tty* Specify serial port"
            echo "  ./deploy.sh --help           Show this help"
            exit 0
            ;;
        *)
            print_error "Unknown argument: $arg"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Detect serial port if not specified
detect_port() {
    if [ -n "$PORT" ]; then
        return
    fi

    # Try to auto-detect M5Stack device
    if [ -f "/dev/cu.usbserial-*" ]; then
        PORT=$(ls /dev/cu.usbserial-* 2>/dev/null | head -n 1)
    elif [ -f "/dev/ttyUSB*" ]; then
        PORT=$(ls /dev/ttyUSB* 2>/dev/null | head -n 1)
    fi

    if [ -z "$PORT" ]; then
        print_error "No serial port detected!"
        echo ""
        echo "Available ports:"
        arduino-cli board list
        echo ""
        echo "Specify port manually with: ./deploy.sh --port=/dev/YOUR_PORT"
        exit 1
    fi
}

# Main build process
print_header "AI Stick Deployment Tool"
echo ""

# If monitor-only mode, skip build and upload
if [ "$MONITOR_ONLY" = true ]; then
    detect_port
    print_info "Monitor-only mode: Opening serial monitor on $PORT (Ctrl+C to exit)..."
    echo ""
    arduino-cli monitor -p "$PORT" -c baudrate=115200
    exit 0
fi

# Check if ESP32 core is installed
if ! arduino-cli core list | grep -q "esp32:esp32"; then
    print_info "ESP32 core not installed, installing..."
    arduino-cli core update-index
    arduino-cli core install esp32:esp32
    print_success "ESP32 core installed"
    echo ""
fi

# Clean if requested
if [ "$CLEAN_BUILD" = true ]; then
    print_info "Cleaning previous build..."
    rm -rf "$BUILD_CACHE_DIR"
    # Also clean arduino-cli's default cache
    arduino-cli cache clean 2>/dev/null || true
    print_success "Clean complete"
    echo ""
fi

# Build firmware
print_info "Building firmware..."
print_info "Board: $FQBN"
print_info "Sketch: $SKETCH_PATH"
echo ""

# Compile the sketch (arduino-cli manages build cache automatically)
if arduino-cli compile --fqbn "$FQBN" --build-property "compiler.c.elf.extra_flags=-Wl,--allow-multiple-definition" "$SKETCH_PATH"; then
    echo ""
    print_success "Build successful!"
else
    echo ""
    print_error "Build failed!"
    exit 1
fi

echo ""

# Exit if build-only mode
if [ "$BUILD_ONLY" = true ]; then
    print_info "Build-only mode: Skipping upload"
    exit 0
fi

echo ""

# Detect port for upload
detect_port

# Prompt for upload
if [ "$AUTO_YES" = true ]; then
    REPLY="Y"
    print_info "Auto-confirming upload to $PORT..."
else
    read -p "$(echo -e ${YELLOW}Upload to device on $PORT? [Y/n]:${NC} )" -n 1 -r
    echo ""
fi

if [[ $REPLY =~ ^[Yy]$ ]] || [[ -z $REPLY ]]; then
    print_info "Uploading firmware to $PORT..."
    echo ""

    # Upload directly - arduino-cli uses the compiled binaries from its cache
    if arduino-cli upload -p "$PORT" --fqbn "$FQBN" "$SKETCH_PATH"; then
        echo ""
        print_success "Upload complete!"
    else
        echo ""
        print_error "Upload failed!"
        echo ""
        print_info "Make sure the AI Stick is connected via USB"
        print_info "Available ports:"
        arduino-cli board list
        exit 1
    fi

    echo ""

    # Prompt for serial monitor
    if [ "$AUTO_YES" = true ]; then
        REPLY="Y"
        print_info "Auto-confirming serial monitor..."
    else
        read -p "$(echo -e ${YELLOW}Open serial monitor? [Y/n]:${NC} )" -n 1 -r
        echo ""
    fi

    if [[ $REPLY =~ ^[Yy]$ ]] || [[ -z $REPLY ]]; then
        print_info "Opening serial monitor on $PORT (Ctrl+C to exit)..."
        echo ""
        # Wait a moment for the device to reset
        sleep 2
        arduino-cli monitor -p "$PORT" -c baudrate=115200
    fi
else
    print_info "Upload cancelled"
fi

echo ""
print_success "Done!"

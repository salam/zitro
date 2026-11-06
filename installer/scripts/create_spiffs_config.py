#!/usr/bin/env python3
"""
Create a minimal SPIFFS filesystem image with config.json
This uses spiffsgen from esp-idf tools
"""

import sys
import json
import os
import tempfile
import shutil

def create_config_spiffs(openai_key, wifi_ssid, wifi_password, output_file, size=0x180000):
    """
    Create a SPIFFS image with config.json

    Args:
        openai_key: OpenAI API key
        wifi_ssid: WiFi network name
        wifi_password: WiFi password
        output_file: Path to output SPIFFS image
        size: Size of SPIFFS partition (default 1.5MB = 0x180000)
    """

    # Create config dictionary
    config = {}

    if openai_key:
        config['openai_api_key'] = openai_key

    if wifi_ssid:
        config['wifi_ssid'] = wifi_ssid
        config['wifi_password'] = wifi_password or ""

    # Create temporary directory for SPIFFS contents
    temp_dir = tempfile.mkdtemp()

    try:
        # Write config.json
        config_path = os.path.join(temp_dir, 'config.json')
        with open(config_path, 'w') as f:
            json.dump(config, f, indent=2)

        print(f"Created config.json with {len(config)} settings")

        # Try to use spiffsgen.py from esp-idf
        try:
            # Look for spiffsgen in common locations
            spiffsgen_locations = [
                # Arduino ESP32 tools
                os.path.expanduser('~/Library/Arduino15/packages/esp32/tools/*/*/spiffsgen.py'),
                # System esp-idf
                '/usr/local/bin/spiffsgen.py',
                # Homebrew esp-idf
                '/opt/homebrew/bin/spiffsgen.py',
            ]

            import subprocess
            import glob

            spiffsgen = None
            for pattern in spiffsgen_locations:
                matches = glob.glob(pattern)
                if matches:
                    spiffsgen = matches[0]
                    break

            if not spiffsgen:
                # Try to find it via arduino-cli
                result = subprocess.run(
                    ['arduino-cli', 'config', 'dump', '--format', 'json'],
                    capture_output=True,
                    text=True
                )
                if result.returncode == 0:
                    import json as json_module
                    config_data = json_module.loads(result.stdout)
                    data_dir = config_data.get('directories', {}).get('data', '')
                    if data_dir:
                        pattern = os.path.join(data_dir, 'packages/esp32/tools/*/*/spiffsgen.py')
                        matches = glob.glob(pattern)
                        if matches:
                            spiffsgen = matches[0]

            if spiffsgen:
                print(f"Using spiffsgen: {spiffsgen}")
                result = subprocess.run([
                    'python3', spiffsgen,
                    str(size),  # SPIFFS size
                    temp_dir,   # Source directory
                    output_file # Output file
                ], capture_output=True, text=True)

                if result.returncode == 0:
                    print(f"SPIFFS image created: {output_file}")
                    return True
                else:
                    print(f"Error creating SPIFFS: {result.stderr}")
                    return False
            else:
                print("Warning: spiffsgen.py not found")
                print("Creating empty SPIFFS image as fallback")

                # Create a minimal empty SPIFFS image
                # Just write the config as a simple binary blob at the start
                with open(output_file, 'wb') as f:
                    # Write config JSON at offset 0
                    config_json = json.dumps(config).encode('utf-8')
                    f.write(config_json)
                    # Pad with zeros to fill partition
                    f.write(b'\xFF' * (size - len(config_json)))

                print(f"Fallback config image created: {output_file}")
                return True

        except Exception as e:
            print(f"Error: {e}")
            return False

    finally:
        # Clean up temp directory
        shutil.rmtree(temp_dir, ignore_errors=True)

if __name__ == '__main__':
    if len(sys.argv) < 4:
        print("Usage: create_spiffs_config.py <openai_key> <wifi_ssid> <wifi_password> <output_file>")
        print("  Use empty string \"\" for optional parameters")
        sys.exit(1)

    openai_key = sys.argv[1] if sys.argv[1] != "" else None
    wifi_ssid = sys.argv[2] if sys.argv[2] != "" else None
    wifi_password = sys.argv[3] if sys.argv[3] != "" else None
    output_file = sys.argv[4]

    success = create_config_spiffs(openai_key, wifi_ssid, wifi_password, output_file)
    sys.exit(0 if success else 1)

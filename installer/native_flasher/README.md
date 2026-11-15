# esp32flash

The command line flasher inside the Zitro installer. Swift, no dependencies, universal binary.

```bash
./build.sh
bin/esp32flash --detect
bin/esp32flash --port /dev/cu.usbserial-XXXX --info                 # Identifies the board, changes nothing
bin/esp32flash --port /dev/cu.usbserial-XXXX --install ../firmware  # Keeps stored settings
echo '{"ssid":"Home","password":"secret123"}' | bin/esp32flash --port /dev/cu.usbserial-XXXX --provision
```

It only writes what a verified firmware package lists, and only to an ESP32 with at least 8 MB of flash. There's no option to erase the chip or write an arbitrary file.

See [../INSTALLER_GUIDE.md](../INSTALLER_GUIDE.md).

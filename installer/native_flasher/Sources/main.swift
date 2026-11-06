import Foundation

// MARK: - CLI

func printUsage() {
    print("""
    ESP32 Flash - Native ESP32 Flasher for macOS

    USAGE:
        esp32flash [OPTIONS]

    OPTIONS:
        --port <device>      Serial port (e.g., /dev/cu.usbserial-XXXX)
        --erase              Erase flash memory
        --write <file>       Write single firmware file to flash
        --write-parts <dir>  Write firmware parts from directory
        --offset <addr>      Flash offset address (default: 0x0)
        --baud <rate>        Baud rate (default: 115200)
        --detect             Detect ESP32 devices
        --help               Show this help message

    EXAMPLES:
        # Detect devices
        esp32flash --detect

        # Erase flash
        esp32flash --port /dev/cu.usbserial-XXXX --erase

        # Write firmware parts (bootloader + partitions + app)
        esp32flash --port /dev/cu.usbserial-XXXX --erase --write-parts firmware/

        # Write single file to specific offset
        esp32flash --port /dev/cu.usbserial-XXXX --write firmware.bin --offset 0x10000

        # Full flash process (erase + write)
        esp32flash --port /dev/cu.usbserial-XXXX --erase --write firmware.bin
    """)
}

func parseArguments() -> [String: String] {
    var args: [String: String] = [:]
    var i = 1

    while i < CommandLine.arguments.count {
        let arg = CommandLine.arguments[i]

        if arg.hasPrefix("--") {
            let key = String(arg.dropFirst(2))

            // Check for flags without values
            if key == "erase" || key == "detect" || key == "help" {
                args[key] = "true"
                i += 1
            } else if i + 1 < CommandLine.arguments.count {
                // Arguments with values
                let value = CommandLine.arguments[i + 1]
                args[key] = value
                i += 2
            } else {
                print("Error: --\(key) requires a value")
                exit(1)
            }
        } else {
            i += 1
        }
    }

    return args
}

func main() {
    let args = parseArguments()

    // Show help
    if args["help"] != nil || CommandLine.arguments.count == 1 {
        printUsage()
        exit(0)
    }

    // Detect devices
    if args["detect"] != nil {
        print("Scanning for ESP32 devices...")
        let devices = findESP32Devices()

        if devices.isEmpty {
            print("No ESP32 devices found.")
            print("\nMake sure:")
            print("  1. ESP32 is connected via USB")
            print("  2. USB drivers are installed")
            print("  3. Device appears as /dev/cu.usbserial-*")
        } else {
            print("Found \(devices.count) device(s):")
            for device in devices {
                print("  • \(device)")
            }
        }
        exit(0)
    }

    // Get port
    guard let port = args["port"] else {
        print("Error: --port is required")
        print("\nUse --detect to find available devices")
        print("Use --help for usage information")
        exit(1)
    }

    // Get baud rate
    let baudRate = Int(args["baud"] ?? "115200") ?? 115200

    // Validate operations
    let shouldErase = args["erase"] != nil
    let shouldWrite = args["write"] != nil
    let shouldWriteParts = args["write-parts"] != nil

    if !shouldErase && !shouldWrite && !shouldWriteParts {
        print("Error: Specify at least one operation (--erase, --write, or --write-parts)")
        printUsage()
        exit(1)
    }

    // Parse offset
    let offsetStr = args["offset"] ?? "0x0"
    var offset: UInt32 = 0
    if offsetStr.hasPrefix("0x") {
        offset = UInt32(offsetStr.dropFirst(2), radix: 16) ?? 0
    } else {
        offset = UInt32(offsetStr) ?? 0
    }

    // Banner
    print("================================")
    print("  ESP32 Flash - Native Flasher")
    print("================================")
    print()
    print("Port: \(port)")
    print("Baud Rate: \(baudRate)")
    print()

    do {
        // Connect to ESP32
        print("Connecting to ESP32...")
        let flasher = try Flasher(portPath: port, baudRate: baudRate)

        try flasher.connect()
        print("✓ Connected to ESP32")

        // Erase if requested
        if shouldErase {
            try flasher.eraseFlash()
        }

        // Write firmware parts if requested
        if shouldWriteParts, let partsDir = args["write-parts"] {
            try flasher.writeFlashParts(firmwareDir: partsDir)
        }

        // Write single file if requested
        if shouldWrite, let firmwarePath = args["write"] {
            try flasher.writeFlash(firmwarePath: firmwarePath, offset: offset)
        }

        flasher.disconnect()

        print()
        print("================================")
        print("  ✓ SUCCESS")
        print("================================")
        print()
        print("You can now:")
        print("  1. Disconnect the ESP32")
        print("  2. Press the reset button")
        print()

    } catch let error as ESP32Error {
        print()
        print("================================")
        print("  ✗ ESP32 ERROR")
        print("================================")
        print()
        print("Error: \(error)")
        print()
        exit(1)
    } catch let error as SerialPortError {
        print()
        print("================================")
        print("  ✗ SERIAL PORT ERROR")
        print("================================")
        print()
        print("Error: \(error)")
        print()
        print("Make sure:")
        print("  1. ESP32 is connected to \(port)")
        print("  2. No other program is using the port")
        print("  3. You have permission to access the port")
        print()
        exit(1)
    } catch {
        print()
        print("================================")
        print("  ✗ UNEXPECTED ERROR")
        print("================================")
        print()
        print("Error: \(error)")
        print()
        exit(1)
    }
}

// Run main
main()

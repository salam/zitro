import Foundation

// Command line flasher behind the Zitro installer app.
//
// Lines on stdout starting with @@ are for the app:
//   @@PROGRESS <0-100> <text>
//   @@ERROR <code> <message>
// Settings arrive on stdin only (--provision), never as arguments: arguments are visible in `ps`.

let usage = """
esp32flash - installs Zitro firmware on an M5StickC Plus2

  esp32flash --detect
  esp32flash --port <device> --info
  esp32flash --port <device> --install <firmware dir> [--provision] [--baud <rate>]
  esp32flash --port <device> --provision

  --detect     List serial ports
  --info       Identify the board. Only reads; changes nothing
  --install    Write the firmware package. Stored settings stay on the stick
  --provision  Send settings (a one-line JSON object on stdin) to the running firmware
  --baud       Write speed (default 460800, falls back to 115200)
"""

func fail(_ failure: FlashFailure) -> Never {
    print("@@ERROR \(failure.code) \(failure.message)")
    FileHandle.standardError.write(Data("\(failure.message)\n".utf8))
    exit(failure.code.hasPrefix("settings-") ? 3 : 1)
}

/// Turns any error into something the user can act on.
func describe(_ error: Error, port: String) -> FlashFailure {
    switch error {
    case let failure as FlashFailure:
        return failure
    case SerialPortError.busy:
        return FlashFailure(code: "port-busy", message: "Another program is using the stick's USB port. Close it (Arduino, serial monitors) and try again.")
    case SerialPortError.openFailed(let detail):
        return FlashFailure(code: "port-open", message: "Could not open the USB port. \(detail)")
    case SerialPortError.readFailed, SerialPortError.writeFailed:
        return FlashFailure(code: "unplugged", message: "The connection to the stick was lost. Check the USB cable and try again.")
    case ESP32Error.wrongChip:
        return FlashFailure(code: "wrong-board", message: "This device is not an M5StickC Plus2 (wrong chip). Nothing was changed on it.")
    case ESP32Error.flashTooSmall(let size):
        return FlashFailure(code: "wrong-board", message: "This device has \(size / 1_048_576) MB of flash and Zitro needs 8 MB, so it isn't an M5StickC Plus2. Nothing was changed on it.")
    case ESP32Error.flashUnreadable:
        return FlashFailure(code: "wrong-board", message: "Could not identify this device's memory. Nothing was changed on it.")
    case ESP32Error.syncFailed:
        return FlashFailure(code: "no-answer", message: "The stick did not answer. Unplug it, plug it back in with a data USB cable, and try again.")
    case ESP32Error.verifyFailed(let part):
        return FlashFailure(code: "verify-failed", message: "The stick's memory did not match after writing (\(part)). Try again with another USB cable.")
    default:
        return FlashFailure(code: "transfer-failed", message: "The transfer to the stick was interrupted (\(error)). Try again; the stick can always be re-installed.")
    }
}

func run() {
    setlinebuf(stdout)

    var options = Set<String>()
    var values: [String: String] = [:]
    var index = 1
    let arguments = CommandLine.arguments
    while index < arguments.count {
        let argument = arguments[index]
        switch argument {
        case "--detect", "--info", "--provision", "--help":
            options.insert(argument)
        case "--port", "--install", "--baud":
            index += 1
            guard index < arguments.count else {
                print("\(argument) needs a value\n\n\(usage)")
                exit(2)
            }
            values[argument] = arguments[index]
        default:
            print("Unknown option \(argument)\n\n\(usage)")
            exit(2)
        }
        index += 1
    }

    if options.contains("--help") || arguments.count == 1 {
        print(usage)
        exit(0)
    }
    if options.contains("--detect") {
        findSerialPorts().forEach { print($0) }
        exit(0)
    }
    guard let port = values["--port"] else {
        print("--port is required\n\n\(usage)")
        exit(2)
    }
    let installDir = values["--install"]
    guard installDir != nil || options.contains("--info") || options.contains("--provision") else {
        print("Nothing to do\n\n\(usage)")
        exit(2)
    }

    // Everything that can fail without the stick fails first
    var settings: Data?
    if options.contains("--provision") {
        let input = FileHandle.standardInput.readDataToEndOfFile()
        guard let object = try? JSONSerialization.jsonObject(with: input), object is [String: Any],
              let compact = try? JSONSerialization.data(withJSONObject: object) else {
            fail(FlashFailure(code: "settings-failed", message: "The settings could not be prepared."))
        }
        settings = compact
    }

    var package: (manifest: FirmwareManifest, images: [FlashImage])?
    if let installDir = installDir {
        do {
            package = try loadFirmwarePackage(directory: installDir)
            print("Firmware \(package!.manifest.firmware): all files verified")
        } catch {
            fail(describe(error, port: port))
        }
    }

    var preferredBaud = 460800  // 921600 loses data on the M5StickC Plus2
    if let requested = values["--baud"] {
        guard let baud = Int(requested), baud >= 9600 else {
            print("Bad --baud value\n\n\(usage)")
            exit(2)
        }
        preferredBaud = baud
    }

    let flasher: Flasher
    do {
        flasher = try Flasher(portPath: port)
    } catch {
        fail(describe(error, port: port))
    }
    defer { flasher.disconnect() }

    if options.contains("--info") {
        do {
            print("@@PROGRESS 0 Looking for the stick")
            let size = try flasher.connect(minFlashBytes: 1, baud: 115200)
            print("ESP32, \(size / 1_048_576) MB flash")
            try flasher.restartStick()
        } catch {
            fail(describe(error, port: port))
        }
        return
    }

    if let package = package {
        let bauds = preferredBaud == 115200 ? [115200] : [preferredBaud, 115200]
        var finished = false
        for (attempt, baud) in bauds.enumerated() where !finished {
            do {
                print("@@PROGRESS 1 Connecting to the stick")
                let size = try flasher.connect(minFlashBytes: package.manifest.minFlashBytes, baud: baud)
                print("ESP32 with \(size / 1_048_576) MB flash found, writing at \(baud) baud")
                print("@@PROGRESS 5 Writing firmware")
                try flasher.write(package.images) { percent, name in
                    print("@@PROGRESS \(5 + percent * 88 / 100) Writing \(name)")
                }
                print("All \(package.images.count) regions verified by the chip")
                print("@@PROGRESS 94 Restarting the stick")
                try flasher.restartStick()
                finished = true
            } catch {
                let failure = describe(error, port: port)
                let retryable = !["wrong-board", "port-busy", "port-open", "unplugged"].contains(failure.code)
                if attempt + 1 < bauds.count && retryable {
                    print("Writing at \(baud) baud failed (\(failure.code)), trying again more slowly")
                    continue
                }
                fail(failure)
            }
        }
    }

    if let settings = settings {
        do {
            print("@@PROGRESS 95 Saving your settings on the stick")
            // Opening a port toggles the reset lines and can leave the chip in its bootloader.
            // After an install the port stayed open, so the firmware is already starting.
            if package == nil { try flasher.restartStick() }
            try flasher.provision(settings: settings)
            print("Settings stored on the stick")
        } catch let failure as FlashFailure {
            fail(failure)
        } catch {
            fail(FlashFailure(code: "settings-failed", message: "The connection was lost while sending the settings."))
        }
    }

    print("@@PROGRESS 100 Done")
}

run()

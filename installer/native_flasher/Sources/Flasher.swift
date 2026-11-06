import Foundation

class Flasher {
    private let port: SerialPort
    private let esp32: ESP32Protocol

    init(portPath: String, baudRate: Int = 115200) throws {
        self.port = SerialPort(path: portPath, baudRate: baudRate)
        self.esp32 = ESP32Protocol(port: port)

        try port.open()
    }

    /// Connect to ESP32 and enter bootloader mode
    func connect() throws {
        try esp32.enterBootloader()
        try esp32.sync()
        try esp32.spiAttach()
        try esp32.spiSetParams()
        try esp32.uploadStub()
    }

    /// Erase entire flash
    func eraseFlash() throws {
        print("\n=== Erasing Flash ===")
        // Erase 16MB (max flash size for ESP32)
        try esp32.eraseFlash(size: 16 * 1024 * 1024, offset: 0)
        print("✓ Flash erased\n")
    }

    /// Write firmware to flash
    func writeFlash(firmwarePath: String, offset: UInt32 = 0x0, blockSize: UInt32 = 0) throws {
        // Use 0x4000 for stub, 0x400 for ROM
        let defaultBlockSize: UInt32 = esp32.usingStub ? 0x4000 : 0x400
        let effectiveBlockSize = blockSize > 0 ? blockSize : defaultBlockSize

        print("\n=== Writing Firmware ===")
        print("File: \(firmwarePath)")
        print("Offset: 0x\(String(offset, radix: 16))")
        print("Block size: 0x\(String(effectiveBlockSize, radix: 16))")

        // Read firmware file
        let url = URL(fileURLWithPath: firmwarePath)
        guard let firmware = try? Data(contentsOf: url) else {
            throw ESP32Error.flashFailed("Failed to read firmware file: \(firmwarePath)")
        }

        print("Size: \(firmware.count) bytes (\(String(format: "%.2f", Double(firmware.count) / 1024.0 / 1024.0)) MB)")

        // Begin flash operation
        try esp32.flashBegin(size: UInt32(firmware.count), offset: offset, blockSize: effectiveBlockSize)

        // Write blocks
        let numBlocks = (firmware.count + Int(effectiveBlockSize) - 1) / Int(effectiveBlockSize)
        print("Writing \(numBlocks) blocks...")

        var sequence: UInt32 = 0
        var bytesWritten = 0

        for blockIndex in 0..<numBlocks {
            let start = blockIndex * Int(effectiveBlockSize)
            let end = min(start + Int(effectiveBlockSize), firmware.count)
            let blockData = firmware[start..<end]

            try esp32.flashData(data: blockData, sequence: sequence)

            sequence += 1
            bytesWritten += blockData.count

            // Progress update every 10 blocks
            if blockIndex % 10 == 0 || blockIndex == numBlocks - 1 {
                let percent = (bytesWritten * 100) / firmware.count
                let bar = String(repeating: "█", count: percent / 2)
                let spaces = String(repeating: " ", count: 50 - percent / 2)
                print("\r[\(bar)\(spaces)] \(percent)% (\(bytesWritten)/\(firmware.count) bytes)", terminator: "")
                fflush(stdout)
            }
        }

        print("\n✓ Firmware written")

        // End flash operation
        try esp32.flashEnd(reboot: false)
        print("✓ Flash complete\n")
    }

    /// Write multiple firmware parts at their correct offsets (standard ESP32 flash layout)
    func writeFlashParts(firmwareDir: String) throws {
        // Standard ESP32 Arduino flash layout
        let parts: [(filename: String, offset: UInt32, description: String)] = [
            ("bootloader.bin", 0x1000, "Bootloader"),
            ("partitions.bin", 0x8000, "Partition table"),
            ("boot_app0.bin", 0xe000, "OTA data"),
            ("application.bin", 0x10000, "Application"),
        ]

        for part in parts {
            let path = "\(firmwareDir)/\(part.filename)"
            guard FileManager.default.fileExists(atPath: path) else {
                print("Skipping \(part.description) (\(part.filename) not found)")
                continue
            }

            print("\n--- \(part.description) ---")
            try writeFlash(firmwarePath: path, offset: part.offset)
        }
    }

    /// Close connection
    func disconnect() {
        port.close()
    }

    deinit {
        disconnect()
    }
}

// MARK: - Helper Functions

func findESP32Devices() -> [String] {
    let fm = FileManager.default
    let devPath = "/dev"

    do {
        let files = try fm.contentsOfDirectory(atPath: devPath)
        return files
            .filter { $0.hasPrefix("cu.usbserial-") || $0.hasPrefix("cu.SLAB_USBtoUART") }
            .map { "\(devPath)/\($0)" }
            .sorted()
    } catch {
        return []
    }
}

func formatBytes(_ bytes: Int) -> String {
    let kb = Double(bytes) / 1024.0
    let mb = kb / 1024.0

    if mb >= 1.0 {
        return String(format: "%.2f MB", mb)
    } else if kb >= 1.0 {
        return String(format: "%.2f KB", kb)
    } else {
        return "\(bytes) bytes"
    }
}

import Compression
import CryptoKit
import Foundation

/// Failures the installer app shows in plain words. `code` is stable; the app switches on it.
struct FlashFailure: Error {
    let code: String
    let message: String
}

// MARK: - Firmware package

struct FirmwareManifest: Decodable {
    struct Part: Decodable {
        let name: String
        let file: String
        let offset: UInt32
        let size: Int
        let sha256: String
    }
    /// Flash regions overwritten with 0xFF, e.g. the spot where old installers left a config file.
    struct Wipe: Decodable {
        let name: String
        let offset: UInt32
        let size: Int
    }
    let firmware: String
    let minFlashBytes: Int
    let parts: [Part]
    let wipes: [Wipe]
}

/// One region to put into the flash, ready to send.
struct FlashImage {
    let name: String
    let offset: UInt32
    let data: Data
    let compressed: Data
    let md5: Data
}

/// Reads the package and refuses it if anything doesn't match the manifest. Runs before the stick is touched.
func loadFirmwarePackage(directory: String) throws -> (manifest: FirmwareManifest, images: [FlashImage]) {
    let corrupt = { (detail: String) in
        FlashFailure(code: "firmware-corrupt", message: "The installer's firmware files are damaged (\(detail)). Please download the installer again.")
    }

    let manifestURL = URL(fileURLWithPath: directory).appendingPathComponent("manifest.json")
    guard let raw = try? Data(contentsOf: manifestURL),
          let manifest = try? JSONDecoder().decode(FirmwareManifest.self, from: raw) else {
        throw corrupt("manifest.json")
    }
    guard !manifest.parts.isEmpty, manifest.minFlashBytes > 0 else { throw corrupt("empty manifest") }

    var images: [FlashImage] = []
    for part in manifest.parts {
        guard !part.file.contains("/"), !part.file.contains("..") else { throw corrupt(part.file) }
        let url = URL(fileURLWithPath: directory).appendingPathComponent(part.file)
        guard let data = try? Data(contentsOf: url), data.count == part.size else { throw corrupt(part.file) }
        let hash = SHA256.hash(data: data).map { String(format: "%02x", $0) }.joined()
        guard hash == part.sha256.lowercased() else { throw corrupt(part.file) }
        images.append(try makeImage(name: part.name, offset: part.offset, data: data))
    }
    for wipe in manifest.wipes {
        images.append(try makeImage(name: wipe.name, offset: wipe.offset, data: Data(repeating: 0xff, count: wipe.size)))
    }

    // No region may overlap another or run off the end of the flash
    let sorted = images.sorted { $0.offset < $1.offset }
    for (i, image) in sorted.enumerated() {
        let end = Int(image.offset) + image.data.count
        guard image.offset % 4096 == 0, !image.data.isEmpty, end <= manifest.minFlashBytes else { throw corrupt(image.name) }
        if i + 1 < sorted.count, end > Int(sorted[i + 1].offset) { throw corrupt(image.name) }
    }
    return (manifest, images)
}

private func makeImage(name: String, offset: UInt32, data: Data) throws -> FlashImage {
    FlashImage(
        name: name,
        offset: offset,
        data: data,
        compressed: try zlibCompress(data),
        md5: Data(Insecure.MD5.hash(data: data))  // Only to compare with what the chip reports
    )
}

/// A zlib stream (RFC 1950), which is what the stub inflates. COMPRESSION_ZLIB alone gives raw deflate.
func zlibCompress(_ data: Data) throws -> Data {
    let capacity = data.count + data.count / 16 + 1024
    var raw = Data(count: capacity)
    let written = raw.withUnsafeMutableBytes { dst in
        data.withUnsafeBytes { src in
            compression_encode_buffer(
                dst.bindMemory(to: UInt8.self).baseAddress!, capacity,
                src.bindMemory(to: UInt8.self).baseAddress!, data.count,
                nil, COMPRESSION_ZLIB)
        }
    }
    guard written > 0 else { throw FlashFailure(code: "firmware-corrupt", message: "Could not compress the firmware.") }

    var a: UInt32 = 1, b: UInt32 = 0
    for byte in data {
        a = (a + UInt32(byte)) % 65521
        b = (b + a) % 65521
    }
    let adler = (b << 16) | a

    var stream = Data([0x78, 0x9c])
    stream.append(raw.prefix(written))
    stream.append(contentsOf: [UInt8(adler >> 24), UInt8((adler >> 16) & 0xff), UInt8((adler >> 8) & 0xff), UInt8(adler & 0xff)])
    return stream
}

// MARK: - Flasher

final class Flasher {
    private static let romBaud = 115200
    private static let blockSize = 0x4000

    private let port: SerialPort
    private let esp32: ESP32Protocol

    init(portPath: String) throws {
        port = SerialPort(path: portPath)
        esp32 = ESP32Protocol(port: port)
        try port.open()
    }

    func disconnect() {
        port.close()
    }

    /// Everything here only reads from the chip. Refuses the wrong board before any write.
    func connect(minFlashBytes: Int, baud: Int) throws -> Int {
        try port.setBaud(Flasher.romBaud)
        try esp32.enterBootloader()
        try esp32.sync()
        try esp32.verifyChip()
        try esp32.spiAttach()
        try esp32.uploadStub()

        let flashSize = try esp32.flashSizeBytes()
        guard flashSize >= minFlashBytes else { throw ESP32Error.flashTooSmall(flashSize) }
        try esp32.setFlashSize(flashSize)

        if baud != Flasher.romBaud {
            try esp32.changeBaud(to: baud, from: Flasher.romBaud)
        }
        return flashSize
    }

    /// Writes all images and has the chip confirm each one from its flash.
    func write(_ images: [FlashImage], progress: (Int, String) -> Void) throws {
        let total = images.reduce(0) { $0 + $1.compressed.count }
        var sent = 0

        for image in images {
            let compressed = image.compressed
            let blocks = (compressed.count + Flasher.blockSize - 1) / Flasher.blockSize
            try esp32.deflateBegin(
                uncompressedSize: UInt32(image.data.count),
                blocks: UInt32(blocks),
                blockSize: UInt32(Flasher.blockSize),
                offset: image.offset)

            for block in 0..<blocks {
                let start = compressed.startIndex + block * Flasher.blockSize
                let chunk = compressed.subdata(in: start..<min(start + Flasher.blockSize, compressed.endIndex))
                try esp32.deflateData(chunk, sequence: UInt32(block))
                sent += chunk.count
                progress(sent * 100 / max(total, 1), image.name)
            }

            try esp32.waitForWrites(timeout: max(10, Double(image.data.count) / 1_048_576 * 40))
            let actual = try esp32.flashMD5(offset: image.offset, size: UInt32(image.data.count))
            guard actual == image.md5 else { throw ESP32Error.verifyFailed(image.name) }
        }
        try esp32.finishFlashing()
    }

    /// Starts the firmware. The port stays at 115200 for provisioning.
    func restartStick() throws {
        try port.setBaud(Flasher.romBaud)
        try esp32.hardReset()
    }

    // MARK: Provisioning

    /// Hands settings to the running firmware. It validates them, stores them and restarts.
    /// The device's serial output is never printed: its log can name the WiFi network.
    func provision(settings: Data, bootTimeout: TimeInterval = 120) throws {
        guard !settings.contains(0x0a), !settings.contains(0x0d) else {
            throw FlashFailure(code: "settings-failed", message: "The settings could not be prepared.")
        }
        port.flush()

        let ready = Data("@@ZITRO READY".utf8)
        var seen = Data()
        var isReady = false
        let bootDeadline = Date().addingTimeInterval(bootTimeout)
        while Date() < bootDeadline && !isReady {
            try port.write(Data("zitro-ping\n".utf8))
            let waitUntil = Date().addingTimeInterval(1)
            while Date() < waitUntil && !isReady {
                seen.append(try port.readAvailable(timeout: 0.1))
                isReady = seen.range(of: ready) != nil
                if seen.count > 8192 { seen = Data(seen.suffix(64)) }
            }
        }
        guard isReady else {
            throw FlashFailure(code: "settings-failed", message: "The stick did not answer after restarting.")
        }

        usleep(100_000)
        port.flush()
        try port.write(Data("provision ".utf8) + settings + Data("\n".utf8))

        let ok = Data("@@PROVISION OK".utf8)
        let fail = Data("@@PROVISION FAIL".utf8)
        var reply = Data()
        let deadline = Date().addingTimeInterval(10)
        while Date() < deadline {
            reply.append(try port.readAvailable(timeout: 0.1))
            if reply.range(of: ok) != nil { return }
            if let range = reply.range(of: fail) {
                usleep(100_000)
                reply.append(try port.readAvailable(timeout: 0.1))
                let tail = reply[range.upperBound...].prefix { $0 != 0x0a && $0 != 0x0d }
                let reason = String(decoding: tail, as: UTF8.self).trimmingCharacters(in: .whitespaces)
                throw FlashFailure(code: "settings-rejected", message: Flasher.explain(rejection: reason))
            }
        }
        throw FlashFailure(code: "settings-failed", message: "The stick did not confirm the settings.")
    }

    private static func explain(rejection reason: String) -> String {
        switch reason {
        case "ssid": return "The stick rejected the WiFi name (1 to 32 bytes)."
        case "password": return "The stick rejected the WiFi password (empty, or 8 to 63 characters)."
        case "provider": return "The stick does not know this AI service."
        case "key": return "The stick rejected the API key (8 to 300 characters, no spaces)."
        case "text": return "The stick rejected the AI service details. A custom address must start with https://."
        default: return "The stick rejected the settings."
        }
    }
}

func findSerialPorts() -> [String] {
    let prefixes = ["cu.usbserial", "cu.wchusbserial", "cu.SLAB_USBtoUART", "cu.usbmodem"]
    let files = (try? FileManager.default.contentsOfDirectory(atPath: "/dev")) ?? []
    return files
        .filter { name in prefixes.contains { name.hasPrefix($0) } }
        .map { "/dev/\($0)" }
        .sorted()
}

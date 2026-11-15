import Foundation

enum ESP32Error: Error {
    case syncFailed(String)
    case commandFailed(String)
    case invalidResponse(String)
    case wrongChip(UInt32)
    case flashTooSmall(Int)
    case flashUnreadable(UInt32)
    case verifyFailed(String)
}

private extension Data {
    mutating func appendLE(_ value: UInt32) {
        Swift.withUnsafeBytes(of: value.littleEndian) { append(contentsOf: $0) }
    }
}

private func args(_ values: UInt32...) -> Data {
    var data = Data()
    for value in values { data.appendLE(value) }
    return data
}

/// One response frame: the header's value field plus the data after the header.
struct ESP32Response {
    let command: UInt8
    let value: UInt32
    let payload: Data
}

/// ESP32 serial bootloader protocol, ROM loader and stub.
final class ESP32Protocol {
    private enum Command {
        static let flashBegin: UInt8 = 0x02
        static let flashData: UInt8 = 0x03
        static let flashEnd: UInt8 = 0x04
        static let memBegin: UInt8 = 0x05
        static let memEnd: UInt8 = 0x06
        static let memData: UInt8 = 0x07
        static let sync: UInt8 = 0x08
        static let writeReg: UInt8 = 0x09
        static let readReg: UInt8 = 0x0a
        static let spiSetParams: UInt8 = 0x0b
        static let spiAttach: UInt8 = 0x0d
        static let changeBaud: UInt8 = 0x0f
        static let flashDeflBegin: UInt8 = 0x10
        static let flashDeflData: UInt8 = 0x11
        static let flashDeflEnd: UInt8 = 0x12
        static let flashMD5: UInt8 = 0x13
    }

    /// Only the original ESP32 has this value here; every other Espressif chip differs.
    private static let chipMagicRegister: UInt32 = 0x4000_1000
    static let esp32ChipMagic: UInt32 = 0x00f0_1d83

    private let port: SerialPort
    private(set) var usingStub = false

    init(port: SerialPort) {
        self.port = port
    }

    // MARK: Reset

    /// Classic esptool sequence. DTR drives IO0 and RTS drives EN, both active low through the
    /// board's transistors: the chip leaves reset with IO0 low and starts the ROM loader.
    func enterBootloader() throws {
        usingStub = false
        try port.setDTR(false)
        try port.setRTS(true)
        usleep(100_000)
        try port.setDTR(true)
        try port.setRTS(false)
        usleep(50_000)
        try port.setDTR(false)
        usleep(50_000)
        port.flush()
    }

    /// Reset into the firmware.
    func hardReset() throws {
        usingStub = false
        try port.setDTR(false)
        try port.setRTS(true)
        usleep(100_000)
        try port.setRTS(false)
    }

    // MARK: Connecting

    func sync() throws {
        var lastError: Error?
        for round in 1...3 {
            if round > 1 {
                try enterBootloader()
                usleep(200_000)
            }
            for _ in 1...7 {
                do {
                    port.flush()
                    let payload = Data([0x07, 0x07, 0x12, 0x20] + [UInt8](repeating: 0x55, count: 32))
                    try send(Command.sync, payload)
                    _ = try readResponse(Command.sync, timeout: 0.5)

                    // The ROM answers one sync with several frames: let them all arrive
                    usleep(300_000)
                    while try !port.readAvailable(timeout: 0.1).isEmpty {}
                    port.flush()
                    return
                } catch let error as SerialPortError {
                    throw error
                } catch {
                    lastError = error
                }
            }
        }
        throw ESP32Error.syncFailed("No answer from the bootloader: \(String(describing: lastError))")
    }

    /// Refuses anything but an ESP32, before anything is written.
    func verifyChip() throws {
        let magic = try readRegister(ESP32Protocol.chipMagicRegister)
        guard magic == ESP32Protocol.esp32ChipMagic else { throw ESP32Error.wrongChip(magic) }
    }

    func spiAttach() throws {
        try send(Command.spiAttach, Data(count: 8))
        _ = try readResponse(Command.spiAttach)
    }

    func uploadStub() throws {
        try uploadSegment(ESP32Stub.textData, to: ESP32Stub.textStart)
        try uploadSegment(ESP32Stub.dataData, to: ESP32Stub.dataStart)

        // The stub takes over right after this: its "OHAI" may arrive with or before the reply
        try send(Command.memEnd, args(0, ESP32Stub.entry))

        let hello = Data("OHAI".utf8)
        let deadline = Date().addingTimeInterval(5)
        var received = Data()
        while Date() < deadline {
            received.append(try port.readAvailable(timeout: 0.2))
            if received.range(of: hello) != nil {
                usingStub = true
                usleep(50_000)
                port.flush()
                return
            }
        }
        throw ESP32Error.syncFailed("The flasher program did not start")
    }

    private func uploadSegment(_ data: Data, to address: UInt32) throws {
        let blockSize: UInt32 = 0x1800
        let blocks = (UInt32(data.count) + blockSize - 1) / blockSize
        try send(Command.memBegin, args(UInt32(data.count), blocks, blockSize, address))
        _ = try readResponse(Command.memBegin)

        for block in 0..<Int(blocks) {
            let start = block * Int(blockSize)
            let chunk = data.subdata(in: start..<min(start + Int(blockSize), data.count))
            try send(Command.memData, args(UInt32(chunk.count), UInt32(block), 0, 0) + chunk, checksum: checksum(chunk))
            _ = try readResponse(Command.memData)
        }
    }

    /// Asks the stub to talk faster. The reply still comes at the old rate.
    func changeBaud(to newBaud: Int, from oldBaud: Int) throws {
        try send(Command.changeBaud, args(UInt32(newBaud), UInt32(oldBaud)))
        _ = try readResponse(Command.changeBaud)
        try port.setBaud(newBaud)
        usleep(50_000)
        port.flush()
    }

    // MARK: Flash size

    /// Reads the flash's JEDEC ID by driving the SPI1 peripheral directly, like esptool.
    func flashSizeBytes() throws -> Int {
        let base: UInt32 = 0x3ff4_2000
        let cmdReg = base + 0x00, usrReg = base + 0x1c, usr2Reg = base + 0x24
        let mosiLenReg = base + 0x28, misoLenReg = base + 0x2c, w0Reg = base + 0x80
        let usrCommand: UInt32 = 1 << 31, usrMiso: UInt32 = 1 << 28, cmdStart: UInt32 = 1 << 18
        let readID: UInt32 = 0x9f

        let oldUsr = try readRegister(usrReg)
        let oldUsr2 = try readRegister(usr2Reg)
        try writeRegister(mosiLenReg, 0)
        try writeRegister(misoLenReg, 24 - 1)
        try writeRegister(usrReg, usrCommand | usrMiso)
        try writeRegister(usr2Reg, (7 << 28) | readID)
        try writeRegister(w0Reg, 0)
        try writeRegister(cmdReg, cmdStart)

        var finished = false
        for _ in 0..<10 where !finished {
            finished = try readRegister(cmdReg) & cmdStart == 0
        }
        let id = try readRegister(w0Reg)
        try writeRegister(usrReg, oldUsr)
        try writeRegister(usr2Reg, oldUsr2)

        // Third ID byte is log2(size). 0x00 / 0xFF mean no answer from the chip.
        let capacity = (id >> 16) & 0xff
        guard finished, capacity >= 0x10, capacity <= 0x1f else { throw ESP32Error.flashUnreadable(id) }
        return 1 << Int(capacity)
    }

    func setFlashSize(_ size: Int) throws {
        // fl_id, total size, block, sector, page, status mask
        try send(Command.spiSetParams, args(0, UInt32(size), 0x10000, 0x1000, 0x100, 0xffff))
        _ = try readResponse(Command.spiSetParams)
    }

    // MARK: Compressed flash writes (stub only)

    func deflateBegin(uncompressedSize: UInt32, blocks: UInt32, blockSize: UInt32, offset: UInt32) throws {
        try send(Command.flashDeflBegin, args(uncompressedSize, blocks, blockSize, offset))
        _ = try readResponse(Command.flashDeflBegin, timeout: 10)
    }

    func deflateData(_ data: Data, sequence: UInt32) throws {
        try send(Command.flashDeflData, args(UInt32(data.count), sequence, 0, 0) + data, checksum: checksum(data))
        _ = try readResponse(Command.flashDeflData, timeout: 30)
    }

    /// The stub acknowledges a block before writing it. A register read only gets
    /// its answer once every block is really in the flash.
    func waitForWrites(timeout: TimeInterval) throws {
        _ = try readRegister(ESP32Protocol.chipMagicRegister, timeout: timeout)
    }

    /// MD5 the chip computes from what is really in its flash.
    func flashMD5(offset: UInt32, size: UInt32) throws -> Data {
        let megabytes = Double(size) / 1_048_576
        try send(Command.flashMD5, args(offset, size, 0, 0))
        let response = try readResponse(Command.flashMD5, timeout: max(5, megabytes * 10))
        guard response.payload.count >= 16 else {
            throw ESP32Error.invalidResponse("Short checksum reply")
        }
        return response.payload.prefix(16)
    }

    /// Leaves the stub's flash mode, staying in the loader until hardReset().
    func finishFlashing() throws {
        try send(Command.flashBegin, args(0, 0, 0x4000, 0))
        _ = try readResponse(Command.flashBegin)
        try send(Command.flashDeflEnd, args(1))
        _ = try readResponse(Command.flashDeflEnd)
    }

    // MARK: Registers

    func readRegister(_ address: UInt32, timeout: TimeInterval = 3) throws -> UInt32 {
        try send(Command.readReg, args(address))
        return try readResponse(Command.readReg, timeout: timeout).value
    }

    private func writeRegister(_ address: UInt32, _ value: UInt32) throws {
        try send(Command.writeReg, args(address, value, 0xffff_ffff, 0))
        _ = try readResponse(Command.writeReg)
    }

    // MARK: Frames

    private func send(_ command: UInt8, _ data: Data, checksum: UInt32 = 0) throws {
        var packet = Data([0x00, command])
        packet.append(UInt8(data.count & 0xff))
        packet.append(UInt8((data.count >> 8) & 0xff))
        packet.appendLE(checksum)
        packet.append(data)
        try port.write(SLIPProtocol.encode(packet))
    }

    /// Waits for the reply to `command`. Replies to anything else (extra sync frames) are skipped.
    private func readResponse(_ command: UInt8, timeout: TimeInterval = 3) throws -> ESP32Response {
        let deadline = Date().addingTimeInterval(timeout)
        var buffer = Data()
        repeat {
            buffer.append(try port.readAvailable(timeout: 0.05))
            while true {
                let (frame, rest) = SLIPProtocol.extractPacket(buffer)
                guard let packet = frame else { break }
                buffer = rest
                let bytes = [UInt8](packet)
                guard bytes.count >= 8, bytes[0] == 0x01, bytes[1] == command else { continue }

                let value = UInt32(bytes[4]) | UInt32(bytes[5]) << 8 | UInt32(bytes[6]) << 16 | UInt32(bytes[7]) << 24
                let payload = Data(bytes[8...])
                // The status sits at the end: 4 bytes from the ROM, 2 from the stub
                let statusLength = usingStub ? 2 : 4
                guard payload.count >= statusLength else {
                    throw ESP32Error.invalidResponse("Reply without status")
                }
                let status = payload[payload.count - statusLength]
                let code = payload[payload.count - statusLength + 1]
                guard status == 0 else {
                    throw ESP32Error.commandFailed(String(format: "Command 0x%02x failed (error 0x%02x)", command, code))
                }
                return ESP32Response(command: command, value: value, payload: payload)
            }
        } while Date() < deadline
        throw ESP32Error.invalidResponse(String(format: "No reply to command 0x%02x", command))
    }

    private func checksum(_ data: Data) -> UInt32 {
        data.reduce(UInt32(0xef)) { $0 ^ UInt32($1) }
    }
}

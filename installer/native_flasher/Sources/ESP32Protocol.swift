import Foundation

enum ESP32Error: Error {
    case syncFailed(String)
    case commandFailed(String)
    case invalidResponse(String)
    case flashFailed(String)
}

/// ESP32 ROM Bootloader Protocol
class ESP32Protocol {
    // Commands
    static let FLASH_BEGIN: UInt8 = 0x02
    static let FLASH_DATA: UInt8 = 0x03
    static let FLASH_END: UInt8 = 0x04
    static let MEM_BEGIN: UInt8 = 0x05
    static let MEM_END: UInt8 = 0x06
    static let MEM_DATA: UInt8 = 0x07
    static let SYNC: UInt8 = 0x08
    static let WRITE_REG: UInt8 = 0x09
    static let READ_REG: UInt8 = 0x0a
    static let SPI_SET_PARAMS: UInt8 = 0x0b
    static let SPI_ATTACH: UInt8 = 0x0d
    static let CHANGE_BAUDRATE: UInt8 = 0x0f

    // Response values
    static let RESP_OK: UInt8 = 0x00
    static let RESP_FAIL: UInt8 = 0x01

    private let port: SerialPort
    private var sequenceNumber: UInt16 = 0

    init(port: SerialPort) {
        self.port = port
    }

    /// Reset ESP32 into bootloader mode using esptool-compatible sequence
    func enterBootloader() throws {
        print("Entering bootloader mode...")

        // Match esptool.py classic reset sequence exactly:
        // DTR controls IO0 (GPIO0): asserted(true) = IO0 LOW, deasserted(false) = IO0 HIGH
        // RTS controls EN (reset):  asserted(true) = EN LOW,  deasserted(false) = EN HIGH
        // Bootloader entry requires: IO0=LOW when EN goes from LOW to HIGH

        // Step 1: Deassert both
        try port.setDTR(false)  // IO0 = HIGH
        try port.setRTS(false)  // EN = HIGH
        usleep(100000)  // 100ms settle

        // Step 2: Assert RTS to hold chip in reset
        try port.setDTR(false)  // IO0 = HIGH
        try port.setRTS(true)   // EN = LOW (hold reset)
        usleep(100000)  // 100ms

        // Step 3: Assert DTR (IO0=LOW) then release RTS (EN=HIGH)
        // Chip exits reset with IO0 held LOW -> enters bootloader
        try port.setDTR(true)   // IO0 = LOW (select bootloader)
        try port.setRTS(false)  // EN = HIGH (release reset)
        usleep(50000)   // 50ms for bootloader to start

        // Step 4: Release DTR
        try port.setDTR(false)  // IO0 = HIGH (release)
        usleep(50000)   // 50ms

        port.flush()
        print("Bootloader entry sequence complete")
    }

    /// Synchronize with ESP32 bootloader
    func sync(attempts: Int = 10) throws {
        print("Synchronizing with ESP32...")

        var lastError: Error?

        for attempt in 1...attempts {
            do {
                port.flush()

                // Send sync command
                let syncData = Data([0x07, 0x07, 0x12, 0x20] + Array(repeating: UInt8(0x55), count: 32))
                try sendCommand(command: ESP32Protocol.SYNC, data: syncData, checksum: 0)

                // Read response with longer timeout
                _ = try readResponse(timeout: 1.0)

                print("Sync successful (attempt \(attempt)/\(attempts))")

                // Important: drain ALL remaining sync responses and any other data
                print("Draining remaining responses...")
                usleep(500000)  // 500ms to let all responses arrive
                var drained = 0
                while true {
                    let available = try port.readAvailable(timeout: 0.2)
                    if available.isEmpty { break }
                    drained += available.count
                }
                if drained > 0 {
                    print("  Drained \(drained) bytes")
                }
                port.flush()

                // Test with a simple READ_REG command to verify communication
                print("Verifying connection...")
                try testReadReg()

                return
            } catch {
                lastError = error
                print("  Sync attempt \(attempt)/\(attempts) failed: \(error)")
                usleep(100000)  // Wait 100ms before retry
            }
        }

        // If all sync attempts failed, try re-entering bootloader and syncing again
        print("All sync attempts failed, retrying bootloader entry...")
        try enterBootloader()
        usleep(200000)  // 200ms extra wait

        for attempt in 1...5 {
            do {
                port.flush()
                let syncData = Data([0x07, 0x07, 0x12, 0x20] + Array(repeating: UInt8(0x55), count: 32))
                try sendCommand(command: ESP32Protocol.SYNC, data: syncData, checksum: 0)
                _ = try readResponse(timeout: 1.0)

                print("Sync successful on retry (attempt \(attempt)/5)")

                // Drain remaining data
                usleep(500000)
                while true {
                    let available = try port.readAvailable(timeout: 0.2)
                    if available.isEmpty { break }
                }
                port.flush()

                try testReadReg()
                return
            } catch {
                lastError = error
                print("  Retry sync \(attempt)/5 failed: \(error)")
                usleep(200000)
            }
        }

        throw ESP32Error.syncFailed("Failed to sync after all attempts: \(lastError!)")
    }

    /// Test communication by reading a register (UART date register)
    private func testReadReg() throws {
        // Read UART_DATE_REG (0x60000078) - a read-only register that always returns a value
        let regAddr: UInt32 = 0x60000078
        var data = Data()
        data.append(contentsOf: withUnsafeBytes(of: regAddr.littleEndian) { Data($0) })

        try sendCommand(command: ESP32Protocol.READ_REG, data: data, checksum: 0)
        let response = try readResponse(timeout: 3.0)
        print("  Register read OK (response: \(response.count) bytes)")
    }

    /// Attach the SPI flash - required before any flash operations
    func spiAttach() throws {
        print("Attaching SPI flash...")

        // SPI_ATTACH command: 8 bytes of zeros (use default SPI pins)
        let data = Data(repeating: 0, count: 8)

        try sendCommand(command: ESP32Protocol.SPI_ATTACH, data: data, checksum: 0)
        _ = try readResponse(timeout: 3.0)
        print("  SPI flash attached")
    }

    /// Upload and run the stub flasher
    func uploadStub() throws {
        print("Uploading stub flasher...")

        let textData = ESP32Stub.textData
        let dataData = ESP32Stub.dataData
        let blockSize: UInt32 = 0x1800  // 6144 - same as esptool

        // Upload text segment
        print("  Uploading text segment (\(textData.count) bytes at 0x\(String(ESP32Stub.textStart, radix: 16)))...")
        let textBlocks = (UInt32(textData.count) + blockSize - 1) / blockSize
        try memBegin(size: UInt32(textData.count), blocks: textBlocks, blockSize: blockSize, offset: ESP32Stub.textStart)
        try memData(data: textData, sequence: 0, blockSize: blockSize)

        // Upload data segment
        print("  Uploading data segment (\(dataData.count) bytes at 0x\(String(ESP32Stub.dataStart, radix: 16)))...")
        let dataBlocks = (UInt32(dataData.count) + blockSize - 1) / blockSize
        try memBegin(size: UInt32(dataData.count), blocks: dataBlocks, blockSize: blockSize, offset: ESP32Stub.dataStart)
        try memData(data: dataData, sequence: 0, blockSize: blockSize)

        // Run the stub (MEM_END with entry point)
        print("  Starting stub at 0x\(String(ESP32Stub.entry, radix: 16))...")

        // Send MEM_END - the stub takes over, so we handle response specially
        var memEndData = Data()
        let execFlag: UInt32 = 0  // 0 = execute
        memEndData.append(contentsOf: withUnsafeBytes(of: execFlag.littleEndian) { Data($0) })
        memEndData.append(contentsOf: withUnsafeBytes(of: ESP32Stub.entry.littleEndian) { Data($0) })
        try sendCommand(command: ESP32Protocol.MEM_END, data: memEndData, checksum: 0)

        // Wait for "OHAI" - the stub sends this as raw ASCII (not SLIP-encoded)
        // The MEM_END SLIP response and "OHAI" may arrive together
        print("  Waiting for stub to start...")
        let deadline = Date().addingTimeInterval(5.0)
        var received = Data()

        while Date() < deadline {
            let available = try port.readAvailable(timeout: 0.5)
            if !available.isEmpty {
                received.append(available)
                // Check for "OHAI" bytes (0x4f 0x48 0x41 0x49) anywhere in received data
                let ohaiBytes = Data([0x4f, 0x48, 0x41, 0x49])  // "OHAI"
                if received.range(of: ohaiBytes) != nil {
                    print("  Stub running! (received OHAI)")
                    usingStub = true
                    // Flush any remaining data
                    usleep(50000)
                    port.flush()
                    return
                }
            }
        }

        let hexStr = received.map { String(format: "%02x", $0) }.joined(separator: " ")
        throw ESP32Error.syncFailed("Stub did not send OHAI (received \(received.count) bytes: \(hexStr))")
    }

    /// MEM_BEGIN - prepare to write to memory
    private func memBegin(size: UInt32, blocks: UInt32, blockSize: UInt32, offset: UInt32) throws {
        var data = Data()
        data.append(contentsOf: withUnsafeBytes(of: size.littleEndian) { Data($0) })
        data.append(contentsOf: withUnsafeBytes(of: blocks.littleEndian) { Data($0) })
        data.append(contentsOf: withUnsafeBytes(of: blockSize.littleEndian) { Data($0) })
        data.append(contentsOf: withUnsafeBytes(of: offset.littleEndian) { Data($0) })

        try sendCommand(command: ESP32Protocol.MEM_BEGIN, data: data, checksum: 0)
        _ = try readResponse(timeout: 3.0)
    }

    /// MEM_DATA - write data to memory
    private func memData(data: Data, sequence: UInt32, blockSize: UInt32) throws {
        var packet = Data()
        packet.append(contentsOf: withUnsafeBytes(of: UInt32(data.count).littleEndian) { Data($0) })
        packet.append(contentsOf: withUnsafeBytes(of: sequence.littleEndian) { Data($0) })
        packet.append(contentsOf: withUnsafeBytes(of: UInt32(0).littleEndian) { Data($0) })  // Reserved
        packet.append(contentsOf: withUnsafeBytes(of: UInt32(0).littleEndian) { Data($0) })  // Reserved
        packet.append(data)

        let checksum = calculateChecksum(data)
        try sendCommand(command: ESP32Protocol.MEM_DATA, data: packet, checksum: checksum)
        _ = try readResponse(timeout: 3.0)
    }

    /// MEM_END - finish memory write, optionally execute
    private func memEnd(execute: Bool, entryPoint: UInt32 = 0) throws {
        var data = Data()
        let execFlag: UInt32 = execute ? 0 : 1  // 0 = execute, 1 = don't execute
        data.append(contentsOf: withUnsafeBytes(of: execFlag.littleEndian) { Data($0) })
        data.append(contentsOf: withUnsafeBytes(of: entryPoint.littleEndian) { Data($0) })

        try sendCommand(command: ESP32Protocol.MEM_END, data: data, checksum: 0)
        // MEM_END response may or may not come depending on whether stub takes over
        do {
            _ = try readResponse(timeout: 0.5)
        } catch {
            // Expected - stub might take over before response
        }
    }

    /// Whether we're communicating with the stub (vs ROM bootloader)
    var usingStub = false

    /// Set SPI flash parameters
    func spiSetParams(flashSize: UInt32 = 4 * 1024 * 1024) throws {
        print("Setting SPI flash parameters...")

        // Parameters: fl_id=0, total_size, block_size, sector_size, page_size, status_mask
        var data = Data()
        data.append(contentsOf: withUnsafeBytes(of: UInt32(0).littleEndian) { Data($0) })  // fl_id
        data.append(contentsOf: withUnsafeBytes(of: flashSize.littleEndian) { Data($0) })  // total_size
        data.append(contentsOf: withUnsafeBytes(of: UInt32(0x10000).littleEndian) { Data($0) })  // block_size (64KB)
        data.append(contentsOf: withUnsafeBytes(of: UInt32(0x1000).littleEndian) { Data($0) })   // sector_size (4KB)
        data.append(contentsOf: withUnsafeBytes(of: UInt32(0x100).littleEndian) { Data($0) })    // page_size (256B)
        data.append(contentsOf: withUnsafeBytes(of: UInt32(0xFFFF).littleEndian) { Data($0) })   // status_mask

        try sendCommand(command: ESP32Protocol.SPI_SET_PARAMS, data: data, checksum: 0)
        _ = try readResponse(timeout: 3.0)
        print("  SPI parameters set (flash size: \(flashSize / 1024 / 1024) MB)")
    }

    /// Erase flash
    func eraseFlash(size: UInt32 = 0, offset: UInt32 = 0) throws {
        print("Erasing flash...")

        // Calculate erase parameters
        let numBlocks = (size + 0xFFFF) / 0x10000  // 64KB blocks
        let eraseSize = numBlocks * 0x10000

        // Prepare erase command data
        var data = Data()
        data.append(contentsOf: withUnsafeBytes(of: eraseSize.littleEndian) { Data($0) })
        data.append(contentsOf: withUnsafeBytes(of: numBlocks.littleEndian) { Data($0) })
        data.append(contentsOf: withUnsafeBytes(of: UInt32(0x1000).littleEndian) { Data($0) })  // Block size
        data.append(contentsOf: withUnsafeBytes(of: offset.littleEndian) { Data($0) })

        try sendCommand(command: ESP32Protocol.FLASH_BEGIN, data: data, checksum: 0, timeout: 30.0)
        _ = try readResponse(timeout: 30.0)

        print("Flash erased successfully")
    }

    /// Begin flash write operation
    func flashBegin(size: UInt32, offset: UInt32, blockSize: UInt32 = 0x400) throws {
        let numBlocks = (size + blockSize - 1) / blockSize

        print("Starting flash write...")
        print("  Size: \(size) bytes")
        print("  Offset: 0x\(String(offset, radix: 16))")
        print("  Block size: \(blockSize)")
        print("  Num blocks: \(numBlocks)")

        // Flush any leftover data before starting
        port.flush()
        usleep(50000)

        var data = Data()
        data.append(contentsOf: withUnsafeBytes(of: size.littleEndian) { Data($0) })
        data.append(contentsOf: withUnsafeBytes(of: numBlocks.littleEndian) { Data($0) })
        data.append(contentsOf: withUnsafeBytes(of: blockSize.littleEndian) { Data($0) })
        data.append(contentsOf: withUnsafeBytes(of: offset.littleEndian) { Data($0) })

        let cmdHex = data.map { String(format: "%02x", $0) }.joined(separator: " ")
        print("  Command data: \(cmdHex)")

        try sendCommand(command: ESP32Protocol.FLASH_BEGIN, data: data, checksum: 0)

        // Flash erase can take a VERY long time on ESP32 ROM bootloader
        // ~20-100ms per 4KB sector, so 1.6MB = ~400 sectors = up to 40+ seconds
        let numSectors = (size + 0xFFF) / 0x1000
        let eraseTimeout = max(30.0, Double(numSectors) * 0.15)  // 150ms per sector estimate
        print("  Waiting for flash erase (~\(numSectors) sectors, timeout: \(String(format: "%.0f", eraseTimeout))s)...")
        _ = try readResponse(timeout: eraseTimeout)
        print("  Flash begin OK")
    }

    /// Write a block of data to flash
    func flashData(data: Data, sequence: UInt32) throws {
        let paddedSize = (data.count + 3) & ~3  // Align to 4 bytes
        var paddedData = data
        if paddedSize > data.count {
            paddedData.append(Data(repeating: 0xFF, count: paddedSize - data.count))
        }

        let checksum = calculateChecksum(paddedData)

        var packet = Data()
        packet.append(contentsOf: withUnsafeBytes(of: UInt32(paddedData.count).littleEndian) { Data($0) })
        packet.append(contentsOf: withUnsafeBytes(of: sequence.littleEndian) { Data($0) })
        packet.append(contentsOf: withUnsafeBytes(of: UInt32(0).littleEndian) { Data($0) })  // Reserved
        packet.append(contentsOf: withUnsafeBytes(of: UInt32(0).littleEndian) { Data($0) })  // Reserved
        packet.append(paddedData)

        try sendCommand(command: ESP32Protocol.FLASH_DATA, data: packet, checksum: checksum, timeout: 5.0)
        _ = try readResponse(timeout: 5.0)
    }

    /// End flash write operation
    func flashEnd(reboot: Bool = false) throws {
        print("Finishing flash...")

        let rebootFlag: UInt32 = reboot ? 0 : 1
        var data = Data()
        data.append(contentsOf: withUnsafeBytes(of: rebootFlag.littleEndian) { Data($0) })

        try sendCommand(command: ESP32Protocol.FLASH_END, data: data, checksum: 0)
        _ = try readResponse(timeout: 1.0)

        print("Flash complete!")
    }

    // MARK: - Private Methods

    private func sendCommand(command: UInt8, data: Data, checksum: UInt32, timeout: TimeInterval = 3.0) throws {
        sequenceNumber &+= 1

        var packet = Data()
        packet.append(0x00)  // Direction: request
        packet.append(command)
        packet.append(contentsOf: withUnsafeBytes(of: UInt16(data.count).littleEndian) { Data($0) })
        packet.append(contentsOf: withUnsafeBytes(of: checksum.littleEndian) { Data($0) })
        packet.append(data)

        let slipPacket = SLIPProtocol.encode(packet)
        try port.write(slipPacket)
    }

    private var verbose = false

    private func readResponse(timeout: TimeInterval = 3.0) throws -> Data {
        let deadline = Date().addingTimeInterval(timeout)
        var buffer = Data()

        while Date() < deadline {
            let available = try port.readAvailable(timeout: 0.1)
            if !available.isEmpty {
                buffer.append(available)
                if verbose && buffer.count <= 128 {
                    let hexStr = available.map { String(format: "%02x", $0) }.joined(separator: " ")
                    print("    << recv \(available.count) bytes: \(hexStr)")
                }
            }

            // Try to extract a complete SLIP packet
            while true {
                let (packet, remaining) = SLIPProtocol.extractPacket(buffer)
                guard let packet = packet, packet.count >= 8 else {
                    break
                }

                buffer = remaining

                // Verify response direction
                if packet[0] != 0x01 {
                    if verbose {
                        print("    << skipping non-response packet (dir=\(packet[0]))")
                    }
                    continue
                }

                let cmd = packet[1]

                // For responses with status (>= 10 bytes)
                if packet.count >= 10 {
                    let status = packet[8]
                    let errorCode = packet[9]
                    if verbose {
                        print("    << response: cmd=0x\(String(format: "%02x", cmd)) status=\(status) error=\(errorCode) len=\(packet.count)")
                    }
                    if status != ESP32Protocol.RESP_OK {
                        throw ESP32Error.commandFailed("Command 0x\(String(format: "%02x", cmd)) failed with status=\(status) error=\(errorCode)")
                    }
                }

                return packet
            }

            usleep(10000)  // 10ms
        }

        if !buffer.isEmpty {
            let hexStr = buffer.prefix(64).map { String(format: "%02x", $0) }.joined(separator: " ")
            throw ESP32Error.invalidResponse("Response timeout (received \(buffer.count) bytes: \(hexStr))")
        }
        throw ESP32Error.invalidResponse("Response timeout (no data received)")
    }

    private func calculateChecksum(_ data: Data) -> UInt32 {
        var checksum: UInt32 = 0xEF
        for byte in data {
            checksum ^= UInt32(byte)
        }
        return checksum
    }
}

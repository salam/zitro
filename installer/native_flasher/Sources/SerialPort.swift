import Foundation

#if os(macOS)
import Darwin
#endif

enum SerialPortError: Error {
    case openFailed(String)
    case configFailed(String)
    case writeFailed(String)
    case readFailed(String)
    case timeout
}

class SerialPort {
    private var fileDescriptor: Int32 = -1
    private let path: String
    private let baudRate: speed_t

    init(path: String, baudRate: Int = 115200) {
        self.path = path
        // Convert baud rate to speed_t
        switch baudRate {
        case 9600: self.baudRate = speed_t(B9600)
        case 19200: self.baudRate = speed_t(B19200)
        case 38400: self.baudRate = speed_t(B38400)
        case 57600: self.baudRate = speed_t(B57600)
        case 115200: self.baudRate = speed_t(B115200)
        case 230400: self.baudRate = speed_t(B230400)
        case 460800: self.baudRate = 460800
        case 921600: self.baudRate = 921600
        default: self.baudRate = speed_t(B115200)
        }
    }

    func open() throws {
        // Open the serial port with O_NONBLOCK to avoid hanging on open
        fileDescriptor = Darwin.open(path, O_RDWR | O_NOCTTY | O_NONBLOCK)

        if fileDescriptor == -1 {
            throw SerialPortError.openFailed("Failed to open \(path): \(String(cString: strerror(errno)))")
        }

        // Clear O_NONBLOCK now that port is open - this ensures tcdrain() works correctly
        let flags = fcntl(fileDescriptor, F_GETFL)
        if flags != -1 {
            _ = fcntl(fileDescriptor, F_SETFL, flags & ~O_NONBLOCK)
        }

        // Get current port settings
        var options = termios()
        if tcgetattr(fileDescriptor, &options) != 0 {
            close()
            throw SerialPortError.configFailed("Failed to get port attributes")
        }

        // Set baud rate
        cfsetispeed(&options, baudRate)
        cfsetospeed(&options, baudRate)

        // Configure port: 8N1, no flow control
        options.c_cflag |= UInt(CS8 | CLOCAL | CREAD)
        options.c_iflag = UInt(IGNPAR)
        options.c_oflag = 0
        options.c_lflag = 0

        // Set timeouts: VMIN=0, VTIME=1 (0.1s timeout for reads)
        options.c_cc.16 = 0  // VMIN
        options.c_cc.17 = 1  // VTIME (0.1s)

        // Apply settings
        tcflush(fileDescriptor, TCIFLUSH)
        if tcsetattr(fileDescriptor, TCSANOW, &options) != 0 {
            close()
            throw SerialPortError.configFailed("Failed to set port attributes")
        }
    }

    func close() {
        if fileDescriptor != -1 {
            Darwin.close(fileDescriptor)
            fileDescriptor = -1
        }
    }

    func write(_ data: Data) throws {
        guard fileDescriptor != -1 else {
            throw SerialPortError.writeFailed("Port not open")
        }

        try data.withUnsafeBytes { (ptr: UnsafeRawBufferPointer) in
            let written = Darwin.write(fileDescriptor, ptr.baseAddress!, data.count)
            if written < 0 {
                throw SerialPortError.writeFailed("Write failed: \(String(cString: strerror(errno)))")
            }
            if written != data.count {
                throw SerialPortError.writeFailed("Incomplete write: \(written)/\(data.count) bytes")
            }
        }

        // Ensure data is transmitted
        tcdrain(fileDescriptor)
    }

    func read(count: Int, timeout: TimeInterval = 1.0) throws -> Data {
        guard fileDescriptor != -1 else {
            throw SerialPortError.readFailed("Port not open")
        }

        var buffer = Data(count: count)
        var totalRead = 0
        let deadline = Date().addingTimeInterval(timeout)

        while totalRead < count && Date() < deadline {
            let remaining = count - totalRead
            let bytesRead = buffer.withUnsafeMutableBytes { ptr -> Int in
                let basePtr = ptr.baseAddress!.advanced(by: totalRead)
                return Darwin.read(fileDescriptor, basePtr, remaining)
            }

            if bytesRead > 0 {
                totalRead += bytesRead
            } else if bytesRead == 0 {
                // No data available, sleep briefly
                usleep(10000) // 10ms
            } else {
                throw SerialPortError.readFailed("Read failed: \(String(cString: strerror(errno)))")
            }
        }

        if totalRead < count {
            throw SerialPortError.timeout
        }

        return buffer.prefix(totalRead)
    }

    func readAvailable(maxBytes: Int = 4096, timeout: TimeInterval = 0.1) throws -> Data {
        guard fileDescriptor != -1 else {
            throw SerialPortError.readFailed("Port not open")
        }

        var buffer = Data(count: maxBytes)
        var totalRead = 0
        let deadline = Date().addingTimeInterval(timeout)

        while Date() < deadline {
            let remaining = maxBytes - totalRead
            let bytesRead = buffer.withUnsafeMutableBytes { ptr -> Int in
                let basePtr = ptr.baseAddress!.advanced(by: totalRead)
                return Darwin.read(fileDescriptor, basePtr, remaining)
            }

            if bytesRead > 0 {
                totalRead += bytesRead
            } else if bytesRead == 0 {
                // No more data immediately available
                if totalRead > 0 {
                    break
                }
                usleep(10000) // 10ms
            } else {
                if errno == EAGAIN || errno == EWOULDBLOCK {
                    if totalRead > 0 {
                        break
                    }
                    usleep(10000)
                } else {
                    throw SerialPortError.readFailed("Read failed: \(String(cString: strerror(errno)))")
                }
            }
        }

        return buffer.prefix(totalRead)
    }

    func flush() {
        if fileDescriptor != -1 {
            tcflush(fileDescriptor, TCIOFLUSH)
        }
    }

    func setDTR(_ enabled: Bool) throws {
        guard fileDescriptor != -1 else { return }

        var status: Int32 = 0
        if ioctl(fileDescriptor, TIOCMGET, &status) == -1 {
            throw SerialPortError.configFailed("Failed to get modem status")
        }

        if enabled {
            status |= TIOCM_DTR
        } else {
            status &= ~TIOCM_DTR
        }

        if ioctl(fileDescriptor, TIOCMSET, &status) == -1 {
            throw SerialPortError.configFailed("Failed to set DTR")
        }
    }

    func setRTS(_ enabled: Bool) throws {
        guard fileDescriptor != -1 else { return }

        var status: Int32 = 0
        if ioctl(fileDescriptor, TIOCMGET, &status) == -1 {
            throw SerialPortError.configFailed("Failed to get modem status")
        }

        if enabled {
            status |= TIOCM_RTS
        } else {
            status &= ~TIOCM_RTS
        }

        if ioctl(fileDescriptor, TIOCMSET, &status) == -1 {
            throw SerialPortError.configFailed("Failed to set RTS")
        }
    }

    deinit {
        close()
    }
}

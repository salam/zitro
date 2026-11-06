import Foundation
import Darwin

enum SerialPortError: Error {
    case busy(String)
    case openFailed(String)
    case configFailed(String)
    case writeFailed(String)
    case readFailed(String)
}

final class SerialPort {
    /// _IOW('T', 2, speed_t) from <IOKit/serial/ioss.h>: the only way to set rates above 230400 on macOS.
    private static let IOSSIOSPEED: UInt = 0x8008_5402

    private var fd: Int32 = -1
    private let path: String

    init(path: String) {
        self.path = path
    }

    func open() throws {
        // O_NONBLOCK so open() can't hang waiting for carrier
        fd = Darwin.open(path, O_RDWR | O_NOCTTY | O_NONBLOCK)
        if fd == -1 {
            let reason = String(cString: strerror(errno))
            if errno == EBUSY { throw SerialPortError.busy("\(path) is in use by another program") }
            throw SerialPortError.openFailed("Could not open \(path): \(reason)")
        }

        // Nobody else may open the port while we write the flash
        if ioctl(fd, TIOCEXCL) == -1 && errno != ENOTTY {
            let reason = String(cString: strerror(errno))
            close()
            throw SerialPortError.busy("\(path) could not be locked: \(reason)")
        }

        let flags = fcntl(fd, F_GETFL)
        if flags != -1 { _ = fcntl(fd, F_SETFL, flags & ~O_NONBLOCK) }

        var options = termios()
        guard tcgetattr(fd, &options) == 0 else {
            close()
            throw SerialPortError.configFailed("Could not read port settings")
        }
        cfmakeraw(&options)
        cfsetispeed(&options, speed_t(B115200))
        cfsetospeed(&options, speed_t(B115200))
        // 8N1, no flow control, whatever the last program left behind
        // No HUPCL: closing the port must not toggle the lines that reset the chip
        options.c_cflag &= ~UInt(PARENB | CSTOPB | CSIZE | CRTSCTS | HUPCL)
        options.c_cflag |= UInt(CS8 | CLOCAL | CREAD)
        // VMIN=0, VTIME=1: read() returns after at most 0.1 s
        options.c_cc.16 = 0
        options.c_cc.17 = 1

        tcflush(fd, TCIOFLUSH)
        guard tcsetattr(fd, TCSANOW, &options) == 0 else {
            close()
            throw SerialPortError.configFailed("Could not apply port settings")
        }
    }

    func close() {
        if fd != -1 {
            Darwin.close(fd)
            fd = -1
        }
    }

    func setBaud(_ baud: Int) throws {
        guard fd != -1 else { throw SerialPortError.configFailed("Port not open") }
        tcdrain(fd)
        var speed = UInt(baud)
        if ioctl(fd, SerialPort.IOSSIOSPEED, &speed) == -1 && errno != ENOTTY {
            throw SerialPortError.configFailed("Could not set \(baud) baud: \(String(cString: strerror(errno)))")
        }
        tcflush(fd, TCIOFLUSH)
    }

    func write(_ data: Data) throws {
        guard fd != -1 else { throw SerialPortError.writeFailed("Port not open") }
        try data.withUnsafeBytes { (ptr: UnsafeRawBufferPointer) in
            var offset = 0
            while offset < data.count {
                let written = Darwin.write(fd, ptr.baseAddress!.advanced(by: offset), data.count - offset)
                if written < 0 {
                    if errno == EINTR || errno == EAGAIN { usleep(1000); continue }
                    throw SerialPortError.writeFailed("Write failed: \(String(cString: strerror(errno)))")
                }
                offset += written
            }
        }
        tcdrain(fd)
    }

    /// Whatever has arrived, waiting at most `timeout` for the first byte. Empty when nothing came.
    func readAvailable(maxBytes: Int = 4096, timeout: TimeInterval = 0.1) throws -> Data {
        guard fd != -1 else { throw SerialPortError.readFailed("Port not open") }

        var buffer = Data(count: maxBytes)
        let deadline = Date().addingTimeInterval(timeout)
        repeat {
            let count = buffer.withUnsafeMutableBytes { Darwin.read(fd, $0.baseAddress!, maxBytes) }
            if count > 0 { return buffer.prefix(count) }
            if count < 0 && errno != EAGAIN && errno != EINTR {
                // Unplugged mid-transfer ends up here
                throw SerialPortError.readFailed("Read failed: \(String(cString: strerror(errno)))")
            }
        } while Date() < deadline
        return Data()
    }

    func flush() {
        if fd != -1 { tcflush(fd, TCIOFLUSH) }
    }

    func setDTR(_ enabled: Bool) throws { try setModemLine(TIOCM_DTR, enabled) }
    func setRTS(_ enabled: Bool) throws { try setModemLine(TIOCM_RTS, enabled) }

    private func setModemLine(_ line: Int32, _ enabled: Bool) throws {
        guard fd != -1 else { return }
        var status: Int32 = 0
        if ioctl(fd, TIOCMGET, &status) == -1 {
            if errno == ENOTTY { return }  // Pseudo-terminal in the tests: no modem lines
            throw SerialPortError.configFailed("Could not read modem lines")
        }
        if enabled { status |= line } else { status &= ~line }
        if ioctl(fd, TIOCMSET, &status) == -1 && errno != ENOTTY {
            throw SerialPortError.configFailed("Could not set modem lines")
        }
    }

    deinit {
        close()
    }
}

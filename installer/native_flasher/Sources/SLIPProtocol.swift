import Foundation

/// SLIP framing used by the ESP32 serial bootloader.
enum SLIPProtocol {
    static let END: UInt8 = 0xC0
    static let ESC: UInt8 = 0xDB
    static let ESC_END: UInt8 = 0xDC
    static let ESC_ESC: UInt8 = 0xDD

    static func encode(_ data: Data) -> Data {
        var encoded = Data(capacity: data.count + 8)
        encoded.append(END)
        for byte in data {
            switch byte {
            case END: encoded.append(contentsOf: [ESC, ESC_END])
            case ESC: encoded.append(contentsOf: [ESC, ESC_ESC])
            default: encoded.append(byte)
            }
        }
        encoded.append(END)
        return encoded
    }

    /// Takes the first complete frame out of `data`. Bytes outside a frame (boot log text)
    /// are skipped, and a frame with a broken escape is dropped so it can't stall the reader.
    /// Returns the frame (nil if none is complete yet) and the bytes still to be read.
    static func extractPacket(_ data: Data) -> (Data?, Data) {
        var inPacket = false
        var escape = false
        var packet = Data()

        for (i, byte) in data.enumerated() {
            if escape {
                escape = false
                switch byte {
                case ESC_END: packet.append(END)
                case ESC_ESC: packet.append(ESC)
                default:
                    inPacket = false
                    packet = Data()
                }
                continue
            }
            switch byte {
            case END:
                if inPacket && !packet.isEmpty {
                    return (packet, Data(data.dropFirst(i + 1)))
                }
                inPacket = true
                packet = Data()
            case ESC:
                if inPacket { escape = true }
            default:
                if inPacket { packet.append(byte) }
            }
        }
        return (nil, data)
    }
}

import Foundation

/// SLIP Protocol implementation for ESP32 bootloader communication
/// SLIP (Serial Line Internet Protocol) is used to frame packets over serial
class SLIPProtocol {
    // SLIP special bytes
    static let END: UInt8 = 0xC0
    static let ESC: UInt8 = 0xDB
    static let ESC_END: UInt8 = 0xDC
    static let ESC_ESC: UInt8 = 0xDD

    /// Encode data with SLIP framing
    static func encode(_ data: Data) -> Data {
        var encoded = Data()
        encoded.append(END)  // Start with END byte

        for byte in data {
            switch byte {
            case END:
                encoded.append(ESC)
                encoded.append(ESC_END)
            case ESC:
                encoded.append(ESC)
                encoded.append(ESC_ESC)
            default:
                encoded.append(byte)
            }
        }

        encoded.append(END)  // End with END byte
        return encoded
    }

    /// Decode SLIP framed data
    static func decode(_ data: Data) -> Data? {
        var decoded = Data()
        var escape = false

        for byte in data {
            if escape {
                switch byte {
                case ESC_END:
                    decoded.append(END)
                case ESC_ESC:
                    decoded.append(ESC)
                default:
                    // Invalid escape sequence
                    return nil
                }
                escape = false
            } else {
                switch byte {
                case END:
                    // Skip END bytes (packet delimiters)
                    continue
                case ESC:
                    escape = true
                default:
                    decoded.append(byte)
                }
            }
        }

        return decoded
    }

    /// Extract first complete SLIP packet from data
    /// Returns (packet, remaining_data)
    static func extractPacket(_ data: Data) -> (Data?, Data) {
        var inPacket = false
        var packet = Data()
        var escape = false

        for (i, byte) in data.enumerated() {

            if escape {
                switch byte {
                case ESC_END:
                    packet.append(END)
                case ESC_ESC:
                    packet.append(ESC)
                default:
                    // Invalid escape
                    return (nil, data)
                }
                escape = false
            } else {
                switch byte {
                case END:
                    if inPacket && !packet.isEmpty {
                        // Complete packet found
                        let remaining = data.suffix(from: data.index(data.startIndex, offsetBy: i + 1))
                        return (packet, remaining)
                    }
                    // Start of new packet
                    inPacket = true
                    packet = Data()
                case ESC:
                    escape = true
                default:
                    if inPacket {
                        packet.append(byte)
                    }
                }
            }
        }

        // No complete packet found
        return (nil, data)
    }
}

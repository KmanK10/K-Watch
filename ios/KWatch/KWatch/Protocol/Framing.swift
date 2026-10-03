import Foundation

/// Chunk framing from docs/companion-protocol.md.
///
/// Each BLE write or notification is one chunk:
/// byte 0 is flags (bit 0 START, bit 1 END), and the rest is the next piece of a UTF-8 JSON message.
/// A message that fits in one chunk has flags `0x03`. A new START discards an unfinished message.
struct ChunkFramer {
    static let start: UInt8 = 0x01
    static let end: UInt8 = 0x02
    static let maxMessageBytes = 8 * 1024

    private var buffer = Data()
    private var started = false

    mutating func reset() {
        buffer.removeAll(keepingCapacity: true)
        started = false
    }

    /// Feeds one notification or write. Returns a complete message when an END chunk arrives.
    mutating func append(_ chunk: Data) -> Data? {
        guard let flags = chunk.first else { return nil }
        let payload = chunk.dropFirst()

        if flags & Self.start != 0 {
            buffer.removeAll(keepingCapacity: true)
            started = true
        }
        guard started else { return nil }

        if buffer.count + payload.count > Self.maxMessageBytes {
            reset()
            return nil
        }
        buffer.append(payload)

        guard flags & Self.end != 0 else { return nil }
        let message = buffer
        reset()
        return message
    }

    /// Splits a message so each chunk is at most `maxWriteLength` bytes, including the flags byte.
    /// `maxWriteLength` is `peripheral.maximumWriteValueLength(for: .withResponse)`.
    static func chunks(for message: Data, maxWriteLength: Int) -> [Data] {
        guard !message.isEmpty else { return [] }
        let room = max(maxWriteLength, 2) - 1
        var offset = 0
        var chunks: [Data] = []
        while offset < message.count {
            let length = min(room, message.count - offset)
            let isStart = offset == 0
            let isEnd = offset + length == message.count
            var flags: UInt8 = 0
            if isStart { flags |= start }
            if isEnd { flags |= end }
            var chunk = Data(capacity: length + 1)
            chunk.append(flags)
            chunk.append(message[offset ..< offset + length])
            chunks.append(chunk)
            offset += length
        }
        return chunks
    }
}

enum CompanionError: Error, LocalizedError {
    case notReady
    case messageTooLarge(Int)
    case timedOut
    case disconnected
    case writeFailed(String)
    case rejected(String)

    var errorDescription: String? {
        switch self {
        case .notReady:
            return "The watch is not connected."
        case .messageTooLarge(let bytes):
            return "Message is \(bytes) bytes; the limit is \(ChunkFramer.maxMessageBytes)."
        case .timedOut:
            return "The watch did not answer."
        case .disconnected:
            return "The watch disconnected."
        case .writeFailed(let reason):
            return reason
        case .rejected(let reason):
            return reason
        }
    }
}

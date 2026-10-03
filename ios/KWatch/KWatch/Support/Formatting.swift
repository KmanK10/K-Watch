import Foundation

struct LogLine: Identifiable, Equatable {
    enum Kind: String {
        case out = "→"
        case inn = "←"
        case info = "·"
    }

    let id: UUID
    let date: Date
    let kind: Kind
    let text: String

    init(kind: Kind, text: String, date: Date = Date()) {
        id = UUID()
        self.date = date
        self.kind = kind
        self.text = text
    }
}

enum Format {
    static func steps(_ value: Int) -> String {
        stepsFormatter.string(from: NSNumber(value: value)) ?? "\(value)"
    }

    static func updated(_ unix: Int, now: Date = Date()) -> String {
        let age = Int(now.timeIntervalSince1970) - unix
        if age < 60 { return "Updated just now" }
        if age < 3600 { return "Updated \(max(age, 0) / 60) min ago" }
        if age < 86_400 { return "Updated \(age / 3600) h ago" }
        return "Updated \(age / 86_400) d ago"
    }

    static func uptime(_ seconds: Int) -> String {
        let seconds = max(0, seconds)
        let days = seconds / 86_400
        let hours = (seconds % 86_400) / 3600
        let minutes = (seconds % 3600) / 60
        if days > 0 { return "\(days)d \(hours)h \(minutes)m" }
        if hours > 0 { return "\(hours)h \(minutes)m" }
        return "\(minutes)m \(seconds % 60)s"
    }

    static func bytes(_ count: Int) -> String {
        byteFormatter.string(fromByteCount: Int64(max(0, count)))
    }

    static func voltage(_ millivolts: Int) -> String {
        guard millivolts > 0 else { return "—" }
        return String(format: "%.2f V", Double(millivolts) / 1000)
    }

    static func clock(_ unix: Int, twentyFourHour: Bool) -> String {
        let formatter = DateFormatter()
        formatter.locale = .current
        formatter.dateFormat = twentyFourHour ? "HH:mm" : "h:mm a"
        return formatter.string(from: Date(timeIntervalSince1970: TimeInterval(unix)))
    }

    static func hour(_ unix: Int, twentyFourHour: Bool) -> String {
        let formatter = DateFormatter()
        formatter.locale = .current
        formatter.dateFormat = twentyFourHour ? "HH" : "h a"
        return formatter.string(from: Date(timeIntervalSince1970: TimeInterval(unix)))
    }

    static func weekday(_ unix: Int, todayStart: Int?) -> String {
        let date = Date(timeIntervalSince1970: TimeInterval(unix))
        if let todayStart {
            let today = Date(timeIntervalSince1970: TimeInterval(todayStart))
            if Calendar.current.isDate(date, inSameDayAs: today) {
                return "Today"
            }
        }
        let formatter = DateFormatter()
        formatter.locale = .current
        formatter.setLocalizedDateFormatFromTemplate("EEE")
        return formatter.string(from: date)
    }

    static func dayStamp(_ unix: Int) -> String {
        let formatter = DateFormatter()
        formatter.locale = .current
        formatter.dateStyle = .medium
        formatter.timeStyle = .short
        return formatter.string(from: Date(timeIntervalSince1970: TimeInterval(unix)))
    }

    static func restart(_ reason: String) -> String {
        switch reason {
        case "power-on": return "Power on"
        case "software": return "Software reset"
        case "crash": return "Crash"
        case "froze": return "Watchdog"
        case "brownout": return "Brownout"
        case "deep-sleep": return "Deep sleep"
        case "other": return "Other"
        default: return reason
        }
    }

    static func aqi(_ value: Int) -> String {
        let band: String
        switch value {
        case ..<51: band = "Good"
        case 51 ... 100: band = "Moderate"
        case 101 ... 150: band = "Sensitive"
        case 151 ... 200: band = "Unhealthy"
        case 201 ... 300: band = "Very unhealthy"
        default: band = "Hazardous"
        }
        return "\(value) \(band)"
    }

    static func uv(_ value: Double) -> String {
        let band: String
        switch value {
        case ..<3: band = "Low"
        case ..<6: band = "Moderate"
        case ..<8: band = "High"
        case ..<11: band = "Very high"
        default: band = "Extreme"
        }
        let number = value.rounded() == value ? String(Int(value)) : String(format: "%.1f", value)
        return "\(number) \(band)"
    }

    private static let stepsFormatter: NumberFormatter = {
        let formatter = NumberFormatter()
        formatter.numberStyle = .decimal
        return formatter
    }()

    private static let byteFormatter: ByteCountFormatter = {
        let formatter = ByteCountFormatter()
        formatter.countStyle = .memory
        return formatter
    }()
}

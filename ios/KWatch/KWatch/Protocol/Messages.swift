import Foundation

/// GATT UUIDs from docs/companion-protocol.md. RX is app to watch, TX is watch to app.
enum CompanionUUID {
    static let service = "58250B93-E11B-42C4-8104-7DDC9BD61EB2"
    static let rx = "58250B94-E11B-42C4-8104-7DDC9BD61EB2"
    static let tx = "58250B95-E11B-42C4-8104-7DDC9BD61EB2"
}

enum CompanionProtocol {
    static let version = 1
    static let appVersion = "1.0"

    /// Seconds east of UTC. The watch stores Unix time and cannot line it up with the clock without this.
    static var utcOffset: Int { TimeZone.current.secondsFromGMT() }
}

/// Names reserved in the protocol. The watch does not implement these yet.
/// To add one: give it a Codable payload, a case in `IncomingMessage`, and a sender next to the others.
enum PlannedMessage {
    static let batteryHistory = "battery.history"
    static let alarmsGet = "alarms.get"
    static let alarmsSet = "alarms.set"
    static let logGet = "log.get"

    static let all: Set<String> = [batteryHistory, alarmsGet, alarmsSet, logGet]

    struct BatteryHistoryRequest: Encodable {
        var t = PlannedMessage.batteryHistory
        var id: Int
    }

    struct AlarmsGetRequest: Encodable {
        var t = PlannedMessage.alarmsGet
        var id: Int
    }

    /// The alarm list goes here once the firmware accepts `alarms.set`.
    struct AlarmsSetRequest: Encodable {
        var t = PlannedMessage.alarmsSet
        var id: Int
    }

    struct LogGetRequest: Encodable {
        var t = PlannedMessage.logGet
        var id: Int
    }
}

struct SimpleRequest: Encodable {
    var t: String
    var id: Int
}

struct HelloMessage: Encodable {
    var t = "hello"
    var id: Int
    var app: String
    var protocolVersion: Int
    var utcOffset: Int

    enum CodingKeys: String, CodingKey {
        case t, id, app
        case protocolVersion = "protocol"
        case utcOffset = "utc_offset"
    }
}

struct HelloInfo: Equatable {
    var protocolVersion: Int
    var name: String
    var device: String
    var firmware: String
    var features: [String]
}

struct WeatherSetMessage: Encodable {
    var t = "weather.set"
    var id: Int
    var utcOffset: Int
    var weather: WeatherPayload

    enum CodingKeys: String, CodingKey {
        case t, id, weather
        case utcOffset = "utc_offset"
    }
}

struct WeatherPayload: Equatable {
    var updated: Int
    var location: String
    var lat: Double?
    var lon: Double?
    /// `"F"` or `"C"`. Temperatures in this forecast are already in this unit.
    var unit: String
    /// `"mph"` or `"kmh"`. Wind in this forecast is already in this unit.
    var windUnit: String
    var now: Now
    var today: Today
    var hours: [Hour]
    var days: [Day]

    struct Now: Codable, Equatable {
        var temp: Int
        var feels: Int
        var code: Int
        var day: Bool
        var humidity: Int
        var wind: Int
        var uv: Double?
        var aqi: Int?
    }

    struct Today: Codable, Equatable {
        var high: Int?
        var low: Int?
        var precip: Int?
        var sunrise: Int?
        var sunset: Int?
    }

    struct Hour: Codable, Equatable, Identifiable {
        var time: Int
        var temp: Int
        var code: Int
        var precip: Int?
        var id: Int { time }
    }

    struct Day: Codable, Equatable, Identifiable {
        var time: Int
        var high: Int
        var low: Int
        var code: Int
        var precip: Int?
        var id: Int { time }
    }

    /// The watch asks again when the forecast is older than 30 minutes.
    var isStale: Bool {
        Date().timeIntervalSince1970 - Double(updated) > 30 * 60
    }
}

extension WeatherPayload: Codable {
    enum CodingKeys: String, CodingKey {
        case updated, location, lat, lon, unit, now, today, hours, days
        case windUnit = "wind_unit"
    }

    init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        updated = try container.decode(Int.self, forKey: .updated)
        location = try container.decode(String.self, forKey: .location)
        lat = try container.decodeIfPresent(Double.self, forKey: .lat)
        lon = try container.decodeIfPresent(Double.self, forKey: .lon)
        unit = try container.decode(String.self, forKey: .unit)
        // Forecasts saved before wind had its own unit followed the temperature unit.
        windUnit = try container.decodeIfPresent(String.self, forKey: .windUnit)
            ?? (unit == "C" ? "kmh" : "mph")
        now = try container.decode(Now.self, forKey: .now)
        today = try container.decode(Today.self, forKey: .today)
        hours = try container.decode([Hour].self, forKey: .hours)
        days = try container.decode([Day].self, forKey: .days)
    }

    func encode(to encoder: Encoder) throws {
        var container = encoder.container(keyedBy: CodingKeys.self)
        try container.encode(updated, forKey: .updated)
        try container.encode(location, forKey: .location)
        try container.encodeIfPresent(lat, forKey: .lat)
        try container.encodeIfPresent(lon, forKey: .lon)
        try container.encode(unit, forKey: .unit)
        try container.encode(windUnit, forKey: .windUnit)
        try container.encode(now, forKey: .now)
        try container.encode(today, forKey: .today)
        try container.encode(hours, forKey: .hours)
        try container.encode(days, forKey: .days)
    }
}

struct SettingsMessage: Encodable {
    var t: String
    var id: Int
    var settings: SettingsPatch
}

/// A partial `settings.set`. Nil fields are left out, so the watch keeps its current value.
/// `bluetooth` is intentionally absent: the watch ignores attempts to turn it off from here.
struct SettingsPatch: Encodable, Equatable {
    var brightness: Int?
    var screenTimeout: Int?
    var raiseToWake: Bool?
    var tapToWake: Bool?
    var notifyVibrate: Bool?
    var touchFeedback: Bool?
    var clock24h: Bool?
    var dnd: Bool?
    var stepGoal: Int?
    var temperature: TemperatureUnit?
    var wind: WindUnit?

    enum CodingKeys: String, CodingKey {
        case brightness
        case screenTimeout = "screen_timeout"
        case raiseToWake = "raise_to_wake"
        case tapToWake = "tap_to_wake"
        case notifyVibrate = "notify_vibrate"
        case touchFeedback = "touch_feedback"
        case clock24h = "clock_24h"
        case dnd
        case stepGoal = "step_goal"
        case temperature = "temp_unit"
        case wind = "wind_unit"
    }

    var isEmpty: Bool {
        brightness == nil && screenTimeout == nil && raiseToWake == nil && tapToWake == nil
            && notifyVibrate == nil && touchFeedback == nil && clock24h == nil && dnd == nil
            && stepGoal == nil && temperature == nil && wind == nil
    }
}

struct WatchSettings: Equatable {
    var brightness: Int
    var screenTimeout: Int
    var raiseToWake: Bool
    var tapToWake: Bool
    var notifyVibrate: Bool
    var touchFeedback: Bool
    var clock24h: Bool
    var dnd: Bool
    var bluetooth: Bool
    var stepGoal: Int
    /// Nil when this watch build does not report the field yet.
    var temperature: TemperatureUnit?
    var wind: WindUnit?

    static let timeouts = [5, 10, 15, 30]
    static let brightnessRange = 10 ... 100
    static let goalRange = 1000 ... 50000
}

struct HealthSnapshot: Equatable {
    var date: String
    var today: Int
    var goal: Int
    /// `days[0]` is yesterday. `nil` means the watch has no count for that day.
    var days: [Int?]

    /// Average of the last `window` finished days, skipping days with no count.
    /// Today is not included, which is how the watch computes its own averages.
    func average(finishedDays window: Int) -> Int? {
        let known = days.prefix(window).compactMap { $0 }
        guard !known.isEmpty else { return nil }
        let sum = known.reduce(Int64(0)) { $0 + Int64($1) }
        return Int(sum / Int64(known.count))
    }
}

struct BatteryStatus: Equatable {
    var percent: Int
    var voltageMillivolts: Int
    var charging: Bool
    var usb: Bool
}

struct Diagnostics: Equatable {
    var firmware: String
    var uptime: Int
    var time: Int
    var restartReason: String
    var heapFree: Int
    var heapMin: Int
    var psramFree: Int
    var battery: BatteryStatus
    var notifications: Int
}

enum IncomingMessage {
    case hello(HelloInfo, replyID: Int)
    case weatherAck(replyID: Int)
    case weatherRequest(reason: String?)
    case settings(WatchSettings, replyID: Int?)
    case health(HealthSnapshot, replyID: Int)
    case diagnostics(Diagnostics, replyID: Int)
    case findAck(replyID: Int)
    case failed(type: String, replyID: Int?, message: String)
    case planned(type: String, replyID: Int?)
    case unrecognized(type: String, replyID: Int?)

    var replyID: Int? {
        switch self {
        case .hello(_, let id): return id
        case .weatherAck(let id): return id
        case .weatherRequest: return nil
        case .settings(_, let id): return id
        case .health(_, let id): return id
        case .diagnostics(_, let id): return id
        case .findAck(let id): return id
        case .failed(_, let id, _): return id
        case .planned(_, let id): return id
        case .unrecognized(_, let id): return id
        }
    }
}

enum TemperatureUnit: String, Codable, CaseIterable, Identifiable {
    case fahrenheit = "F"
    case celsius = "C"

    var id: String { rawValue }
    var apiName: String { self == .fahrenheit ? "fahrenheit" : "celsius" }
    var label: String { self == .fahrenheit ? "°F" : "°C" }
}

enum WindUnit: String, Codable, CaseIterable, Identifiable {
    case mph
    case kmh

    var id: String { rawValue }
    var apiName: String { rawValue }
    var label: String { self == .mph ? "mph" : "km/h" }
}

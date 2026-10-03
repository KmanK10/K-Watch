import Foundation

enum MessageCodec {
    private static let encoder: JSONEncoder = {
        let encoder = JSONEncoder()
        encoder.outputFormatting = [.sortedKeys, .withoutEscapingSlashes]
        return encoder
    }()

    static func encode<T: Encodable>(_ value: T) throws -> Data {
        let data = try encoder.encode(value)
        guard data.count <= ChunkFramer.maxMessageBytes else {
            throw CompanionError.messageTooLarge(data.count)
        }
        return data
    }

    static func decode(_ data: Data) throws -> IncomingMessage {
        let object = try JSONSerialization.jsonObject(with: data) as? [String: Any]
        guard let object, let type = object["t"] as? String else {
            throw DecodingError.dataCorrupted(.init(codingPath: [], debugDescription: "Message has no type"))
        }
        let replyID = intValue(object["re"])
        if let ok = object["ok"] as? Bool, !ok {
            let message = object["error"] as? String ?? "The watch rejected the request."
            return .failed(type: type, replyID: replyID, message: message)
        }
        if PlannedMessage.all.contains(type) {
            return .planned(type: type, replyID: replyID)
        }

        switch type {
        case "hello":
            guard let replyID else { return .unrecognized(type: type, replyID: nil) }
            return .hello(helloInfo(object), replyID: replyID)
        case "weather.set":
            guard let replyID else { return .unrecognized(type: type, replyID: nil) }
            return .weatherAck(replyID: replyID)
        case "weather.request":
            return .weatherRequest(reason: object["reason"] as? String)
        case "settings.get", "settings.set", "settings.changed":
            guard let settings = settings(object["settings"] as? [String: Any]) else {
                return .failed(type: type, replyID: replyID, message: "The watch sent settings the app could not read.")
            }
            return .settings(settings, replyID: replyID)
        case "health.get":
            guard let replyID, let health = health(object) else {
                return .failed(type: type, replyID: replyID, message: "The watch sent a health report the app could not read.")
            }
            return .health(health, replyID: replyID)
        case "diag.get":
            guard let replyID, let diagnostics = diagnostics(object) else {
                return .failed(type: type, replyID: replyID, message: "The watch sent a diagnostic report the app could not read.")
            }
            return .diagnostics(diagnostics, replyID: replyID)
        case "find":
            guard let replyID else { return .unrecognized(type: type, replyID: nil) }
            return .findAck(replyID: replyID)
        default:
            return .unrecognized(type: type, replyID: replyID)
        }
    }

    private static func helloInfo(_ object: [String: Any]) -> HelloInfo {
        HelloInfo(
            protocolVersion: intValue(object["protocol"]) ?? 0,
            name: object["name"] as? String ?? "K-Watch",
            device: object["device"] as? String ?? "",
            firmware: object["firmware"] as? String ?? "",
            features: object["features"] as? [String] ?? []
        )
    }

    private static func settings(_ object: [String: Any]?) -> WatchSettings? {
        guard let object else { return nil }
        guard
            let brightness = intValue(object["brightness"]),
            let screenTimeout = intValue(object["screen_timeout"]),
            let raiseToWake = boolValue(object["raise_to_wake"]),
            let tapToWake = boolValue(object["tap_to_wake"]),
            let notifyVibrate = boolValue(object["notify_vibrate"]),
            let touchFeedback = boolValue(object["touch_feedback"]),
            let clock24h = boolValue(object["clock_24h"]),
            let dnd = boolValue(object["dnd"]),
            let bluetooth = boolValue(object["bluetooth"]),
            let stepGoal = intValue(object["step_goal"])
        else { return nil }
        return WatchSettings(
            brightness: brightness,
            screenTimeout: screenTimeout,
            raiseToWake: raiseToWake,
            tapToWake: tapToWake,
            notifyVibrate: notifyVibrate,
            touchFeedback: touchFeedback,
            clock24h: clock24h,
            dnd: dnd,
            bluetooth: bluetooth,
            stepGoal: stepGoal,
            temperature: (object["temp_unit"] as? String).flatMap(TemperatureUnit.init(rawValue:)),
            wind: (object["wind_unit"] as? String).flatMap(WindUnit.init(rawValue:))
        )
    }

    private static func health(_ object: [String: Any]) -> HealthSnapshot? {
        guard
            let date = object["date"] as? String,
            let today = intValue(object["today"]),
            let goal = intValue(object["goal"]),
            let rawDays = object["days"] as? [Any]
        else { return nil }
        return HealthSnapshot(date: date, today: today, goal: goal, days: rawDays.map(intValue))
    }

    private static func diagnostics(_ object: [String: Any]) -> Diagnostics? {
        guard
            let firmware = object["firmware"] as? String,
            let uptime = intValue(object["uptime"]),
            let time = intValue(object["time"]),
            let restart = object["restart_reason"] as? String,
            let heapFree = intValue(object["heap_free"]),
            let heapMin = intValue(object["heap_min"]),
            let psram = intValue(object["psram_free"]),
            let notifications = intValue(object["notifications"]),
            let batteryObject = object["battery"] as? [String: Any],
            let percent = intValue(batteryObject["percent"]),
            let voltage = intValue(batteryObject["voltage"]),
            let charging = boolValue(batteryObject["charging"]),
            let usb = boolValue(batteryObject["usb"])
        else { return nil }
        return Diagnostics(
            firmware: firmware,
            uptime: uptime,
            time: time,
            restartReason: restart,
            heapFree: heapFree,
            heapMin: heapMin,
            psramFree: psram,
            battery: BatteryStatus(percent: percent, voltageMillivolts: voltage, charging: charging, usb: usb),
            notifications: notifications
        )
    }

    /// cJSON sometimes prints whole numbers as integers and sometimes as `1.0`.
    private static func intValue(_ value: Any?) -> Int? {
        switch value {
        case let int as Int:
            return int
        case let int as Int64:
            return Int(int)
        case let double as Double:
            return Int(double.rounded())
        case let number as NSNumber:
            return Int(number.doubleValue.rounded())
        default:
            return nil
        }
    }

    private static func boolValue(_ value: Any?) -> Bool? {
        switch value {
        case let bool as Bool:
            return bool
        case let number as NSNumber:
            return number.boolValue
        default:
            return nil
        }
    }
}

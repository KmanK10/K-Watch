import XCTest
@testable import KWatch

final class FramingTests: XCTestCase {
    func testSingleChunkUsesBothFlags() {
        let message = Data("{}".utf8)
        let chunks = ChunkFramer.chunks(for: message, maxWriteLength: 512)
        XCTAssertEqual(chunks.count, 1)
        XCTAssertEqual(chunks[0].first, 0x03)
        XCTAssertEqual(chunks[0].dropFirst(), message)

        var framer = ChunkFramer()
        XCTAssertEqual(framer.append(chunks[0]), message)
    }

    func testSplitAndReassembly() {
        let message = Data(repeating: 0x41, count: 50)
        let chunks = ChunkFramer.chunks(for: message, maxWriteLength: 10)
        XCTAssertEqual(chunks.count, 6)
        XCTAssertEqual(chunks.first?.first, ChunkFramer.start)
        XCTAssertEqual(chunks.last?.first, ChunkFramer.end)
        XCTAssertTrue(chunks.dropFirst().dropLast().allSatisfy { $0.first == 0 })
        XCTAssertTrue(chunks.allSatisfy { $0.count <= 10 })

        var framer = ChunkFramer()
        var done: Data?
        for chunk in chunks {
            done = framer.append(chunk) ?? done
        }
        XCTAssertEqual(done, message)
    }

    func testTwoChunksMarkStartThenEnd() {
        let message = Data("abcde".utf8)
        let chunks = ChunkFramer.chunks(for: message, maxWriteLength: 5)
        XCTAssertEqual(chunks.count, 2)
        XCTAssertEqual(chunks[0].first, 0x01)
        XCTAssertEqual(chunks[1].first, 0x02)
    }

    func testNewStartDiscardsAPartialMessage() {
        var framer = ChunkFramer()
        var partial = Data([ChunkFramer.start])
        partial.append(Data("nope".utf8))
        XCTAssertNil(framer.append(partial))

        let good = ChunkFramer.chunks(for: Data("hi".utf8), maxWriteLength: 20)[0]
        XCTAssertEqual(framer.append(good), Data("hi".utf8))
    }

    func testBytesBeforeStartAreIgnored() {
        var framer = ChunkFramer()
        var stray = Data([0x00])
        stray.append(Data("xx".utf8))
        XCTAssertNil(framer.append(stray))
        let good = ChunkFramer.chunks(for: Data("ok".utf8), maxWriteLength: 20)[0]
        XCTAssertEqual(framer.append(good), Data("ok".utf8))
    }

    func testOversizeChunkResetsTheFramer() {
        var framer = ChunkFramer()
        var chunk = Data([ChunkFramer.start | ChunkFramer.end])
        chunk.append(Data(repeating: 0x61, count: ChunkFramer.maxMessageBytes + 1))
        XCTAssertNil(framer.append(chunk))

        let good = ChunkFramer.chunks(for: Data("hi".utf8), maxWriteLength: 20)[0]
        XCTAssertEqual(framer.append(good), Data("hi".utf8))
    }

    func testEmptyMessageProducesNoChunks() {
        XCTAssertTrue(ChunkFramer.chunks(for: Data(), maxWriteLength: 20).isEmpty)
    }
}

final class MessageTests: XCTestCase {
    func testHelloCarriesProtocolAndUTCOffset() throws {
        let data = try MessageCodec.encode(HelloMessage(
            id: 1,
            app: "1.0",
            protocolVersion: 1,
            utcOffset: -25200
        ))
        let object = try json(data)
        XCTAssertEqual(object["t"] as? String, "hello")
        XCTAssertEqual(int(object["id"]), 1)
        XCTAssertEqual(object["app"] as? String, "1.0")
        XCTAssertEqual(int(object["protocol"]), 1)
        XCTAssertEqual(int(object["utc_offset"]), -25200)
    }

    func testWeatherSetPutsUTCOffsetBesideTheForecast() throws {
        let payload = try sampleForecast()
        let data = try MessageCodec.encode(WeatherSetMessage(id: 2, utcOffset: -25200, weather: payload))
        let object = try json(data)
        XCTAssertEqual(object["t"] as? String, "weather.set")
        XCTAssertEqual(int(object["utc_offset"]), -25200)
        let weather = object["weather"] as? [String: Any]
        XCTAssertNil(weather?["utc_offset"])
        XCTAssertEqual(weather?["unit"] as? String, "F")
        XCTAssertEqual(weather?["wind_unit"] as? String, "mph")
        let latitude = (weather?["lat"] as? NSNumber)?.doubleValue
        XCTAssertEqual(latitude ?? 0, 47.61, accuracy: 0.0001)
    }

    func testSettingsPatchOmitsUntouchedFieldsAndBluetooth() throws {
        let data = try MessageCodec.encode(SettingsMessage(
            t: "settings.set",
            id: 4,
            settings: SettingsPatch(brightness: 80, dnd: true)
        ))
        let settings = try json(data)["settings"] as? [String: Any]
        XCTAssertEqual(int(settings?["brightness"]), 80)
        XCTAssertEqual(settings?["dnd"] as? Bool, true)
        XCTAssertNil(settings?["bluetooth"])
        XCTAssertNil(settings?["raise_to_wake"])
        XCTAssertNil(settings?["step_goal"])
        XCTAssertNil(settings?["temp_unit"])
        XCTAssertNil(settings?["wind_unit"])
    }

    func testUnitSettingsAreIndependentOnTheWire() throws {
        let data = try MessageCodec.encode(SettingsMessage(
            t: "settings.set",
            id: 9,
            settings: SettingsPatch(temperature: .celsius, wind: .mph)
        ))
        let settings = try json(data)["settings"] as? [String: Any]
        XCTAssertEqual(settings?["temp_unit"] as? String, "C")
        XCTAssertEqual(settings?["wind_unit"] as? String, "mph")
        XCTAssertNil(settings?["brightness"])

        let changed = try MessageCodec.decode(Data("""
        {"t":"settings.changed","settings":{"brightness":60,"screen_timeout":5,"raise_to_wake":true,"tap_to_wake":true,"notify_vibrate":true,"touch_feedback":true,"clock_24h":false,"dnd":false,"bluetooth":true,"step_goal":8000,"temp_unit":"C","wind_unit":"kmh"}}
        """.utf8))
        guard case .settings(let watch, nil) = changed else { return XCTFail("\(changed)") }
        XCTAssertEqual(watch.temperature, .celsius)
        XCTAssertEqual(watch.wind, .kmh)
    }

    func testDecodeSpecReplies() throws {
        let hello = try MessageCodec.decode(Data("""
        {"t":"hello","re":1,"ok":true,"protocol":1,"name":"K-Watch","device":"LilyGo T-Watch S3","firmware":"v0.4-12-gabcdef","features":["weather","settings","health","diagnostics","find"]}
        """.utf8))
        guard case .hello(let info, let id) = hello else { return XCTFail("\(hello)") }
        XCTAssertEqual(id, 1)
        XCTAssertEqual(info.firmware, "v0.4-12-gabcdef")
        XCTAssertEqual(info.features, ["weather", "settings", "health", "diagnostics", "find"])

        let settings = try MessageCodec.decode(Data("""
        {"t":"settings.get","re":3,"ok":true,"settings":{"brightness":60,"screen_timeout":5,"raise_to_wake":true,"tap_to_wake":true,"notify_vibrate":true,"touch_feedback":true,"clock_24h":false,"dnd":false,"bluetooth":true,"step_goal":8000}}
        """.utf8))
        guard case .settings(let watch, let settingsID) = settings else { return XCTFail("\(settings)") }
        XCTAssertEqual(settingsID, 3)
        XCTAssertEqual(watch.brightness, 60)
        XCTAssertEqual(watch.stepGoal, 8000)
        XCTAssertFalse(watch.clock24h)
        XCTAssertTrue(watch.bluetooth)
        XCTAssertNil(watch.temperature)
        XCTAssertNil(watch.wind)

        let changed = try MessageCodec.decode(Data("""
        {"t":"settings.changed","settings":{"brightness":80,"screen_timeout":10,"raise_to_wake":true,"tap_to_wake":false,"notify_vibrate":true,"touch_feedback":true,"clock_24h":true,"dnd":true,"bluetooth":true,"step_goal":9000}}
        """.utf8))
        guard case .settings(let updated, nil) = changed else { return XCTFail("\(changed)") }
        XCTAssertEqual(updated.brightness, 80)
        XCTAssertTrue(updated.dnd)

        let health = try MessageCodec.decode(Data("""
        {"t":"health.get","re":5,"ok":true,"date":"2026-10-03","today":4321,"goal":8000,"days":[9120,6480,null,7350]}
        """.utf8))
        guard case .health(let snapshot, 5) = health else { return XCTFail("\(health)") }
        XCTAssertEqual(snapshot.today, 4321)
        XCTAssertEqual(snapshot.days, [9120, 6480, nil, 7350])
        XCTAssertEqual(snapshot.average(finishedDays: 7), 7650)
        XCTAssertEqual(snapshot.average(finishedDays: 30), 7650)

        let diag = try MessageCodec.decode(Data("""
        {"t":"diag.get","re":6,"ok":true,"firmware":"v0.4-12-gabcdef","uptime":86400,"time":1790000000,"restart_reason":"power-on","heap_free":180000,"heap_min":150000,"psram_free":8000000,"battery":{"percent":85,"voltage":4010,"charging":false,"usb":false},"notifications":4}
        """.utf8))
        guard case .diagnostics(let report, 6) = diag else { return XCTFail("\(diag)") }
        XCTAssertEqual(report.uptime, 86400)
        XCTAssertEqual(report.restartReason, "power-on")
        XCTAssertEqual(report.battery.voltageMillivolts, 4010)
        XCTAssertFalse(report.battery.charging)
        XCTAssertEqual(report.notifications, 4)

        let request = try MessageCodec.decode(Data("""
        {"t":"weather.request","reason":"stale"}
        """.utf8))
        guard case .weatherRequest("stale") = request else { return XCTFail("\(request)") }

        let failure = try MessageCodec.decode(Data("""
        {"t":"nonsense","re":8,"ok":false,"error":"unknown type"}
        """.utf8))
        guard case .failed("nonsense", 8, "unknown type") = failure else { return XCTFail("\(failure)") }

        let planned = try MessageCodec.decode(Data("""
        {"t":"alarms.get"}
        """.utf8))
        guard case .planned("alarms.get", nil) = planned else { return XCTFail("\(planned)") }
    }

    func testWholeNumberDoublesStillDecode() throws {
        let health = try MessageCodec.decode(Data("""
        {"t":"health.get","re":5,"ok":true,"date":"2026-10-03","today":4321.0,"goal":8000.0,"days":[1.0,null]}
        """.utf8))
        guard case .health(let snapshot, _) = health else { return XCTFail("\(health)") }
        XCTAssertEqual(snapshot.today, 4321)
        XCTAssertEqual(snapshot.days, [1, nil])
    }

    func testMessageLimit() {
        struct Blob: Encodable { var t = "x"; var blob: String }
        let huge = String(repeating: "a", count: ChunkFramer.maxMessageBytes)
        XCTAssertThrowsError(try MessageCodec.encode(Blob(blob: huge)))
    }

    func testForecastStartsAtTheNextHourAndKeepsAQI() throws {
        var calendar = Calendar(identifier: .gregorian)
        calendar.timeZone = TimeZone(secondsFromGMT: 0)!
        let now = try XCTUnwrap(calendar.date(from: DateComponents(
            calendar: calendar, timeZone: calendar.timeZone,
            year: 2026, month: 10, day: 3, hour: 15, minute: 20
        )))
        let nextHour = try XCTUnwrap(calendar.dateInterval(of: .hour, for: now)?.end)
        let origin = Int(nextHour.timeIntervalSince1970) - 2 * 3600

        var times: [Int] = []
        var temps: [Double] = []
        var codes: [Int] = []
        var precip: [Any] = []
        for index in 0 ..< 16 {
            times.append(origin + index * 3600)
            temps.append(50 + Double(index) + 0.2)
            codes.append(index == 2 ? 3 : 1)
            precip.append(index == 2 ? 10 : NSNull())
        }
        let day = origin
        let forecast: [String: Any] = [
            "current": [
                "temperature_2m": 57.2,
                "apparent_temperature": 55.4,
                "weather_code": 3,
                "is_day": 0,
                "relative_humidity_2m": 80,
                "wind_speed_10m": 5.2,
                "uv_index": 2.44,
            ],
            "hourly": [
                "time": times,
                "temperature_2m": temps,
                "weather_code": codes,
                "precipitation_probability": precip,
            ],
            "daily": [
                "time": [day],
                "weather_code": [61],
                "temperature_2m_max": [61.2],
                "temperature_2m_min": [48.6],
                "precipitation_probability_max": [80],
                "sunrise": [day + 7 * 3600],
                "sunset": [day + 19 * 3600],
            ],
        ]
        let air: [String: Any] = ["current": ["us_aqi": 38]]
        let payload = try WeatherService().makePayload(
            forecastJSON: try JSONSerialization.data(withJSONObject: forecast),
            airQualityJSON: try JSONSerialization.data(withJSONObject: air),
            temperature: .fahrenheit,
            wind: .mph,
            place: String(repeating: "S", count: 40),
            latitude: 47.6061,
            longitude: -122.3321,
            now: now,
            calendar: calendar
        )

        XCTAssertEqual(payload.location.utf8.count, 31)
        XCTAssertEqual(payload.unit, "F")
        XCTAssertEqual(payload.windUnit, "mph")
        XCTAssertEqual(payload.lat, 47.6061)
        XCTAssertEqual(payload.lon, -122.3321)
        XCTAssertEqual(payload.now.temp, 57)
        XCTAssertEqual(payload.now.feels, 55)
        XCTAssertEqual(payload.now.wind, 5)
        XCTAssertEqual(payload.now.uv, 2.4)
        XCTAssertEqual(payload.now.aqi, 38)
        XCTAssertFalse(payload.now.day)
        XCTAssertEqual(payload.hours.count, 12)
        XCTAssertEqual(payload.hours.first?.time, Int(nextHour.timeIntervalSince1970))
        XCTAssertEqual(payload.hours.first?.temp, 52)
        XCTAssertEqual(payload.hours.first?.code, 3)
        XCTAssertEqual(payload.hours.first?.precip, 10)
        XCTAssertNil(payload.hours.dropFirst().first?.precip)
        XCTAssertEqual(payload.today.high, 61)
        XCTAssertEqual(payload.today.low, 49)
        XCTAssertEqual(payload.today.precip, 80)
        XCTAssertEqual(payload.days.count, 1)
        XCTAssertEqual(payload.days[0].code, 61)
    }

    func testMissingAQIIsOmitted() throws {
        let forecast: [String: Any] = [
            "current": [
                "temperature_2m": 10,
                "apparent_temperature": 9,
                "weather_code": 0,
                "is_day": 1,
                "relative_humidity_2m": 40,
                "wind_speed_10m": 3,
            ],
        ]
        let payload = try WeatherService().makePayload(
            forecastJSON: try JSONSerialization.data(withJSONObject: forecast),
            airQualityJSON: nil,
            temperature: .celsius,
            wind: .kmh,
            place: "Seattle",
            latitude: 1,
            longitude: 2
        )
        XCTAssertNil(payload.now.aqi)
        XCTAssertNil(payload.now.uv)
        XCTAssertEqual(payload.unit, "C")
        XCTAssertEqual(payload.windUnit, "kmh")
        XCTAssertTrue(payload.hours.isEmpty)
        XCTAssertTrue(payload.days.isEmpty)
    }

    private func sampleForecast() throws -> WeatherPayload {
        let forecast: [String: Any] = [
            "current": [
                "temperature_2m": 57,
                "apparent_temperature": 55,
                "weather_code": 3,
                "is_day": 1,
                "relative_humidity_2m": 80,
                "wind_speed_10m": 5,
                "uv_index": 2.4,
            ],
        ]
        return try WeatherService().makePayload(
            forecastJSON: try JSONSerialization.data(withJSONObject: forecast),
            airQualityJSON: nil,
            temperature: .fahrenheit,
            wind: .mph,
            place: "Seattle",
            latitude: 47.61,
            longitude: -122.33
        )
    }

    private func json(_ data: Data) throws -> [String: Any] {
        try XCTUnwrap(JSONSerialization.jsonObject(with: data) as? [String: Any])
    }

    private func int(_ value: Any?) -> Int? {
        switch value {
        case let int as Int: return int
        case let number as NSNumber: return number.intValue
        default: return nil
        }
    }
}

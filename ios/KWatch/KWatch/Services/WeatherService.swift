import Foundation

struct GeoPoint: Equatable {
    var latitude: Double
    var longitude: Double
}

enum LocationAccess: Equatable {
    case notDetermined
    case denied
    case authorized
}

enum WeatherError: Error, LocalizedError {
    case badResponse
    case noLocation

    var errorDescription: String? {
        switch self {
        case .badResponse:
            return "The weather service returned an unexpected forecast."
        case .noLocation:
            return "Allow location access to fetch the forecast."
        }
    }
}

/// Open-Meteo forecast and US AQI. No API key.
struct WeatherService {
    var session: URLSession = WeatherService.defaultSession

    static let defaultSession: URLSession = {
        let configuration = URLSessionConfiguration.default
        configuration.timeoutIntervalForRequest = 25
        configuration.waitsForConnectivity = true
        return URLSession(configuration: configuration)
    }()

    func fetch(
        latitude: Double,
        longitude: Double,
        temperature: TemperatureUnit,
        wind: WindUnit,
        place: String
    ) async throws -> WeatherPayload {
        async let forecast = get(forecastURL(latitude: latitude, longitude: longitude, temperature: temperature, wind: wind))
        async let air = optionalGet(airQualityURL(latitude: latitude, longitude: longitude))
        return try makePayload(
            forecastJSON: try await forecast,
            airQualityJSON: await air,
            temperature: temperature,
            wind: wind,
            place: place,
            latitude: latitude,
            longitude: longitude
        )
    }

    func makePayload(
        forecastJSON: Data,
        airQualityJSON: Data?,
        temperature: TemperatureUnit,
        wind: WindUnit,
        place: String,
        latitude: Double,
        longitude: Double,
        now: Date = Date(),
        calendar: Calendar = .current
    ) throws -> WeatherPayload {
        guard let root = try JSONSerialization.jsonObject(with: forecastJSON) as? [String: Any],
              let current = root["current"] as? [String: Any],
              let temp = doubleValue(current["temperature_2m"]),
              let feels = doubleValue(current["apparent_temperature"]),
              let code = intValue(current["weather_code"]),
              let humidity = intValue(current["relative_humidity_2m"]),
              let windSpeed = doubleValue(current["wind_speed_10m"])
        else { throw WeatherError.badResponse }

        let hourly = root["hourly"] as? [String: Any]
        let daily = root["daily"] as? [String: Any]
        let hours = nextHours(hourly, now: now, calendar: calendar)
        let days = upcomingDays(daily)
        let today = days.first

        var aqi: Int?
        if let airQualityJSON, let value = usAQI(from: airQualityJSON) {
            aqi = min(500, max(0, value))
        }

        let name = Self.watchPlace(place.isEmpty ? "Current location" : place)
        return WeatherPayload(
            updated: Int(now.timeIntervalSince1970),
            location: name,
            lat: Self.roundedCoordinate(latitude),
            lon: Self.roundedCoordinate(longitude),
            unit: temperature.rawValue,
            windUnit: wind.rawValue,
            now: WeatherPayload.Now(
                temp: Self.whole(temp),
                feels: Self.whole(feels),
                code: code,
                day: (intValue(current["is_day"]) ?? 1) == 1,
                humidity: min(100, max(0, humidity)),
                wind: Self.whole(windSpeed),
                uv: doubleValue(current["uv_index"]).map { ($0 * 10).rounded() / 10 },
                aqi: aqi
            ),
            today: WeatherPayload.Today(
                high: today.map { Self.whole($0.high) },
                low: today.map { Self.whole($0.low) },
                precip: today?.precip,
                sunrise: today?.sunrise,
                sunset: today?.sunset
            ),
            hours: hours,
            days: days.map {
                WeatherPayload.Day(
                    time: $0.time,
                    high: Self.whole($0.high),
                    low: Self.whole($0.low),
                    code: $0.code,
                    precip: $0.precip
                )
            }
        )
    }

    /// The watch's location field is 31 bytes plus a NUL.
    static func watchPlace(_ name: String) -> String {
        var result = ""
        var count = 0
        for scalar in name.unicodeScalars {
            let bytes = scalar.utf8.count
            if count + bytes > 31 { break }
            result.unicodeScalars.append(scalar)
            count += bytes
        }
        return result.isEmpty ? "Current location" : result
    }

    static func roundedCoordinate(_ value: Double) -> Double {
        (value * 10_000).rounded() / 10_000
    }

    static func whole(_ value: Double) -> Int {
        Int(value.rounded())
    }

    private struct ParsedDay {
        var time: Int
        var high: Double
        var low: Double
        var code: Int
        var precip: Int?
        var sunrise: Int?
        var sunset: Int?
    }

    private func nextHours(_ hourly: [String: Any]?, now: Date, calendar: Calendar) -> [WeatherPayload.Hour] {
        guard let hourly,
              let times = doubles(hourly["time"]),
              let temps = doubles(hourly["temperature_2m"]),
              let codes = doubles(hourly["weather_code"])
        else { return [] }
        let precip = doubles(hourly["precipitation_probability"])
        let startOfHour = calendar.dateInterval(of: .hour, for: now)?.end ?? now
        let threshold = Int(startOfHour.timeIntervalSince1970)
        var hours: [WeatherPayload.Hour] = []
        for index in times.indices {
            guard let timeValue = times[index], index < temps.count, let temp = temps[index],
                  index < codes.count, let code = codes[index]
            else { continue }
            let time = Int(timeValue.rounded())
            guard time >= threshold else { continue }
            hours.append(WeatherPayload.Hour(
                time: time,
                temp: Self.whole(temp),
                code: Int(code.rounded()),
                precip: percent(precip, index: index)
            ))
            if hours.count == 12 { break }
        }
        return hours
    }

    private func upcomingDays(_ daily: [String: Any]?) -> [ParsedDay] {
        guard let daily,
              let times = doubles(daily["time"]),
              let highs = doubles(daily["temperature_2m_max"]),
              let lows = doubles(daily["temperature_2m_min"]),
              let codes = doubles(daily["weather_code"])
        else { return [] }
        let precip = doubles(daily["precipitation_probability_max"])
        let sunrise = doubles(daily["sunrise"])
        let sunset = doubles(daily["sunset"])
        var days: [ParsedDay] = []
        for index in times.indices.prefix(7) {
            guard let time = times[index], index < highs.count, let high = highs[index],
                  index < lows.count, let low = lows[index], index < codes.count, let code = codes[index]
            else { continue }
            days.append(ParsedDay(
                time: Int(time.rounded()),
                high: high,
                low: low,
                code: Int(code.rounded()),
                precip: percent(precip, index: index),
                sunrise: optionalUnix(sunrise, index: index),
                sunset: optionalUnix(sunset, index: index)
            ))
        }
        return days
    }

    private func usAQI(from data: Data) -> Int? {
        guard let root = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
              let current = root["current"] as? [String: Any],
              let value = doubleValue(current["us_aqi"])
        else { return nil }
        return Int(value.rounded())
    }

    private func percent(_ values: [Double?]?, index: Int) -> Int? {
        guard let values, index < values.count, let value = values[index] else { return nil }
        return min(100, max(0, Int(value.rounded())))
    }

    private func optionalUnix(_ values: [Double?]?, index: Int) -> Int? {
        guard let values, index < values.count, let value = values[index] else { return nil }
        return Int(value.rounded())
    }

    /// Preserves nulls so a missing value does not shift the rest of the series.
    private func doubles(_ value: Any?) -> [Double?]? {
        guard let array = value as? [Any] else { return nil }
        return array.map { item in
            if item is NSNull { return nil }
            return doubleValue(item)
        }
    }

    private func forecastURL(latitude: Double, longitude: Double, temperature: TemperatureUnit, wind: WindUnit) -> URL? {
        var components = URLComponents(string: "https://api.open-meteo.com/v1/forecast")
        components?.queryItems = [
            URLQueryItem(name: "latitude", value: String(format: "%.4f", latitude)),
            URLQueryItem(name: "longitude", value: String(format: "%.4f", longitude)),
            URLQueryItem(name: "current", value: "temperature_2m,apparent_temperature,weather_code,is_day,relative_humidity_2m,wind_speed_10m,uv_index"),
            URLQueryItem(name: "hourly", value: "temperature_2m,weather_code,precipitation_probability"),
            URLQueryItem(name: "daily", value: "weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max,sunrise,sunset"),
            URLQueryItem(name: "temperature_unit", value: temperature.apiName),
            URLQueryItem(name: "wind_speed_unit", value: wind.apiName),
            URLQueryItem(name: "timeformat", value: "unixtime"),
            URLQueryItem(name: "timezone", value: "auto"),
            URLQueryItem(name: "forecast_days", value: "7"),
        ]
        return components?.url
    }

    private func airQualityURL(latitude: Double, longitude: Double) -> URL? {
        var components = URLComponents(string: "https://air-quality-api.open-meteo.com/v1/air-quality")
        components?.queryItems = [
            URLQueryItem(name: "latitude", value: String(format: "%.4f", latitude)),
            URLQueryItem(name: "longitude", value: String(format: "%.4f", longitude)),
            URLQueryItem(name: "current", value: "us_aqi"),
            URLQueryItem(name: "timezone", value: "auto"),
        ]
        return components?.url
    }

    private func get(_ url: URL?) async throws -> Data {
        guard let url else { throw WeatherError.badResponse }
        let (data, response) = try await session.data(from: url)
        guard let http = response as? HTTPURLResponse, http.statusCode == 200 else {
            throw WeatherError.badResponse
        }
        return data
    }

    private func optionalGet(_ url: URL?) async -> Data? {
        guard let url else { return nil }
        return try? await get(url)
    }

    private func doubleValue(_ value: Any?) -> Double? {
        switch value {
        case let double as Double:
            return double
        case let int as Int:
            return Double(int)
        case let number as NSNumber:
            return number.doubleValue
        default:
            return nil
        }
    }

    private func intValue(_ value: Any?) -> Int? {
        guard let double = doubleValue(value) else { return nil }
        return Int(double.rounded())
    }
}

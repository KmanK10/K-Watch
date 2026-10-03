import BackgroundTasks
import Foundation
import UIKit

/// Owns the watch link, the forecast, and the screens' data.
@MainActor
final class AppModel: ObservableObject {
    enum WeatherReason {
        case connect
        case watch(String)
        case pull
        case background
        case units
        case location
        case foreground
    }

    let connection = WatchConnection()
    let location = LocationProvider()
    private let weatherService = WeatherService()

    @Published var link: WatchConnection.Link = .searching
    @Published var hello: HelloInfo?
    @Published var weather: WeatherPayload?
    @Published var settings: WatchSettings?
    @Published var health: HealthSnapshot?
    @Published var diagnostics: Diagnostics?
    @Published var temperature: TemperatureUnit
    @Published var wind: WindUnit
    @Published var locationAccess: LocationAccess = .notDetermined
    @Published var log: [LogLine] = []
    @Published var banner: String?
    @Published var syncNote: String?
    @Published var findNote: String?
    @Published var showLog = false
    @Published var isFinding = false

    private var started = false
    private var registeredRefresh = false
    private var flight: (temperature: TemperatureUnit, wind: WindUnit, task: Task<WeatherPayload, Error>)?
    private let weatherCacheKey = "weather.cache"
    private let temperatureKey = "temp.unit"
    private let windKey = "wind.unit"

    static let weatherRefreshID = "com.kiefermenard.kwatch.weather-refresh"

    init() {
        let stored = Self.storedUnits()
        temperature = stored.temperature
        wind = stored.wind
        if let data = UserDefaults.standard.data(forKey: weatherCacheKey),
           let cached = try? JSONDecoder().decode(WeatherPayload.self, from: data) {
            weather = cached
            syncNote = "Saved on this iPhone. It will go to the watch when it connects."
        }

        connection.onLink = { [weak self] link in
            self?.link = link
        }
        connection.onLog = { [weak self] kind, text in
            self?.appendLog(kind, text)
        }
        connection.onEvent = { [weak self] message in
            self?.handle(message)
        }
        connection.onReady = { [weak self] in
            self?.watchReady()
        }
        location.onLocation = { [weak self] _ in
            self?.locationMoved()
        }
        location.onAccess = { [weak self] access in
            self?.locationAccess = access
        }
    }

    /// Creates the central manager during launch so Core Bluetooth can restore state.
    func start() {
        guard !started else { return }
        started = true
        registerRefreshIfNeeded()
        connection.start()
        location.start()
        locationAccess = location.access
        scheduleWeatherRefresh()
        appendLog(.info, "Looking for the watch.")
    }

    func sceneBecameActive() {
        location.requestOneShot()
        Task { await refreshWeather(reason: .foreground) }
    }

    func setTemperature(_ unit: TemperatureUnit) {
        guard unit != temperature else { return }
        temperature = unit
        persistUnits()
        if connection.isReady {
            pushSettings(SettingsPatch(temperature: unit))
        }
        Task { await refreshWeather(reason: .units) }
    }

    func setWind(_ unit: WindUnit) {
        guard unit != wind else { return }
        wind = unit
        persistUnits()
        if connection.isReady {
            pushSettings(SettingsPatch(wind: unit))
        }
        Task { await refreshWeather(reason: .units) }
    }

    @discardableResult
    func refreshWeather(reason: WeatherReason) async -> Bool {
        if case .foreground = reason, let weather, !weather.isStale {
            if !connection.isReady {
                syncNote = "Saved on this iPhone. It will go to the watch when it connects."
            }
            return true
        }
        let taskID = UIApplication.shared.beginBackgroundTask(withName: "kwatch.weather") {}
        defer {
            if taskID != .invalid {
                UIApplication.shared.endBackgroundTask(taskID)
            }
        }
        do {
            let payload = try await loadForecast()
            weather = payload
            if let data = try? JSONEncoder().encode(payload) {
                UserDefaults.standard.set(data, forKey: weatherCacheKey)
            }
            try await sendWeather(payload)
            banner = nil
            return true
        } catch is CancellationError {
            return false
        } catch {
            let failure = error
            if let weather, connection.isReady {
                do {
                    if try await sendWeather(weather) {
                        syncNote = "Sent the saved forecast. \(failure.localizedDescription)"
                        return false
                    }
                } catch {
                    syncNote = failure.localizedDescription
                    appendLog(.info, "Weather: \(failure.localizedDescription)")
                    return false
                }
            }
            syncNote = failure.localizedDescription
            appendLog(.info, "Weather: \(failure.localizedDescription)")
            return false
        }
    }

    func refreshSettings() async {
        await request("settings.get") { message in
            if case .settings(let settings, _) = message {
                self.applyWatchSettings(settings)
            }
        }
    }

    func refreshHealth() async {
        await request("health.get") { message in
            if case .health(let health, _) = message {
                self.health = health
            }
        }
    }

    func refreshDiagnostics() async {
        await request("diag.get") { message in
            if case .diagnostics(let diagnostics, _) = message {
                self.diagnostics = diagnostics
            }
        }
    }

    func pushSettings(_ patch: SettingsPatch) {
        guard !patch.isEmpty else { return }
        Task { await sendSettings(patch) }
    }

    func findWatch() {
        guard !isFinding else { return }
        isFinding = true
        findNote = "Buzzing…"
        Task {
            defer { isFinding = false }
            do {
                let id = connection.makeID()
                let data = try MessageCodec.encode(SimpleRequest(t: "find", id: id))
                let reply = try await connection.request(data, id: id)
                if case .failed(_, _, let message) = reply {
                    banner = message
                    findNote = nil
                    return
                }
                findNote = "Buzzing"
                try? await Task.sleep(nanoseconds: 2_000_000_000)
                findNote = nil
            } catch {
                banner = error.localizedDescription
                findNote = nil
            }
        }
    }

    func clearLog() {
        log.removeAll()
    }

    func openSettings() {
        guard let url = URL(string: UIApplication.openSettingsURLString) else { return }
        UIApplication.shared.open(url)
    }

    private func watchReady() {
        Task {
            await sendHello()
            await refreshSettings()
            await refreshWeather(reason: .connect)
            await refreshHealth()
            await refreshDiagnostics()
        }
    }

    private func locationMoved() {
        Task { await refreshWeather(reason: .location) }
    }

    private func handle(_ message: IncomingMessage) {
        switch message {
        case .weatherRequest(let reason):
            appendLog(.info, "Watch asked for weather (\(reason ?? "unspecified")).")
            Task { await refreshWeather(reason: .watch(reason ?? "")) }
        case .settings(let settings, _):
            applyWatchSettings(settings)
        case .planned(let type, _):
            appendLog(.info, "\(type) is reserved and not handled yet.")
        case .failed(_, _, let message):
            banner = message
        case .unrecognized(let type, _):
            appendLog(.info, "Ignored unknown message \(type).")
        default:
            break
        }
    }

    private func sendHello() async {
        do {
            let id = connection.makeID()
            let message = HelloMessage(
                id: id,
                app: CompanionProtocol.appVersion,
                protocolVersion: CompanionProtocol.version,
                utcOffset: CompanionProtocol.utcOffset
            )
            let reply = try await connection.request(try MessageCodec.encode(message), id: id)
            switch reply {
            case .hello(let info, _):
                hello = info
                banner = nil
            case .failed(_, _, let message):
                banner = message
            default:
                break
            }
        } catch {
            appendLog(.info, "Hello failed: \(error.localizedDescription)")
        }
    }

    private func loadForecast() async throws -> WeatherPayload {
        if let flight, flight.temperature == temperature, flight.wind == wind {
            return try await flight.task.value
        }
        flight?.task.cancel()
        let temperature = self.temperature
        let wind = self.wind
        let point = try await resolveCoordinate()
        if let flight, flight.temperature == temperature, flight.wind == wind {
            return try await flight.task.value
        }
        let task = Task {
            let place = await self.location.resolvedPlace(near: point)
            return try await self.weatherService.fetch(
                latitude: point.latitude,
                longitude: point.longitude,
                temperature: temperature,
                wind: wind,
                place: place
            )
        }
        flight = (temperature, wind, task)
        do {
            let payload = try await task.value
            if flight?.task == task { flight = nil }
            return payload
        } catch {
            if flight?.task == task { flight = nil }
            throw error
        }
    }

    private func resolveCoordinate() async throws -> GeoPoint {
        if let coordinate = location.coordinate {
            return coordinate
        }
        location.requestOneShot()
        for _ in 0 ..< 32 {
            try await Task.sleep(nanoseconds: 250_000_000)
            if Task.isCancelled { throw CancellationError() }
            if let coordinate = location.coordinate {
                return coordinate
            }
        }
        throw WeatherError.noLocation
    }

    @discardableResult
    private func sendWeather(_ payload: WeatherPayload) async throws -> Bool {
        guard connection.isReady else {
            syncNote = "Saved on this iPhone. It will go to the watch when it connects."
            return false
        }
        let id = connection.makeID()
        let message = WeatherSetMessage(id: id, utcOffset: CompanionProtocol.utcOffset, weather: payload)
        let reply = try await connection.request(try MessageCodec.encode(message), id: id)
        if case .failed(_, _, let message) = reply {
            throw CompanionError.rejected(message)
        }
        syncNote = "Sent to the watch"
        return true
    }

    private func sendSettings(_ patch: SettingsPatch) async {
        do {
            let id = connection.makeID()
            let message = SettingsMessage(t: "settings.set", id: id, settings: patch)
            let reply = try await connection.request(try MessageCodec.encode(message), id: id)
            switch reply {
            case .settings(let settings, _):
                applyWatchSettings(settings)
                banner = nil
            case .failed(_, _, let message):
                banner = message
            default:
                break
            }
        } catch {
            banner = error.localizedDescription
        }
    }

    private func request(_ type: String, apply: (IncomingMessage) -> Void) async {
        guard connection.isReady else { return }
        do {
            let id = connection.makeID()
            let reply = try await connection.request(try MessageCodec.encode(SimpleRequest(t: type, id: id)), id: id)
            if case .failed(_, _, let message) = reply {
                banner = message
                return
            }
            apply(reply)
            banner = nil
        } catch {
            appendLog(.info, "\(type) failed: \(error.localizedDescription)")
        }
    }

    /// The watch is the source of truth once it reports a unit. A missing field means this
    /// firmware does not store it yet, so the phone keeps the choice it already has.
    private func applyWatchSettings(_ incoming: WatchSettings) {
        var changed = false
        if let temperature = incoming.temperature, temperature != self.temperature {
            self.temperature = temperature
            changed = true
        }
        if let wind = incoming.wind, wind != self.wind {
            self.wind = wind
            changed = true
        }
        if changed {
            persistUnits()
            Task { await refreshWeather(reason: .units) }
        }
        settings = incoming
    }

    private func persistUnits() {
        UserDefaults.standard.set(temperature.rawValue, forKey: temperatureKey)
        UserDefaults.standard.set(wind.rawValue, forKey: windKey)
    }

    private static func storedUnits() -> (temperature: TemperatureUnit, wind: WindUnit) {
        let defaults = UserDefaults.standard
        if let temperature = defaults.string(forKey: "temp.unit").flatMap(TemperatureUnit.init(rawValue:)),
           let wind = defaults.string(forKey: "wind.unit").flatMap(WindUnit.init(rawValue:)) {
            return (temperature, wind)
        }
        if defaults.string(forKey: "units") == "metric" {
            return (.celsius, .kmh)
        }
        return (.fahrenheit, .mph)
    }

    private func appendLog(_ kind: LogLine.Kind, _ text: String) {
        log.append(LogLine(kind: kind, text: text))
        if log.count > 300 {
            log.removeFirst(log.count - 300)
        }
    }

    private func registerRefreshIfNeeded() {
        guard !registeredRefresh else { return }
        registeredRefresh = true
        BGTaskScheduler.shared.register(forTaskWithIdentifier: Self.weatherRefreshID, using: nil) { task in
            guard let task = task as? BGAppRefreshTask else { return }
            Task { @MainActor in
                await self.handleBackgroundRefresh(task)
            }
        }
    }

    private func scheduleWeatherRefresh() {
        let identifier = Self.weatherRefreshID
        let earliest = Date(timeIntervalSinceNow: 30 * 60)
        // submitTaskRequest runs off the main thread; the scheduler asks not to call it there.
        Task.detached {
            let request = BGAppRefreshTaskRequest(identifier: identifier)
            request.earliestBeginDate = earliest
            try? await BGTaskScheduler.shared.submitTaskRequest(request)
        }
    }

    private func handleBackgroundRefresh(_ task: BGAppRefreshTask) async {
        scheduleWeatherRefresh()
        let work = Task { await self.refreshWeather(reason: .background) }
        task.expirationHandler = { work.cancel() }
        let ok = await work.value
        task.setTaskCompleted(success: ok)
    }
}

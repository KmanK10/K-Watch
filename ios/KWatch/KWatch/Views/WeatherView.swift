import SwiftUI

struct WeatherView: View {
    @EnvironmentObject private var model: AppModel

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 16) {
                unitsPicker
                if let weather = model.weather {
                    forecast(weather)
                } else {
                    empty
                }
            }
            .padding(20)
        }
        .aboveTabBar()
        .background(Color.black)
        .contentMargins(.bottom, 12, for: .scrollContent)
        .refreshable { await model.refreshWeather(reason: .pull) }
    }

    private var unitsPicker: some View {
        VStack(alignment: .leading, spacing: 8) {
            Picker("Temperature", selection: Binding(
                get: { model.temperature },
                set: { model.setTemperature($0) }
            )) {
                ForEach(TemperatureUnit.allCases) { unit in
                    Text(unit.label).tag(unit)
                }
            }
            .pickerStyle(.segmented)
            Picker("Wind", selection: Binding(
                get: { model.wind },
                set: { model.setWind($0) }
            )) {
                ForEach(WindUnit.allCases) { unit in
                    Text(unit.label).tag(unit)
                }
            }
            .pickerStyle(.segmented)
        }
    }

    private var empty: some View {
        ContentUnavailableView {
            Label("No forecast yet", systemImage: "cloud.sun")
        } description: {
            Text(emptyMessage)
        } actions: {
            if model.locationAccess == .denied || model.link == .unauthorized || model.link == .bluetoothOff {
                Button("Open Settings", action: model.openSettings)
            }
            Button("Refresh") {
                Task { await model.refreshWeather(reason: .pull) }
            }
            .buttonStyle(.borderedProminent)
        }
        .frame(maxWidth: .infinity)
        .padding(.top, 24)
    }

    private var emptyMessage: String {
        if let syncNote = model.syncNote { return syncNote }
        switch model.locationAccess {
        case .denied:
            return "Allow location access to fetch the forecast for your watch."
        case .notDetermined:
            return "K-Watch will ask for your location, then fetch the forecast."
        case .authorized:
            return "Pull to refresh."
        }
    }

    @ViewBuilder
    private func forecast(_ weather: WeatherPayload) -> some View {
        let day = weather.now.day
        VStack(alignment: .leading, spacing: 4) {
            Text(weather.location)
                .font(.title3.weight(.semibold))
            Text(Format.updated(weather.updated))
                .font(.caption)
                .foregroundStyle(.secondary)
            if let syncNote = model.syncNote {
                Text(syncNote)
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
        }

        Card {
            HStack(alignment: .center, spacing: 16) {
                Image(systemName: WMO.symbol(weather.now.code, day: day))
                    .font(.system(size: 44))
                    .foregroundStyle(Theme.accent)
                    .frame(width: 56)
                VStack(alignment: .leading, spacing: 2) {
                    Text("\(weather.now.temp)°")
                        .font(.system(size: 56, weight: .light, design: .rounded))
                    Text(WMO.describe(weather.now.code))
                        .foregroundStyle(.secondary)
                    Text("Feels \(weather.now.feels)°")
                        .font(.subheadline)
                        .foregroundStyle(.secondary)
                }
                Spacer(minLength: 0)
            }
        }

        LazyVGrid(columns: [GridItem(.flexible()), GridItem(.flexible()), GridItem(.flexible())], spacing: 10) {
            metric("Humidity", "\(weather.now.humidity)%")
            metric("Wind", "\(weather.now.wind) \(WindUnit(rawValue: weather.windUnit)?.label ?? weather.windUnit)")
            metric("UV", weather.now.uv.map(Format.uv) ?? "—")
            metric("Air quality", weather.now.aqi.map(Format.aqi) ?? "—")
            metric("High", weather.today.high.map { "\($0)°" } ?? "—")
            metric("Low", weather.today.low.map { "\($0)°" } ?? "—")
        }

        Card {
            VStack(alignment: .leading, spacing: 10) {
                Text("Today")
                    .font(.headline)
                HStack {
                    if let precip = weather.today.precip, precip >= 10 {
                        Label("\(precip)%", systemImage: "drop.fill")
                            .foregroundStyle(Theme.precip)
                    }
                    Spacer()
                    if let sunrise = weather.today.sunrise {
                        Label(Format.clock(sunrise, twentyFourHour: model.settings?.clock24h ?? false), systemImage: "sunrise.fill")
                    }
                    if let sunset = weather.today.sunset {
                        Label(Format.clock(sunset, twentyFourHour: model.settings?.clock24h ?? false), systemImage: "sunset.fill")
                    }
                }
                .font(.subheadline)
            }
        }

        if !weather.hours.isEmpty {
            VStack(alignment: .leading, spacing: 8) {
                Text("Next hours")
                    .font(.headline)
                ScrollView(.horizontal, showsIndicators: false) {
                    HStack(spacing: 8) {
                        ForEach(weather.hours) { hour in
                            VStack(spacing: 6) {
                                Text(Format.hour(hour.time, twentyFourHour: model.settings?.clock24h ?? false))
                                    .font(.caption)
                                    .foregroundStyle(.secondary)
                                Image(systemName: WMO.symbol(hour.code, day: hourIsDay(hour.time, weather: weather)))
                                    .foregroundStyle(Theme.accent)
                                Text("\(hour.temp)°")
                                    .font(.subheadline.weight(.semibold))
                                if let precip = hour.precip, precip >= 10 {
                                    Text("\(precip)%")
                                        .font(.caption2)
                                        .foregroundStyle(Theme.precip)
                                } else {
                                    Text(" ")
                                        .font(.caption2)
                                }
                            }
                            .frame(width: 58)
                            .padding(.vertical, 10)
                            .background(Theme.card, in: RoundedRectangle(cornerRadius: 12, style: .continuous))
                        }
                    }
                }
            }
        }

        if !weather.days.isEmpty {
            VStack(alignment: .leading, spacing: 8) {
                Text("This week")
                    .font(.headline)
                let floor = weather.days.map(\.low).min() ?? 0
                let ceiling = weather.days.map(\.high).max() ?? 0
                let span = max(ceiling - floor, 1)
                Card {
                    VStack(spacing: 12) {
                        ForEach(weather.days) { day in
                            HStack(spacing: 8) {
                                Text(Format.weekday(day.time, todayStart: weather.days.first?.time))
                                    .frame(width: 52, alignment: .leading)
                                    .font(.subheadline)
                                Image(systemName: WMO.symbol(day.code, day: true))
                                    .foregroundStyle(Theme.accent)
                                    .frame(width: 22)
                                if let precip = day.precip, precip >= 10 {
                                    Text("\(precip)%")
                                        .font(.caption)
                                        .foregroundStyle(Theme.precip)
                                        .frame(width: 36, alignment: .trailing)
                                } else {
                                    Text(" ")
                                        .frame(width: 36)
                                }
                                Text("\(day.low)°")
                                    .font(.subheadline)
                                    .foregroundStyle(.secondary)
                                    .frame(width: 36, alignment: .trailing)
                                RangeBar(low: day.low, high: day.high, floor: floor, span: span)
                                Text("\(day.high)°")
                                    .font(.subheadline.weight(.semibold))
                                    .frame(width: 36, alignment: .leading)
                            }
                        }
                    }
                }
            }
        }
    }

    private func hourIsDay(_ time: Int, weather: WeatherPayload) -> Bool {
        guard let sunrise = weather.today.sunrise, let sunset = weather.today.sunset,
              let todayStart = weather.days.first?.time else {
            return weather.now.day
        }
        let date = Date(timeIntervalSince1970: TimeInterval(time))
        let today = Date(timeIntervalSince1970: TimeInterval(todayStart))
        guard Calendar.current.isDate(date, inSameDayAs: today) else { return false }
        return time >= sunrise && time < sunset
    }

    private func metric(_ title: String, _ value: String) -> some View {
        VStack(alignment: .leading, spacing: 4) {
            Text(title)
                .font(.caption)
                .foregroundStyle(.secondary)
            Text(value)
                .font(.subheadline.weight(.semibold))
                .lineLimit(2)
                .minimumScaleFactor(0.7)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(12)
        .background(Theme.card, in: RoundedRectangle(cornerRadius: 12, style: .continuous))
    }
}

private struct RangeBar: View {
    var low: Int
    var high: Int
    var floor: Int
    var span: Int

    var body: some View {
        GeometryReader { geo in
            let width = geo.size.width
            let start = CGFloat(low - floor) / CGFloat(span) * width
            let end = CGFloat(high - floor) / CGFloat(span) * width
            ZStack(alignment: .leading) {
                Capsule().fill(Color.white.opacity(0.12))
                Capsule()
                    .fill(Theme.accent)
                    .frame(width: max(6, end - start))
                    .offset(x: min(max(0, start), max(0, width - 6)))
            }
        }
        .frame(height: 6)
    }
}

import SwiftUI

struct SettingsView: View {
    @EnvironmentObject private var model: AppModel
    @State private var brightness: Double = 60
    @State private var stepGoal: Double = 8000
    @State private var draggingBrightness = false
    @State private var draggingGoal = false

    var body: some View {
        Group {
            if let settings = model.settings {
                form(settings)
            } else {
                ContentUnavailableView {
                    Label("No settings yet", systemImage: "applewatch")
                } description: {
                    Text(model.link == .connected
                         ? "Reading settings from the watch."
                         : "Settings appear when the watch is connected.")
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity)
            }
        }
        .background(Color.black)
        .onAppear {
            syncSliders()
            Task { await model.refreshSettings() }
        }
        .onChange(of: model.settings) { _, _ in
            syncSliders()
        }
        .refreshable { await model.refreshSettings() }
    }

    private func form(_ settings: WatchSettings) -> some View {
        List {
            Section("Display") {
                VStack(alignment: .leading) {
                    HStack {
                        Text("Brightness")
                        Spacer()
                        Text("\(Int(brightness))%")
                            .foregroundStyle(.secondary)
                            .monospacedDigit()
                    }
                    Slider(value: $brightness, in: 10 ... 100, step: 1) { editing in
                        draggingBrightness = editing
                        if !editing {
                            model.pushSettings(SettingsPatch(brightness: Int(brightness)))
                        }
                    }
                }
                Picker("Screen timeout", selection: timeoutBinding) {
                    ForEach(WatchSettings.timeouts, id: \.self) { seconds in
                        Text("\(seconds) s").tag(seconds)
                    }
                }
            }
            Section("Wake") {
                toggle("Raise to wake", \.raiseToWake) { SettingsPatch(raiseToWake: $0) }
                toggle("Tap to wake", \.tapToWake) { SettingsPatch(tapToWake: $0) }
            }
            Section("Feedback") {
                toggle("Notification vibration", \.notifyVibrate) { SettingsPatch(notifyVibrate: $0) }
                toggle("Touch clicks", \.touchFeedback) { SettingsPatch(touchFeedback: $0) }
            }
            Section {
                toggle("24-hour clock", \.clock24h) { SettingsPatch(clock24h: $0) }
            }
            Section {
                Picker("Temperature", selection: Binding(
                    get: { model.temperature },
                    set: { model.setTemperature($0) }
                )) {
                    ForEach(TemperatureUnit.allCases) { unit in
                        Text(unit.label).tag(unit)
                    }
                }
                Picker("Wind", selection: Binding(
                    get: { model.wind },
                    set: { model.setWind($0) }
                )) {
                    ForEach(WindUnit.allCases) { unit in
                        Text(unit.label).tag(unit)
                    }
                }
            } footer: {
                Text("The watch remembers these. Forecasts are sent already converted.")
            }
            Section {
                toggle("Do not disturb", \.dnd) { SettingsPatch(dnd: $0) }
            } footer: {
                Text("Do not disturb turns off when the watch restarts.")
            }
            Section {
                VStack(alignment: .leading) {
                    HStack {
                        Text("Step goal")
                        Spacer()
                        Text(Format.steps(Int(stepGoal)))
                            .foregroundStyle(.secondary)
                            .monospacedDigit()
                    }
                    Slider(value: $stepGoal, in: 1000 ... 50000, step: 500) { editing in
                        draggingGoal = editing
                        if !editing {
                            model.pushSettings(SettingsPatch(stepGoal: Int(stepGoal)))
                        }
                    }
                }
            }
            Section {
                LabeledContent("Bluetooth", value: settings.bluetooth ? "On" : "Off")
            } footer: {
                Text("Bluetooth stays on. Turning it off would disconnect this app.")
            }
        }
        .listStyle(.insetGrouped)
        .scrollContentBackground(.hidden)
        .aboveTabBar()
        .disabled(model.link != .connected)
    }

    private var timeoutBinding: Binding<Int> {
        Binding(
            get: { model.settings?.screenTimeout ?? 5 },
            set: { newValue in
                model.settings?.screenTimeout = newValue
                model.pushSettings(SettingsPatch(screenTimeout: newValue))
            }
        )
    }

    private func toggle(
        _ title: String,
        _ key: WritableKeyPath<WatchSettings, Bool>,
        patch: @escaping (Bool) -> SettingsPatch
    ) -> some View {
        Toggle(title, isOn: Binding(
            get: { model.settings?[keyPath: key] ?? false },
            set: { newValue in
                model.settings?[keyPath: key] = newValue
                model.pushSettings(patch(newValue))
            }
        ))
    }

    private func syncSliders() {
        guard let settings = model.settings else { return }
        if !draggingBrightness {
            brightness = Double(settings.brightness)
        }
        if !draggingGoal {
            stepGoal = Double(settings.stepGoal)
        }
    }
}

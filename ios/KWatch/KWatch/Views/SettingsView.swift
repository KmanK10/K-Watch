import SwiftUI

struct SettingsView: View {
    @EnvironmentObject private var model: AppModel
    @State private var brightness: Double = 60
    @State private var stepGoal: Double = 8000
    @State private var draggingBrightness = false
    @State private var draggingGoal = false
    @State private var editingSleepStart = false
    @State private var editingSleepEnd = false
    @State private var sleepDraft = Date()

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
                Text("The watch converts the current forecast as soon as these change.")
            }
            if settings.sleepMode != nil {
                sleepSection(settings)
            }
            Section {
                toggle("Do not disturb", \.dnd) { SettingsPatch(dnd: $0) }
            } footer: {
                Text("Notifications stay quiet. Alarms and timers still ring. This turns off when the watch restarts.")
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
        .sheet(isPresented: $editingSleepStart, onDismiss: { commitSleepTime(\.sleepStart) { SettingsPatch(sleepStart: $0) } }) {
            sleepTimeSheet("Sleep starts")
        }
        .sheet(isPresented: $editingSleepEnd, onDismiss: { commitSleepTime(\.sleepEnd) { SettingsPatch(sleepEnd: $0) } }) {
            sleepTimeSheet("Sleep ends")
        }
    }

    @ViewBuilder
    private func sleepSection(_ settings: WatchSettings) -> some View {
        Section {
            toggle("Sleep mode", \.sleepMode) { SettingsPatch(sleepMode: $0) }
            Picker("Tint", selection: Binding(
                get: { model.settings?.sleepColor ?? .red },
                set: { newValue in
                    model.settings?.sleepColor = newValue
                    model.pushSettings(SettingsPatch(sleepColor: newValue))
                }
            )) {
                ForEach(SleepColor.allCases) { color in
                    Text(color.label).tag(color)
                }
            }
            toggle("Schedule", \.sleepSchedule) { SettingsPatch(sleepSchedule: $0) }
            if settings.sleepSchedule == true {
                sleepTimeRow("Starts", minutes: settings.sleepStart ?? 22 * 60, twentyFourHour: settings.clock24h) {
                    beginSleepEdit(settings.sleepStart ?? 22 * 60)
                    editingSleepStart = true
                }
                sleepTimeRow("Ends", minutes: settings.sleepEnd ?? 7 * 60, twentyFourHour: settings.clock24h) {
                    beginSleepEdit(settings.sleepEnd ?? 7 * 60)
                    editingSleepEnd = true
                }
            }
        } footer: {
            Text("The screen stays dim and tinted, touch is locked, and raise to wake is ignored. Alarms and timers still ring. The schedule turns sleep mode on and off, and you can still switch it by hand.")
        }
    }

    private func sleepTimeRow(_ title: String, minutes: Int, twentyFourHour: Bool, action: @escaping () -> Void) -> some View {
        Button(action: action) {
            LabeledContent(title, value: Format.timeOfDay(minutes, twentyFourHour: twentyFourHour))
        }
        .foregroundStyle(.primary)
    }

    private func sleepTimeSheet(_ title: String) -> some View {
        NavigationStack {
            DatePicker(title, selection: $sleepDraft, displayedComponents: .hourAndMinute)
                .datePickerStyle(.wheel)
                .labelsHidden()
                .navigationTitle(title)
                .navigationBarTitleDisplayMode(.inline)
                .toolbar {
                    ToolbarItem(placement: .confirmationAction) {
                        Button("Done") {
                            editingSleepStart = false
                            editingSleepEnd = false
                        }
                    }
                }
        }
        .presentationDetents([.height(320)])
    }

    private func beginSleepEdit(_ minutes: Int) {
        var components = Calendar.current.dateComponents([.year, .month, .day], from: Date())
        let wrapped = ((minutes % 1440) + 1440) % 1440
        components.hour = wrapped / 60
        components.minute = wrapped % 60
        sleepDraft = Calendar.current.date(from: components) ?? Date()
    }

    private func commitSleepTime(
        _ key: WritableKeyPath<WatchSettings, Int?>,
        patch: (Int) -> SettingsPatch
    ) {
        let parts = Calendar.current.dateComponents([.hour, .minute], from: sleepDraft)
        let minutes = min(24 * 60 - 1, max(0, (parts.hour ?? 0) * 60 + (parts.minute ?? 0)))
        guard model.settings?[keyPath: key] != minutes else { return }
        model.settings?[keyPath: key] = minutes
        model.pushSettings(patch(minutes))
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
        toggle(title, isOn: model.settings?[keyPath: key] ?? false) { newValue in
            model.settings?[keyPath: key] = newValue
            model.pushSettings(patch(newValue))
        }
    }

    private func toggle(
        _ title: String,
        _ key: WritableKeyPath<WatchSettings, Bool?>,
        patch: @escaping (Bool) -> SettingsPatch
    ) -> some View {
        toggle(title, isOn: model.settings?[keyPath: key] ?? false) { newValue in
            model.settings?[keyPath: key] = newValue
            model.pushSettings(patch(newValue))
        }
    }

    private func toggle(_ title: String, isOn: Bool, set: @escaping (Bool) -> Void) -> some View {
        Toggle(title, isOn: Binding(get: { isOn }, set: set))
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

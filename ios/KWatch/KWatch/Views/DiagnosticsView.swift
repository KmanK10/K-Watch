import SwiftUI

struct DiagnosticsView: View {
    @EnvironmentObject private var model: AppModel

    var body: some View {
        Group {
            if let diagnostics = model.diagnostics {
                report(diagnostics)
            } else {
                ContentUnavailableView {
                    Label("No diagnostics yet", systemImage: "waveform.path.ecg")
                } description: {
                    Text(model.link == .connected
                         ? "Reading diagnostics from the watch."
                         : "Diagnostics appear when the watch is connected.")
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity)
            }
        }
        .background(Color.black)
        .onAppear { Task { await model.refreshDiagnostics() } }
        .refreshable { await model.refreshDiagnostics() }
    }

    private func report(_ diagnostics: Diagnostics) -> some View {
        List {
            if let hello = model.hello {
                Section("Watch") {
                    row("Name", hello.name)
                    row("Device", hello.device)
                    row("Protocol", "\(hello.protocolVersion)")
                    if !hello.features.isEmpty {
                        row("Features", hello.features.joined(separator: ", "))
                    }
                }
            }
            Section("Firmware") {
                row("Version", diagnostics.firmware)
                row("Uptime", Format.uptime(diagnostics.uptime))
                row("Restart", Format.restart(diagnostics.restartReason))
                row("Watch time", Format.dayStamp(diagnostics.time))
            }
            Section("Memory") {
                row("Heap free", Format.bytes(diagnostics.heapFree))
                row("Heap minimum", Format.bytes(diagnostics.heapMin))
                row("PSRAM free", Format.bytes(diagnostics.psramFree))
            }
            Section("Battery") {
                row("Charge", diagnostics.battery.percent < 0 ? "Unknown" : "\(diagnostics.battery.percent)%")
                row("Voltage", Format.voltage(diagnostics.battery.voltageMillivolts))
                row("Charging", diagnostics.battery.charging ? "Yes" : "No")
                row("USB", diagnostics.battery.usb ? "Yes" : "No")
            }
            Section {
                row("Notifications", "\(diagnostics.notifications)")
            }
        }
        .listStyle(.insetGrouped)
        .scrollContentBackground(.hidden)
        .aboveTabBar()
    }

    private func row(_ title: String, _ value: String) -> some View {
        LabeledContent(title) {
            Text(value)
                .foregroundStyle(.secondary)
                .multilineTextAlignment(.trailing)
        }
    }
}

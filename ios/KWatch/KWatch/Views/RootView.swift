import SwiftUI

struct RootView: View {
    @EnvironmentObject private var model: AppModel

    var body: some View {
        VStack(spacing: 0) {
            ConnectionHeader()
            TabView {
                WeatherView()
                    .tabItem { Label("Weather", systemImage: "cloud.sun.fill") }
                SettingsView()
                    .tabItem { Label("Watch", systemImage: "applewatch") }
                HealthView()
                    .tabItem { Label("Health", systemImage: "figure.walk") }
                DiagnosticsView()
                    .tabItem { Label("Diagnostics", systemImage: "waveform.path.ecg") }
            }
        }
        .background(Color.black)
        .sheet(isPresented: $model.showLog) {
            NavigationStack {
                DebugLogView()
                    .navigationTitle("Debug log")
                    .navigationBarTitleDisplayMode(.inline)
                    .toolbar {
                        ToolbarItem(placement: .cancellationAction) {
                            Button("Clear", action: model.clearLog)
                        }
                        ToolbarItem(placement: .confirmationAction) {
                            Button("Done") { model.showLog = false }
                        }
                    }
            }
            .presentationDragIndicator(.visible)
        }
    }
}

import SwiftUI

@main
struct KWatchApp: App {
    @UIApplicationDelegateAdaptor(AppDelegate.self) private var appDelegate
    @Environment(\.scenePhase) private var scenePhase

    var body: some Scene {
        WindowGroup {
            RootView()
                .environmentObject(appDelegate.model)
                .preferredColorScheme(.dark)
                .tint(Theme.accent)
                .onChange(of: scenePhase) { _, phase in
                    if phase == .active {
                        appDelegate.model.sceneBecameActive()
                    }
                }
        }
    }
}

@MainActor
final class AppDelegate: NSObject, UIApplicationDelegate {
    let model = AppModel()

    func application(
        _ application: UIApplication,
        didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey: Any]? = nil
    ) -> Bool {
        model.start()
        return true
    }
}

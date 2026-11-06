import SwiftUI

@main
struct FirebirdInstallerApp: App {
    var body: some Scene {
        WindowGroup {
            WizardView()
                .frame(width: 700, height: 750)
        }
        .windowStyle(.hiddenTitleBar)
    }
}

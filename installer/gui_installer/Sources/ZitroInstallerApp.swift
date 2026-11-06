import AppKit
import SwiftUI

final class InstallerAppDelegate: NSObject, NSApplicationDelegate {
    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { true }

    // Never leave the flasher running behind a closed window: it would keep the USB port locked
    func applicationWillTerminate(_ notification: Notification) {
        WizardViewModel.runningFlasher?.terminate()
    }
}

@main
struct ZitroInstallerApp: App {
    @NSApplicationDelegateAdaptor(InstallerAppDelegate.self) private var delegate

    var body: some Scene {
        WindowGroup {
            WizardView()
                .frame(width: 640, height: 700)
                .preferredColorScheme(.light)
        }
        .windowStyle(HiddenTitleBarWindowStyle())
    }
}

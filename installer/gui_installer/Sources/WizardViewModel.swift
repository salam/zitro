import Foundation
import Combine

struct Device: Identifiable, Hashable {
    let id = UUID()
    let path: String

    var displayName: String {
        let name = (path as NSString).lastPathComponent
        return "\(name) (\(path))"
    }
}

enum WizardScreen: Int {
    case welcome = 0
    case apiKey = 1
    case wifi = 2
    case connectDevice = 3
    case flashing = 4
    case success = 5
}

class WizardViewModel: ObservableObject {
    @Published var currentScreen: WizardScreen = .welcome
    @Published var apiKey: String = ""
    @Published var wifiSSID: String = ""
    @Published var wifiPassword: String = ""

    @Published var devices: [Device] = []
    @Published var selectedDevice: Device?
    @Published var isDeviceConnected: Bool = false

    @Published var isFlashing: Bool = false
    @Published var isComplete: Bool = false
    @Published var progress: Double = 0
    @Published var statusMessage: String = "Ready to flash"
    @Published var logMessages: [String] = []
    @Published var errorMessage: String?

    private var flasherPath: String {
        if let resourcePath = Bundle.main.resourcePath {
            return "\(resourcePath)/esp32flash"
        }
        return "./esp32flash"
    }

    private var firmwareDir: String {
        if let resourcePath = Bundle.main.resourcePath {
            return "\(resourcePath)/firmware"
        }
        return "./firmware"
    }

    init() {
        // Start device monitoring
        refreshDevices()
    }

    func nextScreen() {
        if let nextScreen = WizardScreen(rawValue: currentScreen.rawValue + 1) {
            currentScreen = nextScreen

            // Auto-refresh devices when reaching connect screen
            if currentScreen == .connectDevice {
                startDeviceMonitoring()
            }
        }
    }

    func previousScreen() {
        if currentScreen.rawValue > 0, let prevScreen = WizardScreen(rawValue: currentScreen.rawValue - 1) {
            currentScreen = prevScreen
        }
    }

    func canProceed() -> Bool {
        switch currentScreen {
        case .welcome:
            return true
        case .apiKey:
            return !apiKey.isEmpty && apiKey.hasPrefix("sk-proj-")
        case .wifi:
            return !wifiSSID.isEmpty && !wifiPassword.isEmpty
        case .connectDevice:
            return selectedDevice != nil
        case .flashing:
            return false
        case .success:
            return true
        }
    }

    private var deviceMonitorTimer: Timer?

    private func startDeviceMonitoring() {
        deviceMonitorTimer?.invalidate()
        deviceMonitorTimer = Timer.scheduledTimer(withTimeInterval: 1.0, repeats: true) { [weak self] _ in
            self?.refreshDevices()
        }
    }

    func refreshDevices() {
        let fileManager = FileManager.default
        do {
            let devFiles = try fileManager.contentsOfDirectory(atPath: "/dev")
            devices = devFiles
                .filter { $0.hasPrefix("cu.usbserial-") || $0.hasPrefix("cu.SLAB_USBtoUART") }
                .map { Device(path: "/dev/\($0)") }
                .sorted { $0.path < $1.path }

            if selectedDevice == nil && !devices.isEmpty {
                selectedDevice = devices.first
            }

            isDeviceConnected = !devices.isEmpty
        } catch {
            log("Error scanning devices: \(error.localizedDescription)")
        }
    }

    func startFlashing() {
        guard let device = selectedDevice else {
            errorMessage = "No device selected"
            return
        }

        // Stop device monitoring immediately
        log("Stopping device monitoring...")
        deviceMonitorTimer?.invalidate()
        deviceMonitorTimer = nil

        currentScreen = .flashing
        isFlashing = true
        isComplete = false
        errorMessage = nil
        progress = 0
        logMessages = []

        log("Waiting for serial port to be released...")

        // Wait longer for macOS to fully release the port
        DispatchQueue.global(qos: .userInitiated).async { [weak self] in
            Thread.sleep(forTimeInterval: 2.0)

            DispatchQueue.main.async {
                self?.log("Starting flash process...")
                self?.log("Device: \(device.path)")
                self?.log("Firmware: \(self?.firmwareDir ?? "")")

                if let apiKey = self?.apiKey, !apiKey.isEmpty {
                    self?.log("API Key: Provided")
                }
                if let wifiSSID = self?.wifiSSID, !wifiSSID.isEmpty {
                    self?.log("WiFi: \(wifiSSID)")
                }
            }

            self?.runFlashProcess(device: device.path)
        }
    }

    private func runFlashProcess(device: String) {
        do {
            updateProgress(10, "Connecting to ESP32...")
            try runCommand(flasherPath, args: [
                "--port", device,
                "--erase",
                "--write-parts", firmwareDir,
                "--baud", "115200"
            ])

            updateProgress(90, "Firmware written successfully!")

            if !apiKey.isEmpty || !wifiSSID.isEmpty {
                updateProgress(92, "Writing configuration...")

                let config: [String: Any] = [
                    "openai_api_key": apiKey,
                    "wifi_ssid": wifiSSID,
                    "wifi_password": wifiPassword
                ]

                let configData = try JSONSerialization.data(withJSONObject: config, options: [.prettyPrinted])
                let configPath = "/tmp/ai-stick-config.json"
                try configData.write(to: URL(fileURLWithPath: configPath))

                let binConfigPath = "/tmp/ai-stick-config.bin"
                let paddedData = configData + Data(repeating: 0xFF, count: 0x1000 - configData.count)
                try paddedData.write(to: URL(fileURLWithPath: binConfigPath))

                try runCommand(flasherPath, args: [
                    "--port", device,
                    "--write", binConfigPath,
                    "--offset", "0x670000",
                    "--baud", "115200"
                ])

                updateProgress(98, "Configuration written successfully!")

                try? FileManager.default.removeItem(atPath: configPath)
                try? FileManager.default.removeItem(atPath: binConfigPath)
            }

            updateProgress(100, "Complete!")

            DispatchQueue.main.async {
                self.isFlashing = false
                self.isComplete = true
                self.currentScreen = .success
                self.log("✓ Flash completed successfully!")
            }

        } catch {
            DispatchQueue.main.async {
                self.isFlashing = false
                self.errorMessage = error.localizedDescription
                self.log("✗ Flash failed: \(error.localizedDescription)")
            }
        }
    }

    private func runCommand(_ path: String, args: [String]) throws {
        let process = Process()
        process.executableURL = URL(fileURLWithPath: path)
        process.arguments = args

        let outputPipe = Pipe()
        let errorPipe = Pipe()
        process.standardOutput = outputPipe
        process.standardError = errorPipe

        var errorOutput = ""

        outputPipe.fileHandleForReading.readabilityHandler = { handle in
            let data = handle.availableData
            if !data.isEmpty, let output = String(data: data, encoding: .utf8) {
                let lines = output.split(separator: "\n").map(String.init)
                for line in lines {
                    self.log(line)

                    if line.contains("%") {
                        if let match = line.range(of: "\\d+%", options: .regularExpression) {
                            let percentStr = String(line[match]).dropLast()
                            if let percent = Double(percentStr) {
                                let scaledProgress = 10 + (percent * 0.8)
                                self.updateProgress(scaledProgress, line)
                            }
                        }
                    }
                }
            }
        }

        errorPipe.fileHandleForReading.readabilityHandler = { handle in
            let data = handle.availableData
            if !data.isEmpty, let output = String(data: data, encoding: .utf8) {
                errorOutput += output
                let lines = output.split(separator: "\n").map(String.init)
                for line in lines {
                    self.log("ERROR: \(line)")
                }
            }
        }

        do {
            try process.run()
        } catch {
            throw NSError(domain: "FlashError", code: -1, userInfo: [
                NSLocalizedDescriptionKey: "Failed to start flash command: \(error.localizedDescription)"
            ])
        }

        process.waitUntilExit()

        outputPipe.fileHandleForReading.readabilityHandler = nil
        errorPipe.fileHandleForReading.readabilityHandler = nil

        guard process.terminationStatus == 0 else {
            if errorOutput.isEmpty {
                errorOutput = "Command exited with status \(process.terminationStatus)"
            }
            throw NSError(domain: "FlashError", code: Int(process.terminationStatus), userInfo: [
                NSLocalizedDescriptionKey: "Flash command failed: \(errorOutput)"
            ])
        }
    }

    private func updateProgress(_ value: Double, _ message: String) {
        DispatchQueue.main.async {
            self.progress = value
            self.statusMessage = message
        }
    }

    private func log(_ message: String) {
        DispatchQueue.main.async {
            self.logMessages.append(message)
            print("[Installer] \(message)")
        }
    }

    deinit {
        deviceMonitorTimer?.invalidate()
    }
}

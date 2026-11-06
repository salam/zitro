import Combine
import Foundation

struct Device: Identifiable, Hashable {
    let path: String
    var id: String { path }
    var name: String { (path as NSString).lastPathComponent }
}

enum WizardScreen: Int {
    case welcome, ai, wifi, connect, installing, done
}

/// AI services of the stick's setup page. Values match the firmware's "provider" setting.
enum AIService: String, CaseIterable, Identifiable {
    case openai, anthropic, infomaniak, custom
    var id: String { rawValue }

    var title: String {
        switch self {
        case .openai: return "OpenAI (ChatGPT)"
        case .anthropic: return "Anthropic (Claude)"
        case .infomaniak: return "Infomaniak AI (Switzerland)"
        case .custom: return "Other (OpenAI-compatible)"
        }
    }

    var keySetting: String {
        switch self {
        case .openai: return "apiKey"
        case .anthropic: return "claudeKey"
        case .infomaniak: return "ikKey"
        case .custom: return "customKey"
        }
    }

    var keyPlaceholder: String {
        switch self {
        case .openai: return "sk-..."
        case .anthropic: return "sk-ant-..."
        case .infomaniak: return "Infomaniak API token"
        case .custom: return "API key"
        }
    }

    var keyPage: URL? {
        switch self {
        case .openai: return URL(string: "https://platform.openai.com/api-keys")
        case .anthropic: return URL(string: "https://platform.claude.com/settings/keys")
        case .infomaniak: return URL(string: "https://manager.infomaniak.com/")
        case .custom: return nil
        }
    }
}

final class WizardViewModel: ObservableObject {
    /// Stopped by the app delegate when the app quits.
    static var runningFlasher: Process?

    @Published var screen: WizardScreen = .welcome

    // AI service
    @Published var useAI = true
    @Published var service: AIService = .openai
    @Published var apiKey = ""
    @Published var voiceKey = ""
    @Published var infomaniakProduct = ""
    @Published var customURL = ""
    @Published var customModel = ""

    // WiFi
    @Published var useWiFi = true
    @Published var wifiName = ""
    @Published var wifiPassword = ""

    // Stick
    @Published var devices: [Device] = []
    @Published var selectedDevice: Device?

    // Install
    @Published var isInstalling = false
    @Published var progress = 0.0
    @Published var status = ""
    @Published var log: [String] = []
    @Published var errorMessage: String?
    /// Firmware is installed but the settings didn't make it.
    @Published var settingsWarning: String?
    @Published var settingsWereSent = false
    @Published var sentWiFi = false

    private var scanTimer: Timer?
    private let flasherURL: URL
    private let firmwareDirectory: URL
    private var firmwareWritten = false

    let firmwareVersion: String

    init(flasherURL: URL? = nil, firmwareDirectory: URL? = nil) {
        // Writing to a flasher that just died must end in an error message, not a crash
        signal(SIGPIPE, SIG_IGN)

        self.flasherURL = flasherURL ?? Bundle.main.url(forAuxiliaryExecutable: "esp32flash")
            ?? URL(fileURLWithPath: "esp32flash")
        let firmware = firmwareDirectory ?? Bundle.main.resourceURL?.appendingPathComponent("firmware")
            ?? URL(fileURLWithPath: "firmware")
        self.firmwareDirectory = firmware

        let manifest = (try? Data(contentsOf: firmware.appendingPathComponent("manifest.json")))
            .flatMap { try? JSONSerialization.jsonObject(with: $0) as? [String: Any] }
        firmwareVersion = manifest?["firmware"] as? String ?? "unknown"
    }

    // MARK: Navigation

    func goNext() {
        guard let next = WizardScreen(rawValue: screen.rawValue + 1) else { return }
        show(next)
    }

    func goBack() {
        guard let previous = WizardScreen(rawValue: screen.rawValue - 1) else { return }
        show(previous)
    }

    func skipAI() {
        useAI = false
        show(.wifi)
    }

    func skipWiFi() {
        useWiFi = false
        show(.connect)
    }

    private func show(_ next: WizardScreen) {
        if next == .ai { useAI = true }
        if next == .wifi { useWiFi = true }
        screen = next
        if next == .connect {
            startScanning()
        } else {
            stopScanning()
        }
    }

    // MARK: Validation (same rules as the firmware, which checks again)

    private func trimmed(_ text: String) -> String {
        text.trimmingCharacters(in: .whitespacesAndNewlines)
    }

    private func isValidKey(_ key: String) -> Bool {
        key.utf8.count >= 8 && key.utf8.count <= 300 && !key.contains(" ")
    }

    var keyHint: String? {
        let key = trimmed(apiKey)
        if key.isEmpty { return nil }
        if !isValidKey(key) { return "A key has 8 to 300 characters and no spaces." }
        if service == .openai && !key.hasPrefix("sk-") { return "OpenAI keys usually start with sk-. Please check it." }
        return nil
    }

    var voiceKeyHint: String? {
        let key = trimmed(voiceKey)
        return !key.isEmpty && !isValidKey(key) ? "A key has 8 to 300 characters and no spaces." : nil
    }

    var customURLHint: String? {
        let url = trimmed(customURL)
        return !url.isEmpty && !url.hasPrefix("https://") ? "The address must start with https://" : nil
    }

    var canContinueFromAI: Bool {
        guard isValidKey(trimmed(apiKey)) else { return false }
        guard voiceKeyHint == nil else { return false }
        switch service {
        case .openai, .anthropic: return true
        case .infomaniak: return !trimmed(infomaniakProduct).isEmpty
        case .custom:
            return trimmed(customURL).hasPrefix("https://") && !trimmed(customModel).isEmpty && trimmed(customModel).utf8.count <= 200
        }
    }

    var wifiHint: String? {
        if wifiName.utf8.count > 32 { return "A WiFi name has at most 32 characters." }
        let length = wifiPassword.utf8.count
        if length != 0 && (length < 8 || length > 63) { return "A WiFi password has 8 to 63 characters. Leave it empty for an open network." }
        return nil
    }

    var canContinueFromWiFi: Bool {
        !wifiName.isEmpty && wifiHint == nil
    }

    // MARK: Finding the stick

    private func startScanning() {
        refreshDevices()
        scanTimer?.invalidate()
        scanTimer = Timer.scheduledTimer(withTimeInterval: 1, repeats: true) { [weak self] _ in
            self?.refreshDevices()
        }
    }

    private func stopScanning() {
        scanTimer?.invalidate()
        scanTimer = nil
    }

    func refreshDevices() {
        let prefixes = ["cu.usbserial", "cu.wchusbserial", "cu.SLAB_USBtoUART", "cu.usbmodem"]
        let files = (try? FileManager.default.contentsOfDirectory(atPath: "/dev")) ?? []
        let found = files
            .filter { name in prefixes.contains { name.hasPrefix($0) } }
            .sorted()
            .map { Device(path: "/dev/\($0)") }
        if found != devices { devices = found }
        // The flasher checks the chip before it writes, so a wrong pick can't do harm
        if let selected = selectedDevice, found.contains(selected) { return }
        selectedDevice = found.first
    }

    // MARK: Install

    /// One-line JSON for the firmware. Empty when there's nothing to send.
    /// Fields left out keep what is already stored on the stick.
    private func settingsJSON() -> Data? {
        var settings: [String: String] = [:]
        if useAI {
            settings["provider"] = service.rawValue
            settings[service.keySetting] = trimmed(apiKey)
            if service != .openai, !trimmed(voiceKey).isEmpty { settings["apiKey"] = trimmed(voiceKey) }
            if service == .infomaniak { settings["ikProduct"] = trimmed(infomaniakProduct) }
            if service == .custom {
                settings["customUrl"] = trimmed(customURL)
                settings["customModel"] = trimmed(customModel)
            }
        }
        if useWiFi {
            settings["ssid"] = wifiName
            settings["password"] = wifiPassword
        }
        if settings.isEmpty { return nil }
        return try? JSONSerialization.data(withJSONObject: settings)
    }

    func startInstall() {
        guard let device = selectedDevice else { return }
        stopScanning()

        screen = .installing
        isInstalling = true
        progress = 0
        status = "Getting ready"
        log = []
        errorMessage = nil
        settingsWarning = nil
        firmwareWritten = false

        let settings = settingsJSON()
        settingsWereSent = settings != nil && useAI
        sentWiFi = settings != nil && useWiFi
        append("Port: \(device.path)")
        append("Firmware: \(firmwareVersion)")
        append(settings == nil ? "Settings: stay as they are" : "Settings: will be sent over USB")

        DispatchQueue.global(qos: .userInitiated).async {
            self.runFlasher(port: device.path, settings: settings)
        }
    }

    private func runFlasher(port: String, settings: Data?) {
        let process = Process()
        process.executableURL = flasherURL
        process.arguments = ["--port", port, "--install", firmwareDirectory.path]
        if settings != nil { process.arguments?.append("--provision") }

        let output = Pipe()
        let input = Pipe()
        process.standardOutput = output
        process.standardError = FileHandle.nullDevice
        process.standardInput = input

        // Pipes deliver chunks: only whole lines are decoded, so nothing is cut mid-character
        let lines = DispatchQueue(label: "installer.flasher-output")
        var pending = Data()
        var failure: (code: String, message: String)?
        let consume = { (chunk: Data) in
            pending.append(chunk)
            while let newline = pending.firstIndex(of: 0x0a) {
                let line = String(decoding: pending[pending.startIndex..<newline], as: UTF8.self)
                pending = Data(pending[pending.index(after: newline)...])
                if let parsed = self.handle(line: line) { failure = parsed }
            }
        }
        output.fileHandleForReading.readabilityHandler = { handle in
            let chunk = handle.availableData
            lines.sync { consume(chunk) }
        }

        do {
            try process.run()
        } catch {
            output.fileHandleForReading.readabilityHandler = nil
            finish(failure: ("start-failed", "The installer could not start its flasher. Please download it again."))
            return
        }
        WizardViewModel.runningFlasher = process

        // Over a pipe only: secrets never go into arguments or files
        if let settings = settings { input.fileHandleForWriting.write(settings) }
        input.fileHandleForWriting.closeFile()

        process.waitUntilExit()
        WizardViewModel.runningFlasher = nil

        output.fileHandleForReading.readabilityHandler = nil
        let rest = output.fileHandleForReading.readDataToEndOfFile()
        lines.sync { consume(rest + Data([0x0a])) }

        var outcome: (code: String, message: String)?
        lines.sync { outcome = failure }
        if process.terminationStatus != 0 && outcome == nil {
            outcome = ("transfer-failed", "The flasher stopped unexpectedly. Please try again.")
        }
        finish(failure: outcome)
    }

    /// Returns the failure if this line reports one.
    private func handle(line: String) -> (code: String, message: String)? {
        if line.hasPrefix("@@PROGRESS ") {
            let parts = line.dropFirst(11).split(separator: " ", maxSplits: 1).map(String.init)
            guard let percent = parts.first.flatMap(Double.init) else { return nil }
            let text = parts.count > 1 ? parts[1] : ""
            if percent >= 94 { DispatchQueue.main.async { self.firmwareWritten = true } }
            DispatchQueue.main.async {
                self.progress = percent
                self.status = text
            }
            return nil
        }
        if line.hasPrefix("@@ERROR ") {
            let parts = line.dropFirst(8).split(separator: " ", maxSplits: 1).map(String.init)
            guard parts.count == 2 else { return nil }
            append("Failed: \(parts[0])")
            return (parts[0], parts[1])
        }
        if !line.isEmpty { append(line) }
        return nil
    }

    private func finish(failure: (code: String, message: String)?) {
        DispatchQueue.main.async {
            self.isInstalling = false
            if let failure = failure {
                if failure.code.hasPrefix("settings-") && self.firmwareWritten {
                    self.settingsWarning = failure.message
                    self.screen = .done
                } else {
                    self.errorMessage = failure.message
                }
                return
            }
            self.progress = 100
            self.clearSecrets()
            self.screen = .done
        }
    }

    private func clearSecrets() {
        apiKey = ""
        voiceKey = ""
        wifiPassword = ""
    }

    func retry() {
        startInstall()
    }

    func backToSettings() {
        errorMessage = nil
        settingsWarning = nil
        show(.ai)
    }

    func backToConnect() {
        errorMessage = nil
        show(.connect)
    }

    private func append(_ line: String) {
        DispatchQueue.main.async {
            self.log.append(line)
        }
    }

    deinit {
        scanTimer?.invalidate()
    }
}

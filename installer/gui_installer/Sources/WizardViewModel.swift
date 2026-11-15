import Combine
import Foundation
import IOKit
import IOKit.serial

/// Which board sits behind a USB serial port, told by the USB vendor ID of the device
enum BoardKind {
    /// M5StickC Plus2: an ESP32 behind a WCH USB serial chip
    case plus2
    /// M5Stack StickS3: an ESP32-S3 on its own USB (Espressif). Zitro can't be installed on it yet
    case stickS3

    static let wchVendor = 0x1A86
    static let espressifVendor = 0x303A

    init?(vendor: Int) {
        switch vendor {
        case BoardKind.wchVendor: self = .plus2
        case BoardKind.espressifVendor: self = .stickS3
        default: return nil
        }
    }
}

struct Device: Identifiable, Hashable {
    let path: String
    let kind: BoardKind
    var id: String { path }
    var name: String { (path as NSString).lastPathComponent }
}

/// Serial ports of USB devices we recognise. Any other port is left out: a device that doesn't
/// take data made the old installer hang, and Bluetooth or built-in ports are never a stick.
func findBoards() -> [Device] {
    guard let matching = IOServiceMatching(kIOSerialBSDServiceValue) else { return [] }
    var iterator: io_iterator_t = 0
    // 0 is the default main port (kIOMainPortDefault needs macOS 12)
    guard IOServiceGetMatchingServices(0, matching, &iterator) == KERN_SUCCESS else { return [] }
    defer { IOObjectRelease(iterator) }

    var boards: [Device] = []
    while case let service = IOIteratorNext(iterator), service != 0 {
        defer { IOObjectRelease(service) }
        guard let path = IORegistryEntryCreateCFProperty(service, kIOCalloutDeviceKey as CFString, kCFAllocatorDefault, 0)?
                .takeRetainedValue() as? String,
              let vendor = IORegistryEntrySearchCFProperty(service, kIOServicePlane, "idVendor" as CFString, kCFAllocatorDefault,
                                                           IOOptionBits(kIORegistryIterateRecursively | kIORegistryIterateParents)) as? Int,
              let kind = BoardKind(vendor: vendor) else { continue }
        boards.append(Device(path: path, kind: kind))
    }
    return boards.sorted { $0.path < $1.path }
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
    /// A StickS3 is plugged in and no stick Zitro can be installed on
    @Published var onlyStickS3 = false

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
    /// The install in progress. A run ends once: by the flasher's exit or by the watchdog, whichever comes first
    private var currentRun: UUID?

    /// The flasher has its own time limits. These catch what it can't, like a USB port the system
    /// never finishes opening. Its first line after opening the port must come within `firstAnswer`;
    /// after that it is never silent for long, except while the stick boots for the settings (up to 120 s).
    private static let firstAnswerSeconds: TimeInterval = 20
    private static let silenceSeconds: TimeInterval = 150

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
        let boards = findBoards()
        let found = boards.filter { $0.kind == .plus2 }
        let stickS3 = found.isEmpty && boards.contains { $0.kind == .stickS3 }
        if found != devices { devices = found }
        if stickS3 != onlyStickS3 { onlyStickS3 = stickS3 }
        // The flasher still checks the chip before it writes
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

        let run = UUID()
        currentRun = run
        DispatchQueue.global(qos: .userInitiated).async {
            self.runFlasher(run: run, port: device.path, settings: settings)
        }
    }

    private func runFlasher(run: UUID, port: String, settings: Data?) {
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
        var lastLine = Date()
        var portOpen = false
        // Given up on by the watchdog: whatever it says later must not reach the next try
        var abandoned = false
        let consume = { (chunk: Data) in
            guard !abandoned else { return }
            pending.append(chunk)
            while let newline = pending.firstIndex(of: 0x0a) {
                let line = String(decoding: pending[pending.startIndex..<newline], as: UTF8.self)
                pending = Data(pending[pending.index(after: newline)...])
                lastLine = Date()
                // The flasher reports progress only once it has the port open
                if line.hasPrefix("@@PROGRESS ") { portOpen = true }
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
            finish(run: run, failure: ("start-failed", "The installer could not start its flasher. Please download it again."))
            return
        }
        WizardViewModel.runningFlasher = process

        // Over a pipe only: secrets never go into arguments or files
        if let settings = settings { input.fileHandleForWriting.write(settings) }
        input.fileHandleForWriting.closeFile()

        let watchdog = DispatchSource.makeTimerSource(queue: lines)
        watchdog.schedule(deadline: .now() + 1, repeating: 1)
        watchdog.setEventHandler {
            let limit = portOpen ? WizardViewModel.silenceSeconds : WizardViewModel.firstAnswerSeconds
            guard Date().timeIntervalSince(lastLine) > limit else { return }
            watchdog.cancel()
            abandoned = true
            self.append("Failed: not-responding")
            process.terminate()
            // Don't wait for the flasher to end: one stuck in the system may only end once the stick is unplugged
            self.finish(run: run, failure: ("not-responding", "The stick doesn't respond. Unplug it, plug it back in and try again."))
        }
        watchdog.resume()

        process.waitUntilExit()
        lines.sync { watchdog.cancel() }
        if WizardViewModel.runningFlasher === process { WizardViewModel.runningFlasher = nil }

        output.fileHandleForReading.readabilityHandler = nil
        let rest = output.fileHandleForReading.readDataToEndOfFile()
        lines.sync { consume(rest + Data([0x0a])) }

        var outcome: (code: String, message: String)?
        var gaveUp = false
        lines.sync { outcome = failure; gaveUp = abandoned }
        if gaveUp { return }
        if process.terminationStatus != 0 && outcome == nil {
            outcome = ("transfer-failed", "The flasher stopped unexpectedly. Please try again.")
        }
        finish(run: run, failure: outcome)
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

    private func finish(run: UUID, failure: (code: String, message: String)?) {
        DispatchQueue.main.async {
            guard self.currentRun == run else { return }
            self.currentRun = nil
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

import AppKit
import SwiftUI

// MARK: - Look (zitro.spot palette)

private extension Color {
    static let paper = Color(red: 0xf4 / 255, green: 0xef / 255, blue: 0xe6 / 255)
    static let card = Color(red: 0xfb / 255, green: 0xf8 / 255, blue: 0xf1 / 255)
    static let ink = Color(red: 0x2b / 255, green: 0x27 / 255, blue: 0x21 / 255)
    static let inkSoft = Color(red: 0x6b / 255, green: 0x64 / 255, blue: 0x59 / 255)
    static let citrus = Color(red: 0xf2 / 255, green: 0xb7 / 255, blue: 0x05 / 255)
    static let citrusDeep = Color(red: 0x8f / 255, green: 0x6a / 255, blue: 0x00 / 255)
    static let leaf = Color(red: 0x4f / 255, green: 0x76 / 255, blue: 0x35 / 255)
    static let alert = Color(red: 0xb0 / 255, green: 0x3a / 255, blue: 0x2e / 255)
}

private struct CitrusButton: ButtonStyle {
    @Environment(\.isEnabled) private var isEnabled

    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .font(.system(size: 15, weight: .semibold))
            .foregroundColor(isEnabled ? .ink : .inkSoft)
            .padding(.horizontal, 24)
            .frame(height: 44)
            .background(Capsule().fill(isEnabled ? Color.citrus : Color.ink.opacity(0.08)))
            .opacity(configuration.isPressed ? 0.8 : 1)
    }
}

private struct QuietButton: ButtonStyle {
    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .font(.system(size: 14, weight: .medium))
            .foregroundColor(.inkSoft)
            .padding(.horizontal, 16)
            .frame(height: 44)
            .background(Capsule().stroke(Color.ink.opacity(0.18)))
            .contentShape(Capsule())
            .opacity(configuration.isPressed ? 0.6 : 1)
    }
}

private struct Hint: View {
    let text: String
    var color: Color = .citrusDeep

    var body: some View {
        Text(text)
            .font(.system(size: 12))
            .foregroundColor(color)
            .fixedSize(horizontal: false, vertical: true)
    }
}

private struct FieldLabel: View {
    let text: String

    init(_ text: String) { self.text = text }

    var body: some View {
        Text(text)
            .font(.system(size: 13, weight: .medium))
            .foregroundColor(.ink)
    }
}

private struct ScreenTitle: View {
    let title: String
    let subtitle: String

    var body: some View {
        VStack(spacing: 6) {
            Text(title)
                .font(.system(size: 26, weight: .bold, design: .rounded))
                .foregroundColor(.ink)
                .multilineTextAlignment(.center)
            Text(subtitle)
                .font(.system(size: 14))
                .foregroundColor(.inkSoft)
                .multilineTextAlignment(.center)
                .fixedSize(horizontal: false, vertical: true)
        }
    }
}

private struct BrandMark: View {
    var size: CGFloat = 72

    var body: some View {
        Group {
            if let path = Bundle.main.path(forResource: "zitro-mark", ofType: "png"), let image = NSImage(contentsOfFile: path) {
                Image(nsImage: image).resizable().scaledToFit()
            } else {
                Circle().fill(Color.citrus)
            }
        }
        .frame(width: size, height: size)
    }
}

// MARK: - Wizard

struct WizardView: View {
    @StateObject private var model = WizardViewModel()

    var body: some View {
        WizardScreens(model: model)
    }
}

struct WizardScreens: View {
    @ObservedObject var model: WizardViewModel

    var body: some View {
        ZStack {
            Color.paper.edgesIgnoringSafeArea(.all)
            content
                .padding(.horizontal, 44)
                .padding(.top, 34)
                .padding(.bottom, 28)
        }
    }

    @ViewBuilder
    private var content: some View {
        switch model.screen {
        case .welcome: WelcomeScreen(model: model)
        case .ai: AIScreen(model: model)
        case .wifi: WiFiScreen(model: model)
        case .connect: ConnectScreen(model: model)
        case .installing: InstallingScreen(model: model)
        case .done: DoneScreen(model: model)
        }
    }
}

// MARK: - Welcome

struct WelcomeScreen: View {
    @ObservedObject var model: WizardViewModel

    var body: some View {
        VStack(spacing: 22) {
            Spacer()
            BrandMark(size: 120)
            ScreenTitle(title: "Install Zitro on your stick", subtitle: "The little AI that fits in a pocket. Ready in about a minute.")

            VStack(alignment: .leading, spacing: 12) {
                Bullet(icon: "bolt.fill", text: "Puts the newest Zitro on your M5StickC Plus2")
                Bullet(icon: "key.fill", text: "Your keys and WiFi go straight to the stick, nowhere else")
                Bullet(icon: "arrow.triangle.2.circlepath", text: "Already using Zitro? Your settings stay")
            }
            .padding(.top, 6)

            Spacer()
            Text("Installer \(BuildInfo.version) · Firmware \(model.firmwareVersion)")
                .font(.system(size: 11))
                .foregroundColor(.inkSoft)
            Button("Get started") { model.goNext() }
                .buttonStyle(CitrusButton())
                .keyboardShortcut(.defaultAction)
        }
    }
}

private struct Bullet: View {
    let icon: String
    let text: String

    var body: some View {
        HStack(spacing: 12) {
            Image(systemName: icon)
                .foregroundColor(.citrusDeep)
                .frame(width: 22)
            Text(text)
                .font(.system(size: 14))
                .foregroundColor(.ink)
        }
    }
}

// MARK: - AI service

struct AIScreen: View {
    @ObservedObject var model: WizardViewModel

    var body: some View {
        VStack(spacing: 18) {
            ScreenTitle(title: "Which AI should Zitro use?", subtitle: "Your key is stored on the stick only. You can change it any time on the stick's setup page.")

            VStack(alignment: .leading, spacing: 12) {
                FieldLabel("AI service")
                Picker("", selection: $model.service) {
                    ForEach(AIService.allCases) { service in
                        Text(service.title).tag(service)
                    }
                }
                .labelsHidden()

                if model.service == .infomaniak {
                    FieldLabel("Product ID")
                    TextField("12345", text: $model.infomaniakProduct)
                        .textFieldStyle(RoundedBorderTextFieldStyle())
                }
                if model.service == .custom {
                    FieldLabel("Address")
                    TextField("https://example.com/v1", text: $model.customURL)
                        .textFieldStyle(RoundedBorderTextFieldStyle())
                    if let hint = model.customURLHint { Hint(text: hint) }
                    FieldLabel("Model")
                    TextField("Model name", text: $model.customModel)
                        .textFieldStyle(RoundedBorderTextFieldStyle())
                }

                FieldLabel("API key")
                SecureField(model.service.keyPlaceholder, text: $model.apiKey)
                    .textFieldStyle(RoundedBorderTextFieldStyle())
                if let hint = model.keyHint { Hint(text: hint) }

                if let page = model.service.keyPage {
                    Button("Get a key") { NSWorkspace.shared.open(page) }
                        .buttonStyle(LinkButtonStyle())
                        .font(.system(size: 13))
                }

                if model.service != .openai {
                    FieldLabel("OpenAI key for voice (optional)")
                    SecureField("sk-...", text: $model.voiceKey)
                        .textFieldStyle(RoundedBorderTextFieldStyle())
                    Hint(text: model.voiceKeyHint ?? "Listening, speaking and pictures use OpenAI. Without this key the stick can't hear you.")
                }
            }
            .frame(maxWidth: 440)

            Spacer()
            HStack(spacing: 10) {
                Button("Back") { model.goBack() }.buttonStyle(QuietButton())
                Spacer()
                Button("Set up later") { model.skipAI() }.buttonStyle(QuietButton())
                Button("Continue") { model.goNext() }
                    .buttonStyle(CitrusButton())
                    .keyboardShortcut(.defaultAction)
                    .disabled(!model.canContinueFromAI)
            }
        }
    }
}

// MARK: - WiFi

struct WiFiScreen: View {
    @ObservedObject var model: WizardViewModel

    var body: some View {
        VStack(spacing: 18) {
            ScreenTitle(title: "Which WiFi should Zitro join?", subtitle: "Stored on the stick only. It adds this network to the ones it already knows.")

            VStack(alignment: .leading, spacing: 12) {
                FieldLabel("Network name")
                TextField("My WiFi", text: $model.wifiName)
                    .textFieldStyle(RoundedBorderTextFieldStyle())
                FieldLabel("Password")
                SecureField("Leave empty for an open network", text: $model.wifiPassword)
                    .textFieldStyle(RoundedBorderTextFieldStyle())
                if let hint = model.wifiHint { Hint(text: hint) }
                Hint(text: "The stick only reaches 2.4 GHz networks. Most home routers offer one.", color: .inkSoft)
            }
            .frame(maxWidth: 440)

            Spacer()
            HStack(spacing: 10) {
                Button("Back") { model.goBack() }.buttonStyle(QuietButton())
                Spacer()
                Button("Set up later") { model.skipWiFi() }.buttonStyle(QuietButton())
                Button("Continue") { model.goNext() }
                    .buttonStyle(CitrusButton())
                    .keyboardShortcut(.defaultAction)
                    .disabled(!model.canContinueFromWiFi)
            }
        }
    }
}

// MARK: - Connect

struct ConnectScreen: View {
    @ObservedObject var model: WizardViewModel

    private var plugImage: NSImage? {
        Bundle.main.path(forResource: "plug-in", ofType: "png").flatMap { NSImage(contentsOfFile: $0) }
    }

    var body: some View {
        VStack(spacing: 18) {
            ScreenTitle(title: "Plug in your stick", subtitle: "Connect the M5StickC Plus2 to this Mac with a USB-C data cable.")

            if let image = plugImage {
                Image(nsImage: image)
                    .resizable()
                    .scaledToFit()
                    .frame(height: 210)
                    .clipShape(RoundedRectangle(cornerRadius: 14))
            }

            if model.devices.isEmpty {
                VStack(spacing: 8) {
                    HStack(spacing: 8) {
                        ProgressView().scaleEffect(0.6).frame(width: 18, height: 18)
                        Text("Looking for your stick...")
                            .font(.system(size: 14, weight: .medium))
                            .foregroundColor(.ink)
                    }
                    Hint(text: "Nothing yet? Some USB-C cables only charge: try another one, and turn the stick on.", color: .inkSoft)
                }
            } else {
                HStack(spacing: 8) {
                    Image(systemName: "checkmark.circle.fill").foregroundColor(.leaf)
                    Text("USB device found")
                        .font(.system(size: 14, weight: .medium))
                        .foregroundColor(.leaf)
                }
                if model.devices.count > 1 {
                    Picker("", selection: $model.selectedDevice) {
                        ForEach(model.devices) { device in
                            Text(device.name).tag(device as Device?)
                        }
                    }
                    .labelsHidden()
                    .frame(maxWidth: 340)
                    Hint(text: "Several USB devices found. Pick the stick. Other boards are recognised and left untouched.", color: .inkSoft)
                }
                Hint(text: "Close Arduino and any serial monitors, and keep the stick plugged in until it's done.", color: .inkSoft)
            }

            Spacer()
            HStack(spacing: 10) {
                Button("Back") { model.goBack() }.buttonStyle(QuietButton())
                Spacer()
                Button("Install Zitro") { model.startInstall() }
                    .buttonStyle(CitrusButton())
                    .keyboardShortcut(.defaultAction)
                    .disabled(model.selectedDevice == nil)
            }
        }
    }
}

// MARK: - Installing

struct InstallingScreen: View {
    @ObservedObject var model: WizardViewModel
    @State private var showLog = false

    var body: some View {
        VStack(spacing: 20) {
            Spacer()
            if let error = model.errorMessage {
                Image(systemName: "exclamationmark.triangle.fill")
                    .font(.system(size: 44))
                    .foregroundColor(.alert)
                ScreenTitle(title: "That didn't work", subtitle: error)
                Text("Your stick is fine. It can always be installed again.")
                    .font(.system(size: 13, weight: .medium))
                    .foregroundColor(.ink)
            } else {
                BrandMark(size: 72)
                ScreenTitle(title: "Installing Zitro", subtitle: "Keep the stick plugged in.")
                ProgressView(value: model.progress, total: 100)
                    .accentColor(.citrus)
                    .frame(maxWidth: 400)
                Text(model.status)
                    .font(.system(size: 13))
                    .foregroundColor(.inkSoft)
            }

            Button(showLog ? "Hide details" : "Show details") { showLog.toggle() }
                .buttonStyle(LinkButtonStyle())
                .font(.system(size: 12))

            if showLog {
                ScrollView {
                    Text(model.log.joined(separator: "\n"))
                        .font(.system(size: 11, design: .monospaced))
                        .foregroundColor(.ink)
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .padding(10)
                }
                .frame(height: 140)
                .background(RoundedRectangle(cornerRadius: 8).fill(Color.card))
                Button("Copy details") {
                    NSPasteboard.general.clearContents()
                    NSPasteboard.general.setString(model.log.joined(separator: "\n"), forType: .string)
                }
                .buttonStyle(LinkButtonStyle())
                .font(.system(size: 12))
            }

            Spacer()
            if model.errorMessage != nil {
                HStack(spacing: 10) {
                    Button("Back") { model.backToConnect() }.buttonStyle(QuietButton())
                    Spacer()
                    Button("Try again") { model.retry() }
                        .buttonStyle(CitrusButton())
                        .keyboardShortcut(.defaultAction)
                }
            }
        }
    }
}

// MARK: - Done

struct DoneScreen: View {
    @ObservedObject var model: WizardViewModel

    private var isReady: Bool { model.settingsWarning == nil && model.settingsWereSent && model.sentWiFi }

    var body: some View {
        VStack(spacing: 20) {
            Spacer()
            Image(systemName: "checkmark.circle.fill")
                .font(.system(size: 64))
                .foregroundColor(.leaf)

            if isReady {
                ScreenTitle(title: "Zitro is ready!", subtitle: "Your stick restarts and joins your WiFi.")
                VStack(alignment: .leading, spacing: 14) {
                    Step(number: 1, text: "Unplug the stick, or keep it charging")
                    Step(number: 2, text: "Press the M5 button and ask a question")
                }
            } else {
                ScreenTitle(title: "Zitro is installed!", subtitle: "If you've used Zitro before, your settings are still there and you're done. If not, finish on the stick:")
                VStack(alignment: .leading, spacing: 14) {
                    Step(number: 1, text: "On your phone or Mac, join the WiFi called “zitro”")
                    Step(number: 2, text: "Open zitro.local in the browser")
                    Step(number: 3, text: "Add your WiFi and your AI key")
                }
            }

            if let warning = model.settingsWarning {
                VStack(spacing: 8) {
                    Text("Your settings were not saved")
                        .font(.system(size: 13, weight: .semibold))
                        .foregroundColor(.alert)
                    Hint(text: warning, color: .alert)
                    Button("Change settings and try again") { model.backToSettings() }
                        .buttonStyle(LinkButtonStyle())
                        .font(.system(size: 13))
                }
                .padding(.top, 4)
            }

            Spacer()
            Button("Done") { NSApplication.shared.terminate(nil) }
                .buttonStyle(CitrusButton())
                .keyboardShortcut(.defaultAction)
        }
    }
}

private struct Step: View {
    let number: Int
    let text: String

    var body: some View {
        HStack(spacing: 12) {
            Text("\(number)")
                .font(.system(size: 14, weight: .bold))
                .foregroundColor(.ink)
                .frame(width: 28, height: 28)
                .background(Circle().fill(Color.citrus))
            Text(text)
                .font(.system(size: 14))
                .foregroundColor(.ink)
        }
    }
}

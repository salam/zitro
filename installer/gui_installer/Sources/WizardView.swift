import SwiftUI
import AppKit

// MARK: - Selectable Text View

struct SelectableTextView: NSViewRepresentable {
    let text: String

    func makeNSView(context: Context) -> NSScrollView {
        // Use the proper NSTextView.scrollableTextView() factory —
        // this correctly wires up the text container, layout manager,
        // and scroll view so text actually renders.
        let scrollView = NSTextView.scrollableTextView()
        scrollView.hasVerticalScroller = true
        scrollView.autohidesScrollers = true
        scrollView.borderType = .noBorder
        scrollView.drawsBackground = true
        scrollView.backgroundColor = NSColor.black

        if let textView = scrollView.documentView as? NSTextView {
            textView.isEditable = false
            textView.isSelectable = true
            textView.drawsBackground = true
            textView.backgroundColor = NSColor.black
            textView.textColor = NSColor.white
            textView.font = NSFont.monospacedSystemFont(ofSize: 11, weight: .regular)
            textView.textContainerInset = NSSize(width: 8, height: 8)
            textView.isRichText = false
            textView.allowsUndo = false
        }

        return scrollView
    }

    func updateNSView(_ scrollView: NSScrollView, context: Context) {
        guard let textView = scrollView.documentView as? NSTextView else { return }

        textView.string = text
        textView.textColor = NSColor.white
        textView.font = NSFont.monospacedSystemFont(ofSize: 11, weight: .regular)

        // Scroll to bottom
        textView.scrollToEndOfDocument(nil)
    }
}

struct WizardView: View {
    @StateObject private var viewModel = WizardViewModel()

    var body: some View {
        VStack(spacing: 0) {
            HeaderView()

            ZStack {
                switch viewModel.currentScreen {
                case .welcome:
                    WelcomeScreen(viewModel: viewModel)
                case .apiKey:
                    APIKeyScreen(viewModel: viewModel)
                case .wifi:
                    WiFiScreen(viewModel: viewModel)
                case .connectDevice:
                    ConnectDeviceScreen(viewModel: viewModel)
                case .flashing:
                    FlashingScreen(viewModel: viewModel)
                case .success:
                    SuccessScreen(viewModel: viewModel)
                }
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity)
        }
        .background(Color(NSColor.windowBackgroundColor))
    }
}

struct HeaderView: View {
    var body: some View {
        ZStack {
            LinearGradient(
                colors: [Color(red: 0.29, green: 0.56, blue: 0.89), Color(red: 0.20, green: 0.40, blue: 0.70)],
                startPoint: .topLeading,
                endPoint: .bottomTrailing
            )
            .frame(height: 100)

            VStack(spacing: 4) {
                Image(systemName: "flame.fill")
                    .font(.system(size: 32))
                    .foregroundColor(.white)

                Text("Firebird AI Stick Installer")
                    .font(.system(size: 24, weight: .bold))
                    .foregroundColor(.white)
            }
        }
    }
}

// MARK: - Welcome Screen

struct WelcomeScreen: View {
    @ObservedObject var viewModel: WizardViewModel

    var body: some View {
        VStack(spacing: 30) {
            Spacer()

            Image(systemName: "flame.circle.fill")
                .font(.system(size: 80))
                .foregroundColor(Color(red: 0.29, green: 0.56, blue: 0.89))

            Text("Welcome to Firebird AI Stick")
                .font(.system(size: 28, weight: .bold))

            Text("Your pocket-sized AI companion")
                .font(.system(size: 18))
                .foregroundColor(.secondary)

            VStack(alignment: .leading, spacing: 12) {
                FeatureRow(icon: "mic.fill", text: "Voice-activated AI assistant")
                FeatureRow(icon: "photo.fill", text: "Image generation and analysis")
                FeatureRow(icon: "wifi", text: "WiFi connectivity")
                FeatureRow(icon: "brain.head.profile", text: "Powered by OpenAI GPT")
            }
            .padding()

            Spacer()

            VStack(spacing: 4) {
                Text("Version \(BuildInfo.version) (Build \(BuildInfo.buildNumber))")
                    .font(.system(size: 11))
                    .foregroundColor(.secondary)
                Text("Built: \(BuildInfo.buildTime)")
                    .font(.system(size: 10))
                    .foregroundColor(.secondary)
            }
            .padding(.bottom, 8)

            Button(action: { viewModel.nextScreen() }) {
                HStack {
                    Text("Get Started")
                        .font(.headline)
                        .foregroundColor(.white)
                    Image(systemName: "arrow.right")
                        .foregroundColor(.white)
                }
                .frame(maxWidth: .infinity)
                .frame(height: 50)
                .background(Color(red: 0.29, green: 0.56, blue: 0.89))
                .cornerRadius(8)
            }
            .buttonStyle(PlainButtonStyle())
            .padding(.horizontal, 40)
            .padding(.bottom, 40)
        }
    }
}

struct FeatureRow: View {
    let icon: String
    let text: String

    var body: some View {
        HStack(spacing: 12) {
            Image(systemName: icon)
                .foregroundColor(Color(red: 0.29, green: 0.56, blue: 0.89))
                .frame(width: 24)
            Text(text)
                .font(.system(size: 16))
        }
    }
}

// MARK: - API Key Screen

struct APIKeyScreen: View {
    @ObservedObject var viewModel: WizardViewModel

    var body: some View {
        VStack(spacing: 24) {
            Spacer()

            Image(systemName: "key.fill")
                .font(.system(size: 60))
                .foregroundColor(Color(red: 0.29, green: 0.56, blue: 0.89))

            Text("OpenAI API Key")
                .font(.system(size: 24, weight: .bold))

            Text("Enter your OpenAI API key to enable AI features")
                .font(.system(size: 16))
                .foregroundColor(.secondary)
                .multilineTextAlignment(.center)
                .padding(.horizontal, 40)

            VStack(alignment: .leading, spacing: 8) {
                TextField("sk-proj-...", text: $viewModel.apiKey)
                    .textFieldStyle(.roundedBorder)
                    .font(.system(.body, design: .monospaced))
                    .frame(maxWidth: 400)

                if !viewModel.apiKey.isEmpty && !viewModel.apiKey.hasPrefix("sk-proj-") {
                    HStack(spacing: 4) {
                        Image(systemName: "exclamationmark.triangle.fill")
                            .foregroundColor(.orange)
                            .font(.system(size: 12))
                        Text("API key should start with 'sk-proj-'")
                            .font(.system(size: 12))
                            .foregroundColor(.orange)
                    }
                }

                Button(action: {
                    if let url = URL(string: "https://platform.openai.com/settings/organization/api-keys") {
                        NSWorkspace.shared.open(url)
                    }
                }) {
                    HStack(spacing: 4) {
                        Image(systemName: "link")
                            .font(.system(size: 12))
                        Text("Get your API key from OpenAI")
                            .font(.system(size: 14))
                    }
                }
                .buttonStyle(.borderless)
            }

            Spacer()

            HStack(spacing: 16) {
                Button(action: { viewModel.previousScreen() }) {
                    HStack {
                        Image(systemName: "arrow.left")
                            .foregroundColor(.secondary)
                        Text("Back")
                            .foregroundColor(.secondary)
                    }
                    .frame(width: 120, height: 50)
                    .background(Color(NSColor.controlBackgroundColor))
                    .cornerRadius(8)
                }
                .buttonStyle(PlainButtonStyle())

                Button(action: { viewModel.nextScreen() }) {
                    HStack {
                        Text("Continue")
                            .foregroundColor(.white)
                        Image(systemName: "arrow.right")
                            .foregroundColor(.white)
                    }
                    .frame(maxWidth: .infinity)
                    .frame(height: 50)
                    .background(viewModel.canProceed() ? Color(red: 0.29, green: 0.56, blue: 0.89) : Color.gray)
                    .cornerRadius(8)
                }
                .buttonStyle(PlainButtonStyle())
                .disabled(!viewModel.canProceed())
            }
            .padding(.horizontal, 40)
            .padding(.bottom, 40)
        }
    }
}

// MARK: - WiFi Screen

struct WiFiScreen: View {
    @ObservedObject var viewModel: WizardViewModel

    var body: some View {
        VStack(spacing: 24) {
            Spacer()

            Image(systemName: "wifi")
                .font(.system(size: 60))
                .foregroundColor(Color(red: 0.29, green: 0.56, blue: 0.89))

            Text("WiFi Configuration")
                .font(.system(size: 24, weight: .bold))

            Text("Configure your device's WiFi connection")
                .font(.system(size: 16))
                .foregroundColor(.secondary)

            VStack(alignment: .leading, spacing: 16) {
                VStack(alignment: .leading, spacing: 8) {
                    Text("Network Name (SSID)")
                        .font(.system(size: 14, weight: .medium))
                    TextField("Enter WiFi network name", text: $viewModel.wifiSSID)
                        .textFieldStyle(.roundedBorder)
                        .frame(maxWidth: 400)
                }

                VStack(alignment: .leading, spacing: 8) {
                    Text("WiFi Password")
                        .font(.system(size: 14, weight: .medium))
                    SecureField("Enter WiFi password", text: $viewModel.wifiPassword)
                        .textFieldStyle(.roundedBorder)
                        .frame(maxWidth: 400)
                        .disabled(viewModel.wifiSSID.isEmpty)
                }

                HStack(spacing: 8) {
                    Image(systemName: "info.circle")
                        .foregroundColor(.secondary)
                        .font(.system(size: 14))
                    Text("Tip: Click the WiFi icon in the macOS menu bar, then 'WiFi Settings' to view your current network and copy its password")
                        .font(.system(size: 13))
                        .foregroundColor(.secondary)
                        .fixedSize(horizontal: false, vertical: true)
                }
                .padding()
                .background(Color(NSColor.controlBackgroundColor))
                .cornerRadius(8)
                .frame(maxWidth: 450)
            }

            Spacer()

            HStack(spacing: 16) {
                Button(action: { viewModel.previousScreen() }) {
                    HStack {
                        Image(systemName: "arrow.left")
                            .foregroundColor(.secondary)
                        Text("Back")
                            .foregroundColor(.secondary)
                    }
                    .frame(width: 120, height: 50)
                    .background(Color(NSColor.controlBackgroundColor))
                    .cornerRadius(8)
                }
                .buttonStyle(PlainButtonStyle())

                Button(action: { viewModel.nextScreen() }) {
                    HStack {
                        Text("Continue")
                            .foregroundColor(.white)
                        Image(systemName: "arrow.right")
                            .foregroundColor(.white)
                    }
                    .frame(maxWidth: .infinity)
                    .frame(height: 50)
                    .background(viewModel.canProceed() ? Color(red: 0.29, green: 0.56, blue: 0.89) : Color.gray)
                    .cornerRadius(8)
                }
                .buttonStyle(PlainButtonStyle())
                .disabled(!viewModel.canProceed())
            }
            .padding(.horizontal, 40)
            .padding(.bottom, 40)
        }
    }
}

// MARK: - Connect Device Screen

struct ConnectDeviceScreen: View {
    @ObservedObject var viewModel: WizardViewModel

    var body: some View {
        VStack(spacing: 24) {
            Spacer()

            if let plugInImage = NSImage(contentsOfFile: Bundle.main.resourcePath.map { "\($0)/plug-in.png" } ?? "") {
                Image(nsImage: plugInImage)
                    .resizable()
                    .scaledToFit()
                    .frame(width: 200, height: 200)
            } else {
                Image(systemName: "cable.connector")
                    .font(.system(size: 60))
                    .foregroundColor(Color(red: 0.29, green: 0.56, blue: 0.89))
            }

            Text("Connect Your Device")
                .font(.system(size: 24, weight: .bold))

            Text("Connect your M5StickC Plus2 via USB cable")
                .font(.system(size: 16))
                .foregroundColor(.secondary)

            if viewModel.devices.isEmpty {
                HStack(spacing: 8) {
                    ProgressView()
                        .scaleEffect(0.7)
                    Text("Waiting for device...")
                        .font(.system(size: 16))
                        .foregroundColor(.secondary)
                }
                .padding()
                .background(Color(NSColor.controlBackgroundColor))
                .cornerRadius(8)

                VStack(spacing: 8) {
                    HStack(spacing: 4) {
                        Image(systemName: "info.circle")
                            .foregroundColor(.secondary)
                            .font(.system(size: 14))
                        Text("Make sure:")
                            .font(.system(size: 13, weight: .medium))
                            .foregroundColor(.secondary)
                    }
                    VStack(alignment: .leading, spacing: 4) {
                        Text("• ESP32 is connected via USB")
                            .font(.system(size: 13))
                            .foregroundColor(.secondary)
                        Text("• USB drivers are installed")
                            .font(.system(size: 13))
                            .foregroundColor(.secondary)
                        Text("• Device appears as /dev/cu.usbserial-*")
                            .font(.system(size: 13))
                            .foregroundColor(.secondary)
                    }
                }
                .padding()
                .background(Color(NSColor.controlBackgroundColor))
                .cornerRadius(8)
                .frame(maxWidth: 450)
            } else {
                HStack(spacing: 8) {
                    Image(systemName: "checkmark.circle.fill")
                        .foregroundColor(.green)
                        .font(.system(size: 20))
                    Text("Device detected!")
                        .font(.system(size: 16, weight: .medium))
                        .foregroundColor(.green)
                }
                .padding()
                .background(Color.green.opacity(0.1))
                .cornerRadius(8)

                VStack(alignment: .leading, spacing: 8) {
                    Text("Select Serial Port:")
                        .font(.system(size: 14, weight: .medium))

                    Picker("", selection: $viewModel.selectedDevice) {
                        ForEach(viewModel.devices) { device in
                            Text(device.displayName).tag(device as Device?)
                        }
                    }
                    .pickerStyle(.menu)
                    .frame(maxWidth: 400)

                    if let device = viewModel.selectedDevice {
                        HStack(spacing: 4) {
                            Image(systemName: "checkmark.circle.fill")
                                .foregroundColor(.green)
                                .font(.system(size: 12))
                            Text("Selected: \(device.path)")
                                .font(.system(size: 13, design: .monospaced))
                                .foregroundColor(.green)
                        }
                    }
                }
            }

            Spacer()

            HStack(spacing: 16) {
                Button(action: { viewModel.previousScreen() }) {
                    HStack {
                        Image(systemName: "arrow.left")
                            .foregroundColor(.secondary)
                        Text("Back")
                            .foregroundColor(.secondary)
                    }
                    .frame(width: 120, height: 50)
                    .background(Color(NSColor.controlBackgroundColor))
                    .cornerRadius(8)
                }
                .buttonStyle(PlainButtonStyle())

                Button(action: { viewModel.startFlashing() }) {
                    HStack {
                        Text("Install Firmware")
                            .foregroundColor(.white)
                        Image(systemName: "bolt.fill")
                            .foregroundColor(.white)
                    }
                    .frame(maxWidth: .infinity)
                    .frame(height: 50)
                    .background(viewModel.canProceed() ? Color(red: 0.29, green: 0.56, blue: 0.89) : Color.gray)
                    .cornerRadius(8)
                }
                .buttonStyle(PlainButtonStyle())
                .disabled(!viewModel.canProceed())
            }
            .padding(.horizontal, 40)
            .padding(.bottom, 40)
        }
    }
}

// MARK: - Flashing Screen

struct FlashingScreen: View {
    @ObservedObject var viewModel: WizardViewModel
    @State private var isHoveringCopyButton = false

    var body: some View {
        VStack(spacing: 24) {
            Spacer()

            if viewModel.isFlashing {
                ProgressView()
                    .scaleEffect(1.5)
                    .padding()
            } else if viewModel.errorMessage != nil {
                Image(systemName: "exclamationmark.triangle.fill")
                    .font(.system(size: 60))
                    .foregroundColor(.orange)
            }

            Text(viewModel.isFlashing ? "Installing Firmware" : "Installation Error")
                .font(.system(size: 24, weight: .bold))

            SwiftUI.ProgressView(value: viewModel.progress, total: 100) {
                Text(viewModel.statusMessage)
                    .font(.system(size: 14))
                    .foregroundColor(.secondary)
            }
            .frame(maxWidth: 400)

            if !viewModel.logMessages.isEmpty {
                VStack(alignment: .leading, spacing: 0) {
                    HStack {
                        Text("Installation Log")
                            .font(.system(size: 12, weight: .medium))
                            .foregroundColor(.secondary)
                        Spacer()
                        Button(action: {
                            let logText = viewModel.logMessages.joined(separator: "\n")
                            let pasteboard = NSPasteboard.general
                            pasteboard.clearContents()
                            pasteboard.setString(logText, forType: .string)
                        }) {
                            HStack(spacing: 4) {
                                Image(systemName: "doc.on.clipboard")
                                    .font(.system(size: 11))
                                Text("Copy log")
                                    .font(.system(size: 11))
                            }
                            .foregroundColor(isHoveringCopyButton ? Color(red: 0.20, green: 0.40, blue: 0.70) : Color(red: 0.29, green: 0.56, blue: 0.89))
                            .padding(.horizontal, 6)
                            .padding(.vertical, 3)
                            .background(isHoveringCopyButton ? Color(red: 0.29, green: 0.56, blue: 0.89).opacity(0.1) : Color.clear)
                            .cornerRadius(4)
                        }
                        .buttonStyle(PlainButtonStyle())
                        .onHover { hovering in
                            isHoveringCopyButton = hovering
                        }
                        .help("Copy log to clipboard")
                    }
                    .padding(8)
                    .background(Color(NSColor.controlBackgroundColor))

                    SelectableTextView(
                        text: viewModel.logMessages.joined(separator: "\n")
                    )
                    .frame(height: 200)
                }
                .frame(maxWidth: 500)
                .overlay(
                    RoundedRectangle(cornerRadius: 6)
                        .stroke(Color(NSColor.separatorColor), lineWidth: 1)
                )
            }

            if let error = viewModel.errorMessage {
                VStack(alignment: .leading, spacing: 8) {
                    HStack {
                        Image(systemName: "exclamationmark.octagon.fill")
                            .foregroundColor(.red)
                        Text("Error Details:")
                            .font(.system(size: 14, weight: .medium))
                            .foregroundColor(.red)
                    }
                    Text(error)
                        .font(.system(size: 13))
                        .foregroundColor(.red)
                }
                .padding()
                .frame(maxWidth: 500)
                .background(Color.red.opacity(0.1))
                .cornerRadius(8)
            }

            Spacer()
        }
        .padding()
    }
}

// MARK: - Success Screen

struct SuccessScreen: View {
    @ObservedObject var viewModel: WizardViewModel

    var body: some View {
        VStack(spacing: 30) {
            Spacer()

            Image(systemName: "checkmark.circle.fill")
                .font(.system(size: 80))
                .foregroundColor(.green)

            Text("Success!")
                .font(.system(size: 28, weight: .bold))

            Text("Your Firebird AI Stick is ready")
                .font(.system(size: 18))
                .foregroundColor(.secondary)

            VStack(alignment: .leading, spacing: 16) {
                InstructionRow(number: "1", text: "Disconnect the USB cable from your computer")
                InstructionRow(number: "2", text: "Press the reset button on your device")
                InstructionRow(number: "3", text: "Wait for the device to boot up")
                InstructionRow(number: "4", text: "Press and hold the button to start talking")
            }
            .padding()

            Text("Now go out and play!")
                .font(.system(size: 20, weight: .semibold))
                .foregroundColor(Color(red: 0.29, green: 0.56, blue: 0.89))

            Spacer()

            Button(action: { NSApplication.shared.terminate(nil) }) {
                HStack {
                    Text("Close")
                        .font(.headline)
                        .foregroundColor(.white)
                    Image(systemName: "xmark.circle")
                        .foregroundColor(.white)
                }
                .frame(maxWidth: .infinity)
                .frame(height: 50)
                .background(Color(red: 0.29, green: 0.56, blue: 0.89))
                .cornerRadius(8)
            }
            .buttonStyle(PlainButtonStyle())
            .padding(.horizontal, 40)
            .padding(.bottom, 40)
        }
    }
}

struct InstructionRow: View {
    let number: String
    let text: String

    var body: some View {
        HStack(spacing: 12) {
            Text(number)
                .font(.system(size: 18, weight: .bold))
                .foregroundColor(.white)
                .frame(width: 32, height: 32)
                .background(Color(red: 0.29, green: 0.56, blue: 0.89))
                .cornerRadius(16)
            Text(text)
                .font(.system(size: 16))
        }
    }
}

struct WizardView_Previews: PreviewProvider {
    static var previews: some View {
        WizardView()
    }
}

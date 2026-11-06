// swift-tools-version:5.5
import PackageDescription

let package = Package(
    name: "esp32flash",
    platforms: [
        .macOS(.v10_13)
    ],
    targets: [
        .executableTarget(
            name: "esp32flash",
            dependencies: [],
            path: "Sources"
        )
    ]
)

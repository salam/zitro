// swift-tools-version:5.5
import PackageDescription

let package = Package(
    name: "FirebirdInstaller",
    platforms: [
        .macOS(.v11)
    ],
    targets: [
        .executableTarget(
            name: "FirebirdInstaller",
            dependencies: [],
            path: "Sources"
        )
    ]
)

// swift-tools-version: 5.6
// The swift-tools-version declares the minimum version of Swift required to build this package.

import PackageDescription

let package = Package(
    name: "PopCameraDevice",
	platforms: [
		.macOS(.v10_14), .iOS(.v13)
	],
    products: [
        // Products define the executables and libraries a package produces, and make them visible to other packages.
        .library(
            name: "PopCameraDevice",
            targets: ["PopCameraDevice"]),
    ],
    dependencies: [
        // Dependencies declare other packages that this package depends on.
        // .package(url: /* package url */, from: "1.0.0"),
    ],
    targets: [
        // Targets are the basic building blocks of a package. A target can define a module or a test suite.
        // Targets can depend on other targets in this package, and on products in packages this package depends on.
		.binaryTarget(
			name: "PopCameraDevice",
			url: "https://github.com/NewChromantics/PopCameraDevice/releases/download/v2.3.0/PopCameraDevice.xcframework.zip",
			checksum: "9ae79ab55d83eda553f78dbf5dd4cfeb24025b7bec61e0118a136bd04349ff92"
		),
        
    ]
)

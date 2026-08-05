// KNTKTACompanionApp.swift — iOS Companion App Entry Point
// Lightweight SwiftUI app that joins Ableton Link and exposes
// device parameters to KNTKTA over OSC.

import SwiftUI

@main
struct KNTKTACompanionApp: App {
    @StateObject private var linkManager = LinkManager()
    @StateObject private var paramBridge = ParameterBridge()

    var body: some Scene {
        WindowGroup {
            ContentView()
                .environmentObject(linkManager)
                .environmentObject(paramBridge)
        }
    }
}

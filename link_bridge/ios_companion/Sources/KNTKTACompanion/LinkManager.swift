// LinkManager.swift — Ableton Link session manager for iOS
// Uses ABLLink framework (Ableton Link SDK for iOS)

import Foundation
import Combine

/// Manages the Ableton Link session state on iOS.
/// Requires ABLLink.framework linked in the Xcode project.
class LinkManager: ObservableObject {

    @Published var isEnabled: Bool = false
    @Published var tempo: Double = 120.0
    @Published var peers: Int = 0
    @Published var isPlaying: Bool = false
    @Published var beat: Double = 0.0
    @Published var phase: Double = 0.0

    // ABLLinkRef would be used here with the native SDK
    // private var linkRef: ABLLinkRef?

    private var timer: Timer?
    let quantum: Double = 4.0

    init() {
        // In production: linkRef = ABLLinkNew(tempo)
        // For simulator/preview builds we use a software clock
        startSoftwareClock()
    }

    func enable(_ enabled: Bool) {
        isEnabled = enabled
        // ABLLinkSetActive(linkRef, enabled)
    }

    func setTempo(_ bpm: Double) {
        tempo = max(20, min(300, bpm))
        // ABLLinkSetTempo(linkRef, bpm, hostTime)
        objectWillChange.send()
    }

    func setPlaying(_ playing: Bool) {
        isPlaying = playing
        // ABLLinkSetIsPlayingAndRequestBeatAtTime(linkRef, playing, hostTime, 0, quantum)
    }

    private func startSoftwareClock() {
        let startTime = Date()
        timer = Timer.scheduledTimer(withTimeInterval: 0.02, repeats: true) { [weak self] _ in
            guard let self = self else { return }
            let elapsed = Date().timeIntervalSince(startTime)
            self.beat = elapsed * self.tempo / 60.0
            self.phase = self.beat.truncatingRemainder(dividingBy: self.quantum)
        }
    }

    deinit {
        timer?.invalidate()
        // ABLLinkDelete(linkRef)
    }
}

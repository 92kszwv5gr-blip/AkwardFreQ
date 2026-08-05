// ParameterBridge.swift — Exposes app parameters to KNTKTA over OSC

import Foundation
import Network
import Combine

/// Represents one parameter exposed to KNTKTA
struct AppParameter: Identifiable {
    let id: Int
    var name: String
    var oscPath: String
    var value: Double       // 0.0 – 1.0 normalised
    var displayValue: String { String(format: "%.2f", value) }
}

/// Manages OSC communication and parameter state
class ParameterBridge: ObservableObject {

    @Published var parameters: [AppParameter] = []
    @Published var kntktaHost: String = "192.168.1.100"
    @Published var kntktaPort: UInt16 = 8765
    @Published var isConnected: Bool = false

    private var udpConnection: NWConnection?
    private var listenerQueue = DispatchQueue(label: "kntkta.osc.listener")

    init() {
        setupDefaultParameters()
        startOscListener()
    }

    // MARK: - Default Parameters

    private func setupDefaultParameters() {
        parameters = [
            AppParameter(id: 0, name: "Volume",   oscPath: "/kntkta/param/volume",  value: 0.75),
            AppParameter(id: 1, name: "Filter",   oscPath: "/kntkta/param/filter",  value: 0.5),
            AppParameter(id: 2, name: "Reverb",   oscPath: "/kntkta/param/reverb",  value: 0.3),
            AppParameter(id: 3, name: "Delay",    oscPath: "/kntkta/param/delay",   value: 0.2),
            AppParameter(id: 4, name: "Attack",   oscPath: "/kntkta/param/attack",  value: 0.1),
            AppParameter(id: 5, name: "Release",  oscPath: "/kntkta/param/release", value: 0.4),
            AppParameter(id: 6, name: "Pitch",    oscPath: "/kntkta/param/pitch",   value: 0.5),
            AppParameter(id: 7, name: "Pan",      oscPath: "/kntkta/param/pan",     value: 0.5),
        ]
    }

    // MARK: - OSC Send

    func sendParameter(_ param: AppParameter) {
        let msg = buildOscMessage(address: param.oscPath, value: Float(param.value))
        sendUdp(data: msg)
    }

    func sendRegistration() {
        // Announce ourselves to KNTKTA
        let msg = buildOscMessage(address: "/kntkta/register",
                                  deviceId: UIDevice.current.identifierForVendor?.uuidString ?? "unknown",
                                  name: UIDevice.current.name)
        sendUdp(data: msg)
    }

    func updateParameter(id: Int, value: Double) {
        if let idx = parameters.firstIndex(where: { $0.id == id }) {
            parameters[idx].value = max(0, min(1, value))
            sendParameter(parameters[idx])
        }
    }

    // MARK: - OSC Receive

    private func startOscListener() {
        let params = NWParameters.udp
        guard let port = NWEndpoint.Port(rawValue: 8766) else { return }
        let listener = try? NWListener(using: params, on: port)
        listener?.newConnectionHandler = { [weak self] conn in
            self?.handleIncomingConnection(conn)
        }
        listener?.start(queue: listenerQueue)
    }

    private func handleIncomingConnection(_ conn: NWConnection) {
        conn.receiveMessage { [weak self] data, _, _, _ in
            guard let data = data, let self = self else { return }
            self.parseOscMessage(data: data)
        }
        conn.start(queue: listenerQueue)
    }

    private func parseOscMessage(data: Data) {
        // Simplified OSC parse: extract address and float value
        guard let addressEnd = data.firstIndex(of: 0) else { return }
        let address = String(data: data[0..<addressEnd], encoding: .utf8) ?? ""

        // Find float value after type tag
        guard data.count >= 12 else { return }
        let valueOffset = data.count - 4
        let valueData = data[valueOffset...]
        let value = valueData.withUnsafeBytes { ptr -> Float in
            let raw = ptr.load(as: UInt32.self).bigEndian
            return Float(bitPattern: raw)
        }

        DispatchQueue.main.async {
            if let idx = self.parameters.firstIndex(where: { $0.oscPath == address }) {
                self.parameters[idx].value = Double(value)
            }
        }
    }

    // MARK: - UDP

    private func sendUdp(data: Data) {
        let connection = NWConnection(
            host: NWEndpoint.Host(kntktaHost),
            port: NWEndpoint.Port(rawValue: kntktaPort)!,
            using: .udp
        )
        connection.start(queue: listenerQueue)
        connection.send(content: data, completion: .idempotent)
    }

    // MARK: - OSC Encoding

    private func buildOscMessage(address: String, value: Float) -> Data {
        var data = Data()
        let addrBytes = encodeOscString(address)
        let typeBytes = encodeOscString(",f")
        var floatBE = value.bitPattern.bigEndian
        let floatBytes = Data(bytes: &floatBE, count: 4)
        data.append(contentsOf: addrBytes)
        data.append(contentsOf: typeBytes)
        data.append(contentsOf: floatBytes)
        return data
    }

    private func buildOscMessage(address: String, deviceId: String, name: String) -> Data {
        var data = Data()
        data.append(contentsOf: encodeOscString(address))
        data.append(contentsOf: encodeOscString(",ss"))
        data.append(contentsOf: encodeOscString(deviceId))
        data.append(contentsOf: encodeOscString(name))
        return data
    }

    private func encodeOscString(_ s: String) -> [UInt8] {
        var bytes = Array(s.utf8) + [0]
        let pad = (4 - bytes.count % 4) % 4
        bytes.append(contentsOf: [UInt8](repeating: 0, count: pad == 0 ? 4 : pad))
        return bytes
    }
}

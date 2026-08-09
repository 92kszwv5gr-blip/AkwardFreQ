// ContentView.swift — Main UI for the KNTKTA iOS Companion App

import SwiftUI

struct ContentView: View {
    @EnvironmentObject var linkManager: LinkManager
    @EnvironmentObject var paramBridge: ParameterBridge

    var body: some View {
        NavigationView {
            ScrollView {
                VStack(spacing: 20) {
                    // Link Status
                    LinkStatusCard()
                        .environmentObject(linkManager)

                    // KNTKTA Connection
                    ConnectionCard()
                        .environmentObject(paramBridge)

                    // Parameter Controls
                    ParameterGrid()
                        .environmentObject(paramBridge)
                }
                .padding()
            }
            .navigationTitle("KNTKTA Companion")
            .navigationBarTitleDisplayMode(.inline)
        }
    }
}

// MARK: - Link Status Card

struct LinkStatusCard: View {
    @EnvironmentObject var linkManager: LinkManager

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack {
                Image(systemName: "link.circle.fill")
                    .foregroundColor(linkManager.isEnabled ? .green : .gray)
                Text("Ableton Link")
                    .font(.headline)
                Spacer()
                Toggle("", isOn: $linkManager.isEnabled)
                    .labelsHidden()
                    .onChange(of: linkManager.isEnabled) { linkManager.enable($0) }
            }

            HStack(spacing: 24) {
                VStack {
                    Text("\(Int(linkManager.tempo))")
                        .font(.system(size: 36, weight: .bold, design: .monospaced))
                    Text("BPM").font(.caption).foregroundColor(.secondary)
                }
                VStack {
                    Text("\(linkManager.peers)")
                        .font(.system(size: 36, weight: .bold, design: .monospaced))
                    Text("Peers").font(.caption).foregroundColor(.secondary)
                }
                VStack {
                    Text(String(format: "%.2f", linkManager.phase))
                        .font(.system(size: 36, weight: .bold, design: .monospaced))
                    Text("Phase").font(.caption).foregroundColor(.secondary)
                }
            }

            // Beat visualiser
            BeatBar(phase: linkManager.phase, quantum: 4.0)
        }
        .padding()
        .background(Color(.secondarySystemBackground))
        .cornerRadius(12)
    }
}

// MARK: - Beat Bar

struct BeatBar: View {
    let phase: Double
    let quantum: Double

    var body: some View {
        GeometryReader { geo in
            ZStack(alignment: .leading) {
                RoundedRectangle(cornerRadius: 4)
                    .fill(Color(.tertiarySystemBackground))
                RoundedRectangle(cornerRadius: 4)
                    .fill(Color.accentColor)
                    .frame(width: geo.size.width * CGFloat(phase / quantum))
            }
        }
        .frame(height: 8)
    }
}

// MARK: - Connection Card

struct ConnectionCard: View {
    @EnvironmentObject var paramBridge: ParameterBridge

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack {
                Image(systemName: "wifi")
                    .foregroundColor(paramBridge.isConnected ? .green : .orange)
                Text("KNTKTA Host")
                    .font(.headline)
            }
            HStack {
                TextField("IP Address", text: $paramBridge.kntktaHost)
                    .textFieldStyle(.roundedBorder)
                    .keyboardType(.numbersAndPunctuation)
                Button("Connect") {
                    paramBridge.sendRegistration()
                }
                .buttonStyle(.borderedProminent)
            }
        }
        .padding()
        .background(Color(.secondarySystemBackground))
        .cornerRadius(12)
    }
}

// MARK: - Parameter Grid

struct ParameterGrid: View {
    @EnvironmentObject var paramBridge: ParameterBridge
    let columns = [GridItem(.flexible()), GridItem(.flexible())]

    var body: some View {
        VStack(alignment: .leading) {
            Text("Parameters")
                .font(.headline)
            LazyVGrid(columns: columns, spacing: 12) {
                ForEach(paramBridge.parameters.indices, id: \.self) { idx in
                    ParameterKnob(parameter: $paramBridge.parameters[idx]) { param in
                        paramBridge.sendParameter(param)
                    }
                }
            }
        }
    }
}

// MARK: - Parameter Knob

struct ParameterKnob: View {
    @Binding var parameter: AppParameter
    var onChanged: (AppParameter) -> Void

    var body: some View {
        VStack(spacing: 8) {
            Text(parameter.name)
                .font(.caption)
                .foregroundColor(.secondary)

            ZStack {
                Circle()
                    .stroke(Color(.tertiarySystemBackground), lineWidth: 8)
                Circle()
                    .trim(from: 0, to: CGFloat(parameter.value))
                    .stroke(Color.accentColor, style: StrokeStyle(lineWidth: 8, lineCap: .round))
                    .rotationEffect(.degrees(-90))
                Text(parameter.displayValue)
                    .font(.system(.caption, design: .monospaced))
            }
            .frame(width: 60, height: 60)
            .gesture(DragGesture(minimumDistance: 0)
                .onChanged { drag in
                    let delta = -drag.translation.height / 200.0
                    parameter.value = max(0, min(1, parameter.value + delta))
                    onChanged(parameter)
                }
            )
        }
        .padding()
        .background(Color(.secondarySystemBackground))
        .cornerRadius(12)
    }
}

struct ContentView_Previews: PreviewProvider {
    static var previews: some View {
        ContentView()
            .environmentObject(LinkManager())
            .environmentObject(ParameterBridge())
    }
}

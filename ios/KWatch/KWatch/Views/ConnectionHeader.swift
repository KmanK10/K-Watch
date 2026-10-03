import SwiftUI

struct ConnectionHeader: View {
    @EnvironmentObject private var model: AppModel

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack(spacing: 12) {
                Circle()
                    .fill(dotColor)
                    .frame(width: 10, height: 10)
                VStack(alignment: .leading, spacing: 2) {
                    Text(title)
                        .font(.headline)
                    Text(subtitle)
                        .font(.caption)
                        .foregroundStyle(.secondary)
                        .lineLimit(1)
                }
                Spacer(minLength: 8)
                Button(action: model.findWatch) {
                    Image(systemName: "wave.3.right")
                        .font(.body.weight(.semibold))
                        .frame(width: 36, height: 36)
                        .background(Theme.card, in: Circle())
                }
                .disabled(model.link != .connected || model.isFinding)
                .accessibilityLabel("Find my watch")
                Button {
                    model.showLog = true
                } label: {
                    Image(systemName: "text.alignleft")
                        .font(.body.weight(.semibold))
                        .frame(width: 36, height: 36)
                        .background(Theme.card, in: Circle())
                }
                .accessibilityLabel("Debug log")
            }
            if let banner = model.banner {
                Text(banner)
                    .font(.caption)
                    .foregroundStyle(Theme.accent)
                    .onTapGesture { model.banner = nil }
            }
        }
        .padding(.horizontal, 20)
        .padding(.top, 8)
        .padding(.bottom, 10)
    }

    private var title: String {
        if let findNote = model.findNote { return findNote }
        switch model.link {
        case .bluetoothOff: return "Bluetooth is off"
        case .unauthorized: return "Bluetooth access needed"
        case .searching: return "Searching"
        case .connected:
            if let firmware = model.hello?.firmware, !firmware.isEmpty {
                return firmware
            }
            return "Connected"
        }
    }

    private var subtitle: String {
        switch model.link {
        case .bluetoothOff:
            return "Turn Bluetooth on to reach the watch."
        case .unauthorized:
            return "Allow Bluetooth for K-Watch in Settings."
        case .searching:
            return "Looking for your watch."
        case .connected:
            return model.hello == nil ? "Waiting for the watch." : "Connected"
        }
    }

    private var dotColor: Color {
        switch model.link {
        case .connected: return Theme.goal
        case .searching: return Theme.accent
        default: return .gray
        }
    }
}

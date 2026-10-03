import SwiftUI

struct DebugLogView: View {
    @EnvironmentObject private var model: AppModel

    var body: some View {
        ScrollViewReader { proxy in
            ScrollView {
                LazyVStack(alignment: .leading, spacing: 8) {
                    if model.log.isEmpty {
                        Text("Messages to and from the watch show up here.")
                            .foregroundStyle(.secondary)
                            .padding(.top, 24)
                    }
                    ForEach(model.log) { line in
                        VStack(alignment: .leading, spacing: 2) {
                            Text("\(stamp(line.date))  \(line.kind.rawValue)")
                                .font(.caption2.monospaced())
                                .foregroundStyle(.secondary)
                            Text(line.text)
                                .font(.caption.monospaced())
                                .foregroundStyle(color(line.kind))
                                .textSelection(.enabled)
                        }
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .id(line.id)
                    }
                }
                .padding(16)
            }
            .background(Color.black)
            .onChange(of: model.log.last?.id) { _, id in
                guard let id else { return }
                withAnimation { proxy.scrollTo(id, anchor: .bottom) }
            }
        }
    }

    private func stamp(_ date: Date) -> String {
        Self.formatter.string(from: date)
    }

    private func color(_ kind: LogLine.Kind) -> Color {
        switch kind {
        case .out: return Theme.accent
        case .inn: return .white
        case .info: return .secondary
        }
    }

    private static let formatter: DateFormatter = {
        let formatter = DateFormatter()
        formatter.dateFormat = "HH:mm:ss"
        return formatter
    }()
}

import Charts
import SwiftUI

struct HealthView: View {
    @EnvironmentObject private var model: AppModel

    var body: some View {
        Group {
            if let health = model.health {
                chart(health)
            } else {
                ContentUnavailableView {
                    Label("No step history yet", systemImage: "figure.walk")
                } description: {
                    Text(model.link == .connected
                         ? "Reading steps from the watch."
                         : "Step history appears when the watch is connected.")
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity)
            }
        }
        .background(Color.black)
        .onAppear { Task { await model.refreshHealth() } }
        .refreshable { await model.refreshHealth() }
    }

    private func chart(_ health: HealthSnapshot) -> some View {
        let bars = bars(for: health)
        let upper = max(health.goal, bars.compactMap(\.steps).max() ?? 0, 1)
        return ScrollView {
            VStack(alignment: .leading, spacing: 16) {
                Card {
                    VStack(alignment: .leading, spacing: 8) {
                        Text(health.date)
                            .font(.caption)
                            .foregroundStyle(.secondary)
                        Text(Format.steps(health.today))
                            .font(.system(size: 48, weight: .light, design: .rounded))
                        Text("of \(Format.steps(health.goal))")
                            .foregroundStyle(.secondary)
                        ProgressView(value: progress(health))
                            .tint(health.today >= health.goal ? Theme.goal : Theme.accent)
                    }
                }
                HStack(spacing: 10) {
                    average("7-day avg", health.average(finishedDays: 7))
                    average("30-day avg", health.average(finishedDays: 30))
                }
                Card {
                    VStack(alignment: .leading, spacing: 8) {
                        Text("Last 30 days")
                            .font(.headline)
                        Chart {
                            ForEach(bars) { bar in
                                BarMark(
                                    x: .value("Day", bar.date, unit: .day),
                                    y: .value("Steps", bar.steps ?? 0)
                                )
                                .foregroundStyle(color(for: bar, goal: health.goal))
                            }
                            RuleMark(y: .value("Goal", health.goal))
                                .foregroundStyle(Theme.goal.opacity(0.9))
                                .lineStyle(StrokeStyle(lineWidth: 1, dash: [4, 3]))
                        }
                        .chartYScale(domain: 0 ... upper)
                        .chartXAxis {
                            AxisMarks(values: .stride(by: .day, count: 7)) { _ in
                                AxisGridLine()
                                AxisValueLabel(format: .dateTime.day())
                            }
                        }
                        .frame(height: 200)
                        Text("Missing bars are days the watch has no count for. The line is the step goal.")
                            .font(.caption)
                            .foregroundStyle(.secondary)
                    }
                }
            }
            .padding(20)
        }
        .aboveTabBar()
        .background(Color.black)
    }

    private func average(_ title: String, _ value: Int?) -> some View {
        Card {
            VStack(alignment: .leading, spacing: 4) {
                Text(title)
                    .font(.caption)
                    .foregroundStyle(.secondary)
                Text(value.map(Format.steps) ?? "—")
                    .font(.title2.weight(.semibold).monospacedDigit())
                    .foregroundStyle(Theme.accent)
            }
        }
    }

    private func progress(_ health: HealthSnapshot) -> Double {
        guard health.goal > 0 else { return 0 }
        return min(1, Double(health.today) / Double(health.goal))
    }

    private func color(for bar: StepBar, goal: Int) -> Color {
        guard let steps = bar.steps else { return Theme.noData }
        return steps >= goal ? Theme.goal : Theme.accent
    }

    /// Today plus the previous 29 days. `days[0]` on the watch is yesterday.
    private func bars(for health: HealthSnapshot) -> [StepBar] {
        let formatter = DateFormatter()
        formatter.calendar = Calendar(identifier: .gregorian)
        formatter.locale = Locale(identifier: "en_US_POSIX")
        formatter.dateFormat = "yyyy-MM-dd"
        formatter.timeZone = .current
        let today = formatter.date(from: health.date) ?? Calendar.current.startOfDay(for: Date())
        return (0 ..< 30).reversed().map { daysAgo in
            let steps: Int?
            if daysAgo == 0 {
                steps = health.today
            } else if daysAgo - 1 < health.days.count {
                steps = health.days[daysAgo - 1]
            } else {
                steps = nil
            }
            let date = Calendar.current.date(byAdding: .day, value: -daysAgo, to: today) ?? today
            return StepBar(daysAgo: daysAgo, steps: steps, date: date)
        }
    }
}

private struct StepBar: Identifiable {
    var daysAgo: Int
    var steps: Int?
    var date: Date
    var id: Int { daysAgo }
}

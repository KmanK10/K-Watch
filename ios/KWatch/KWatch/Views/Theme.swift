import SwiftUI

enum Theme {
    static let accent = Color(red: 1, green: 95.0 / 255, blue: 31.0 / 255)
    static let card = Color(red: 0x30 / 255, green: 0x30 / 255, blue: 0x30 / 255)
    static let precip = Color(red: 0x42 / 255, green: 0xA5 / 255, blue: 0xF5 / 255)
    static let goal = Color(red: 0x2E / 255, green: 0xBD / 255, blue: 0x59 / 255)
    static let noData = Color(red: 0x5A / 255, green: 0x5A / 255, blue: 0x5A / 255)
}

extension View {
    /// The floating tab bar draws over tab content, so lists need extra room to scroll clear of it.
    func aboveTabBar() -> some View {
        padding(.bottom, 92)
    }
}

struct Card<Content: View>: View {
    let content: Content

    init(@ViewBuilder content: () -> Content) {
        self.content = content()
    }

    var body: some View {
        content
            .padding(16)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(Theme.card, in: RoundedRectangle(cornerRadius: 12, style: .continuous))
    }
}

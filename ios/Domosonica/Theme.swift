//
//  Theme.swift
//  Domosonica — la misma paleta del sitio público: tinta / madera / onda.
//
//  Tipografía: SF Pro (pesos livianos, tracking ancho en labels) + SF Mono
//  para todo dato físico. Deliberadamente sin serif.
//

import SwiftUI

enum Theme {
    // MARK: paleta (idéntica a sitio/index.html)
    static let ink      = Color(red: 0.039, green: 0.051, blue: 0.071)  // #0a0d12
    static let ink2     = Color(red: 0.067, green: 0.086, blue: 0.122)  // #11161f
    static let line     = Color(red: 0.133, green: 0.165, blue: 0.220)  // #222a38
    static let air      = Color(red: 0.914, green: 0.898, blue: 0.855)  // #e9e5da
    static let body     = Color(red: 0.718, green: 0.741, blue: 0.792)  // #b7bdca
    static let muted    = Color(red: 0.553, green: 0.580, blue: 0.651)  // #8d94a6
    static let wood     = Color(red: 0.788, green: 0.627, blue: 0.420)  // #c9a06b
    static let wave     = Color(red: 0.435, green: 0.878, blue: 0.808)  // #6fe0ce
    static let waveHot  = Color(red: 0.918, green: 1.000, blue: 0.973)  // #eafff8
    static let alert    = Color(red: 0.880, green: 0.490, blue: 0.436)  // rojo apagado

    // MARK: tipografía
    /// Título de sección: liviano y grande (eco del display ultralight del sitio)
    static func display(_ size: CGFloat) -> Font {
        .system(size: size, weight: .light, design: .default)
    }
    /// Label técnico: chico, caps, tracking ancho (se aplica .tracking aparte)
    static func label(_ size: CGFloat = 11) -> Font {
        .system(size: size, weight: .semibold, design: .default)
    }
    /// Datos físicos: mono siempre
    static func mono(_ size: CGFloat, weight: Font.Weight = .regular) -> Font {
        .system(size: size, weight: weight, design: .monospaced)
    }
}

// MARK: - componentes compartidos de estilo

/// Tarjeta estándar: panel ink2 con hairline
struct Card: ViewModifier {
    func body(content: Content) -> some View {
        content
            .padding(14)
            .background(Theme.ink2)
            .overlay(RoundedRectangle(cornerRadius: 10).stroke(Theme.line, lineWidth: 1))
            .clipShape(RoundedRectangle(cornerRadius: 10))
    }
}

extension View {
    func card() -> some View { modifier(Card()) }
}

/// Eyebrow de sección: caps madera + hairline, como en el sitio
struct SectionLabel: View {
    let text: String
    var body: some View {
        HStack(spacing: 10) {
            Text(text.uppercased())
                .font(Theme.label())
                .tracking(2.6)
                .foregroundStyle(Theme.wood)
            Rectangle().fill(Theme.line).frame(height: 1)
        }
    }
}

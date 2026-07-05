//
//  Components.swift
//  Domosonica — piezas de UI compartidas: header, VU, chips, banner.
//

import SwiftUI

// MARK: - header de pantalla

struct ScreenHeader: View {
    @EnvironmentObject var app: AppState
    let subtitle: String

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack(alignment: .firstTextBaseline) {
                Text("DOMOSONICA")
                    .font(Theme.label(15))
                    .tracking(4.5)
                    .foregroundStyle(Theme.air)
                Spacer()
                HStack(spacing: 6) {
                    Circle().fill(app.conn.dot).frame(width: 8, height: 8)
                    Text(app.conn.text)
                        .font(Theme.mono(10))
                        .foregroundStyle(Theme.muted)
                }
            }
            Text(subtitle)
                .font(Theme.mono(11))
                .foregroundStyle(Theme.muted)
        }
    }
}

// MARK: - banner de error

struct ErrorBanner: View {
    @EnvironmentObject var app: AppState
    var body: some View {
        if let err = app.lastError {
            HStack(spacing: 8) {
                Image(systemName: "exclamationmark.triangle")
                Text(err).font(Theme.mono(11))
                Spacer()
                Button { app.lastError = nil } label: {
                    Image(systemName: "xmark").font(.system(size: 10))
                }
            }
            .foregroundStyle(Theme.alert)
            .padding(10)
            .background(Theme.alert.opacity(0.08))
            .overlay(RoundedRectangle(cornerRadius: 8).stroke(Theme.alert.opacity(0.35), lineWidth: 1))
            .clipShape(RoundedRectangle(cornerRadius: 8))
        }
    }
}

// MARK: - VU de las 8 entradas del ADC

struct VUMeter: View {
    let peaks: [Int]          // adc_pk crudo (valor absoluto de pico)

    private func level(_ pk: Int) -> CGFloat {
        // pico → dBFS (asumiendo 16 bit útiles del monitor) → 0..1 sobre −60 dB
        let db = 20 * log10(max(Double(pk), 1) / 32768.0)
        return CGFloat(min(max((db + 60) / 60, 0), 1))
    }

    var body: some View {
        HStack(alignment: .bottom, spacing: 8) {
            ForEach(0..<8, id: \.self) { i in
                let lv = i < peaks.count ? level(peaks[i]) : 0
                VStack(spacing: 4) {
                    GeometryReader { geo in
                        ZStack(alignment: .bottom) {
                            RoundedRectangle(cornerRadius: 2)
                                .fill(Theme.line.opacity(0.5))
                            RoundedRectangle(cornerRadius: 2)
                                .fill(lv > 0.92 ? Theme.wood : Theme.wave)
                                .frame(height: max(geo.size.height * lv, 2))
                                .animation(.linear(duration: 0.25), value: lv)
                        }
                    }
                    Text("c\(i)")
                        .font(Theme.mono(9))
                        .foregroundStyle(Theme.muted)
                }
            }
        }
        .frame(height: 74)
    }
}

// MARK: - fila de dato mono (salud del sistema)

struct DataRow: View {
    let label: String
    let value: String
    var alert = false
    var body: some View {
        HStack {
            Text(label).font(Theme.mono(11)).foregroundStyle(Theme.muted)
            Spacer()
            Text(value).font(Theme.mono(11, weight: .medium))
                .foregroundStyle(alert ? Theme.alert : Theme.air)
        }
    }
}

// MARK: - lectura física grande (az / xyz)

struct PhysReadout: View {
    let text: String
    var body: some View {
        Text(text)
            .font(Theme.mono(15, weight: .medium))
            .foregroundStyle(Theme.wave)
            .frame(maxWidth: .infinity)
            .padding(.vertical, 8)
            .background(Theme.ink)
            .overlay(RoundedRectangle(cornerRadius: 8).stroke(Theme.line, lineWidth: 1))
            .clipShape(RoundedRectangle(cornerRadius: 8))
    }
}

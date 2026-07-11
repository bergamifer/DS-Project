//
//  RIRView.swift
//  Domosonica — pestaña "Sala": medición de RIR y diagnóstico "¿esta sala focaliza?".
//

import SwiftUI

struct RIRView: View {
    @StateObject private var meter = RIRMeter()

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 18) {
                ScreenHeader(subtitle: "sala · respuesta al impulso (RIR)")

                SectionLabel(text: "Medición")
                VStack(alignment: .leading, spacing: 12) {
                    HStack {
                        Text("excitación").font(Theme.mono(11)).foregroundStyle(Theme.muted)
                        Spacer()
                        Text("parlante del iPhone").font(Theme.mono(11)).foregroundStyle(Theme.air)
                    }
                    HStack {
                        Text("instrumento (walker)").font(Theme.mono(11)).foregroundStyle(Theme.muted)
                        Spacer()
                        Text("próximamente").font(Theme.mono(10)).foregroundStyle(Theme.wood)
                    }
                    Divider().overlay(Theme.line)
                    Text("Subí el volumen del iPhone al máximo, sostenelo a 1–2 m de una pared, y hacé **silencio** durante los ~5 s del barrido. El resultado estima cuánto reverbera la sala y si el foco va a formarse nítido.")
                        .font(.system(size: 12))
                        .foregroundStyle(Theme.muted)

                    Button {
                        Task { await meter.measure() }
                    } label: {
                        HStack {
                            if case .running = meter.state {
                                ProgressView().tint(Theme.ink)
                            } else {
                                Image(systemName: "waveform.badge.mic")
                            }
                            Text(buttonLabel).font(Theme.mono(13, weight: .semibold))
                        }
                        .frame(maxWidth: .infinity)
                        .padding(.vertical, 6)
                    }
                    .buttonStyle(.borderedProminent)
                    .tint(Theme.wave)
                    .disabled(isRunning)

                    if case .failed(let msg) = meter.state {
                        Text(msg).font(Theme.mono(11)).foregroundStyle(Theme.alert)
                    }
                }
                .card()

                if let r = meter.result {
                    SectionLabel(text: "Veredicto")
                    Text(r.verdict)
                        .font(.system(size: 14))
                        .foregroundStyle(Theme.air)
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .padding(14)
                        .background((r.midRT ?? 9) < 1.2 ? Theme.wave.opacity(0.08) : Theme.wood.opacity(0.08))
                        .overlay(RoundedRectangle(cornerRadius: 10)
                            .stroke(((r.midRT ?? 9) < 1.2 ? Theme.wave : Theme.wood).opacity(0.4), lineWidth: 1))
                        .clipShape(RoundedRectangle(cornerRadius: 10))

                    SectionLabel(text: "RT60 por banda")
                    HStack(spacing: 10) {
                        BandTile(label: "global", rt: r.rt60Broadband)
                        ForEach(r.bands, id: \.hz) { b in
                            BandTile(label: b.hz >= 1000 ? "\(b.hz / 1000)k" : "\(b.hz)", rt: b.rt60)
                        }
                    }

                    SectionLabel(text: "Curva de decaimiento (Schroeder)")
                    DecayPlot(edc: r.edcDB, dt: r.edcDt)
                        .frame(height: 170)
                        .card()

                    SectionLabel(text: "Impulso")
                    IRPlot(ir: r.ir)
                        .frame(height: 90)
                        .card()
                }

                Text("Método Farina: sweep exponencial 60 Hz–16 kHz (3 s) + deconvolución. El modo .measurement desactiva parte del procesado del mic del iPhone; aun así no es un mic de calibración — usalo como diagnóstico, no como medición de laboratorio.")
                    .font(Theme.mono(9.5))
                    .foregroundStyle(Theme.muted)
            }
            .padding(16)
        }
        .background(Theme.ink)
    }

    private var isRunning: Bool {
        if case .running = meter.state { return true }
        return false
    }

    private var buttonLabel: String {
        switch meter.state {
        case .idle:            return "Medir la sala"
        case .running(let s):  return s
        case .done:            return "Medir de nuevo"
        case .failed:          return "Reintentar"
        }
    }
}

// MARK: - tiles y gráficos

struct BandTile: View {
    let label: String
    let rt: Float?
    var body: some View {
        VStack(spacing: 3) {
            Text(rt.map { String(format: "%.2f", $0) } ?? "—")
                .font(Theme.mono(14, weight: .medium))
                .foregroundStyle(rt == nil ? Theme.muted : Theme.air)
            Text(label)
                .font(Theme.mono(9))
                .foregroundStyle(Theme.muted)
        }
        .frame(maxWidth: .infinity)
        .padding(.vertical, 10)
        .background(Theme.ink2)
        .overlay(RoundedRectangle(cornerRadius: 8).stroke(Theme.line, lineWidth: 1))
        .clipShape(RoundedRectangle(cornerRadius: 8))
    }
}

/// EDC en dB (0 a −60) contra tiempo.
struct DecayPlot: View {
    let edc: [Float]
    let dt: Float

    var body: some View {
        Canvas { ctx, size in
            guard edc.count > 1 else { return }
            let minDB: Float = -60
            // grilla horizontal cada -10 dB
            for db in stride(from: 0, through: -60, by: -10) {
                let y = CGFloat((Float(-db)) / -minDB) * size.height
                var line = Path()
                line.move(to: CGPoint(x: 0, y: y))
                line.addLine(to: CGPoint(x: size.width, y: y))
                ctx.stroke(line, with: .color(Theme.line.opacity(0.6)), lineWidth: 0.5)
                ctx.draw(Text("\(db)").font(.system(size: 7, design: .monospaced)).foregroundStyle(Theme.muted),
                         at: CGPoint(x: 12, y: max(y - 6, 5)))
            }
            var path = Path()
            for (i, v) in edc.enumerated() {
                let x = CGFloat(i) / CGFloat(edc.count - 1) * size.width
                let y = CGFloat(min(max(v / minDB, 0), 1)) * size.height
                i == 0 ? path.move(to: CGPoint(x: x, y: y)) : path.addLine(to: CGPoint(x: x, y: y))
            }
            ctx.stroke(path, with: .color(Theme.wave), lineWidth: 1.6)
            // duración total
            let dur = Float(edc.count) * dt
            ctx.draw(Text(String(format: "0 → %.1f s", dur))
                        .font(.system(size: 8, design: .monospaced)).foregroundStyle(Theme.muted),
                     at: CGPoint(x: size.width - 34, y: size.height - 8))
        }
    }
}

/// Envolvente del impulso.
struct IRPlot: View {
    let ir: [Float]
    var body: some View {
        Canvas { ctx, size in
            guard ir.count > 1 else { return }
            let mid = size.height / 2
            var path = Path()
            for (i, v) in ir.enumerated() {
                let x = CGFloat(i) / CGFloat(ir.count - 1) * size.width
                let y = mid - CGFloat(max(min(v, 1), -1)) * (mid - 2)
                i == 0 ? path.move(to: CGPoint(x: x, y: y)) : path.addLine(to: CGPoint(x: x, y: y))
            }
            ctx.stroke(path, with: .color(Theme.wood.opacity(0.9)), lineWidth: 0.8)
        }
    }
}

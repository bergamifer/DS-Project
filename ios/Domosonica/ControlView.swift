//
//  ControlView.swift
//  Domosonica — v1: modos, haz RX/TX, AUTOFOCUS y entradas, contra la API del ESP32.
//

import SwiftUI

struct ControlView: View {
    @EnvironmentObject var app: AppState

    // haz custom
    @State private var beamIsTX = true
    @State private var rxAz: Double = 0            // grados
    @State private var fx: Double = 0              // mm
    @State private var fy: Double = 0
    @State private var fz: Double = 1000

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 18) {
                ScreenHeader(subtitle: app.status?.deviceId.map { "nodo \($0)" } ?? "nodo —")
                ErrorBanner()

                // ---- MODOS ----
                SectionLabel(text: "Modos")
                LazyVGrid(columns: [GridItem(.flexible(), spacing: 10), GridItem(.flexible())],
                          spacing: 10) {
                    ForEach(DSModes.grid(names: app.modeNames,
                                         numModes: app.status?.numModes)) { m in
                        ModeButton(mode: m, active: app.status?.mode == m.id) {
                            app.setMode(m.id)
                        }
                    }
                }

                // ---- AUTOFOCUS en curso ----
                if app.status?.mode == DSModes.autofocusIndex {
                    AutofocusCard(state: CalState(rawValue: app.status?.calState ?? 0) ?? .idle)
                }

                // ---- HAZ CUSTOM ----
                SectionLabel(text: "Haz a medida")
                VStack(alignment: .leading, spacing: 14) {
                    Picker("", selection: $beamIsTX) {
                        Text("Foco de emisión").tag(true)
                        Text("Haz de escucha").tag(false)
                    }
                    .pickerStyle(.segmented)

                    if beamIsTX {
                        SliderRow(label: "x — lateral", value: $fx, range: -1500...1500, unit: "mm") { sendBeam() }
                        SliderRow(label: "y — altura", value: $fy, range: -500...500, unit: "mm") { sendBeam() }
                        SliderRow(label: "z — distancia", value: $fz, range: 200...3000, unit: "mm") { sendBeam() }
                        PhysReadout(text: String(format: "foco  x %+.2f  y %+.2f  z %.2f  m",
                                                 fx / 1000, fy / 1000, fz / 1000))
                    } else {
                        SliderRow(label: "azimuth", value: $rxAz, range: -90...90, unit: "°") { sendBeam() }
                        PhysReadout(text: String(format: "escucha  az %+.0f°", rxAz))
                    }

                    Text("Se aplica solo (250 ms después de soltar). El STM32 confirma con ACK.")
                        .font(Theme.mono(10))
                        .foregroundStyle(Theme.muted)
                }
                .card()

                // ---- ENTRADAS ----
                SectionLabel(text: "Entradas · picos ADC")
                VUMeter(peaks: app.status?.adcPk ?? [])
                    .card()

                // ---- SALUD ----
                SectionLabel(text: "Salud")
                VStack(spacing: 8) {
                    DataRow(label: "underruns", value: "\(app.status?.underruns ?? 0)",
                            alert: (app.status?.underruns ?? 0) > 100)
                    DataRow(label: "errores SAI", value: "\(app.status?.saiErrors ?? 0)",
                            alert: (app.status?.saiErrors ?? 0) > 0)
                    DataRow(label: "ADC overflow", value: (app.status?.ovf ?? 0) == 0 ? "no" : "SÍ",
                            alert: (app.status?.ovf ?? 0) != 0)
                    DataRow(label: "protocolo", value: "v\(app.status?.protoVer ?? 0)")
                    DataRow(label: "uptime ESP32", value: uptimeText)
                }
                .card()
            }
            .padding(16)
        }
        .background(Theme.ink)
        .onAppear { syncFromStatus() }
        .onChange(of: app.status?.mode) { syncFromStatus() }
    }

    private var uptimeText: String {
        let s = (app.status?.uptimeMs ?? 0) / 1000
        return String(format: "%d:%02d:%02d", s / 3600, (s / 60) % 60, s % 60)
    }

    private func sendBeam() {
        if beamIsTX {
            app.queueBeam(tx: true, fxMM: Int(fx), fyMM: Int(fy), fzMM: Int(fz))
        } else {
            app.queueBeam(tx: false, az: Int(rxAz))
        }
    }

    /// Adoptar el estado real reportado (p.ej. tras un preset o cambio remoto)
    private func syncFromStatus() {
        guard let s = app.status else { return }
        if let f = s.focus, f.fz != 0 {
            fx = Double(f.fx); fy = Double(f.fy); fz = Double(f.fz)
        }
        if let az = s.rxAz { rxAz = Double(az) }
    }
}

// MARK: - botón de modo

struct ModeButton: View {
    let mode: DSMode
    let active: Bool
    let action: () -> Void

    private var accent: Color {
        switch mode.kind {
        case .autofocus: return Theme.wood
        default:         return Theme.wave
        }
    }

    var body: some View {
        Button(action: action) {
            VStack(alignment: .leading, spacing: 3) {
                HStack {
                    Text(String(format: "%02d", mode.id))
                        .font(Theme.mono(10))
                        .foregroundStyle(active ? accent : Theme.muted)
                    Spacer()
                    if active {
                        Circle().fill(accent).frame(width: 6, height: 6)
                    }
                }
                Text(mode.short)
                    .font(.system(size: 15, weight: active ? .semibold : .regular))
                    .foregroundStyle(active ? Theme.air : Theme.body)
                Text(mode.detail)
                    .font(Theme.mono(9.5))
                    .foregroundStyle(Theme.muted)
                    .lineLimit(1)
            }
            .padding(11)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(active ? accent.opacity(0.10) : Theme.ink2)
            .overlay(RoundedRectangle(cornerRadius: 9)
                .stroke(active ? accent.opacity(0.7) : Theme.line, lineWidth: 1))
            .clipShape(RoundedRectangle(cornerRadius: 9))
        }
        .buttonStyle(.plain)
    }
}

// MARK: - tarjeta de AUTOFOCUS

struct AutofocusCard: View {
    let state: CalState

    var body: some View {
        HStack(spacing: 12) {
            if state.isActive {
                ProgressView().tint(Theme.wood)
            } else {
                Image(systemName: state == .done ? "scope" : "mic")
                    .foregroundStyle(Theme.wood)
            }
            VStack(alignment: .leading, spacing: 2) {
                Text("AUTOFOCUS").font(Theme.label(10)).tracking(2).foregroundStyle(Theme.wood)
                Text(state.label).font(Theme.mono(12)).foregroundStyle(Theme.air)
            }
            Spacer()
        }
        .padding(12)
        .background(Theme.wood.opacity(0.07))
        .overlay(RoundedRectangle(cornerRadius: 9).stroke(Theme.wood.opacity(0.4), lineWidth: 1))
        .clipShape(RoundedRectangle(cornerRadius: 9))
    }
}

// MARK: - slider con label y unidad

struct SliderRow: View {
    let label: String
    @Binding var value: Double
    let range: ClosedRange<Double>
    let unit: String
    let onChange: () -> Void

    var body: some View {
        VStack(spacing: 4) {
            HStack {
                Text(label).font(Theme.mono(11)).foregroundStyle(Theme.muted)
                Spacer()
                Text("\(Int(value)) \(unit)")
                    .font(Theme.mono(11, weight: .medium))
                    .foregroundStyle(Theme.air)
            }
            Slider(value: $value, in: range) { editing in
                if !editing { onChange() }
            }
            .tint(Theme.wave)
        }
    }
}

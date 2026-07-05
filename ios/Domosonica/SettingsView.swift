//
//  SettingsView.swift
//  Domosonica — conexión al ESP32 y ficha del protocolo.
//

import SwiftUI

struct SettingsView: View {
    @EnvironmentObject var app: AppState
    @State private var testResult: String?
    @State private var testing = false

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 18) {
                ScreenHeader(subtitle: "ajustes · conexión al nodo")

                SectionLabel(text: "ESP32 — red local")
                VStack(alignment: .leading, spacing: 12) {
                    HStack {
                        Text("host").font(Theme.mono(11)).foregroundStyle(Theme.muted)
                        TextField("192.168.0.50", text: $app.host)
                            .font(Theme.mono(14))
                            .foregroundStyle(Theme.air)
                            .keyboardType(.URL)
                            .autocorrectionDisabled()
                            .textInputAutocapitalization(.never)
                            .multilineTextAlignment(.trailing)
                    }
                    Divider().overlay(Theme.line)
                    HStack {
                        Text("puerto").font(Theme.mono(11)).foregroundStyle(Theme.muted)
                        TextField("80", value: $app.port, format: .number.grouping(.never))
                            .font(Theme.mono(14))
                            .foregroundStyle(Theme.air)
                            .keyboardType(.numberPad)
                            .multilineTextAlignment(.trailing)
                    }
                    Divider().overlay(Theme.line)
                    Button {
                        testing = true
                        Task {
                            testResult = await app.testConnection()
                            testing = false
                        }
                    } label: {
                        HStack {
                            if testing { ProgressView().tint(Theme.wave) }
                            Text(testing ? "probando…" : "Probar conexión")
                                .font(Theme.mono(12, weight: .medium))
                        }
                    }
                    .buttonStyle(.bordered)
                    .tint(Theme.wave)

                    if let r = testResult {
                        Text(r).font(Theme.mono(11)).foregroundStyle(
                            r.hasPrefix("✓") ? Theme.wave :
                            r.hasPrefix("✗") ? Theme.alert : Theme.wood)
                    }
                }
                .card()

                SectionLabel(text: "Protocolo")
                VStack(spacing: 8) {
                    DataRow(label: "GET /ping", value: "eco del puente")
                    DataRow(label: "GET /status", value: "telemetría 1 Hz")
                    DataRow(label: "POST /mode", value: "{mode: 0–11}")
                    DataRow(label: "POST /beam", value: "{type, az, fx, fy, fz}")
                    DataRow(label: "transporte", value: "HTTP → ESP32 → UART → STM32")
                }
                .card()

                SectionLabel(text: "Acerca de")
                VStack(alignment: .leading, spacing: 6) {
                    Text("Domosonica").font(Theme.display(22)).foregroundStyle(Theme.air)
                    Text("El espacio como instrumento.")
                        .font(Theme.mono(11)).foregroundStyle(Theme.wood)
                    Text("App de control v0.1 — habla con un nodo DS10/DS11 en la red local. " +
                         "Roadmap: colocación de focos con ARKit, follow-me real, auto-survey " +
                         "de posiciones de parlantes. Ver docs/app_ios.md en el repo.")
                        .font(.system(size: 12))
                        .foregroundStyle(Theme.muted)
                        .padding(.top, 4)
                }
                .frame(maxWidth: .infinity, alignment: .leading)
                .card()
            }
            .padding(16)
        }
        .background(Theme.ink)
        .scrollDismissesKeyboard(.interactively)
    }
}

//
//  FleetView.swift
//  Domosonica — la instalación de un vistazo.
//
//  Esta pantalla existe para responder una sola pregunta desde el fondo de la
//  sala: ¿están los quince haciendo lo mismo? El caso que hay que poder ver sin
//  pensar no es "un nodo se cayó" —eso se nota— sino "un nodo quedó en otro
//  modo", que no produce ningún síntoma salvo que la obra suene mal.
//

import SwiftUI

struct FleetView: View {
    @EnvironmentObject var fleet: FleetController
    @State private var confirmAmps: Bool?          // nil = sin confirmación pendiente
    @State private var showDiscovery = false

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 20) {
                header
                if fleet.fleetDiverges { divergenceBanner }
                if let r = fleet.lastFanout { fanoutBanner(r) }
                fleetControls
                nodeList
                discovery
            }
            .padding(16)
        }
        .background(Theme.ink)
        .refreshable { await fleet.refreshAll() }
        .onAppear { fleet.startDiscovery() }
        .onDisappear { fleet.stopDiscovery() }
        .confirmationDialog(
            confirmAmps == true ? "Encender los amplificadores" : "Apagar los amplificadores",
            isPresented: Binding(get: { confirmAmps != nil },
                                 set: { if !$0 { confirmAmps = nil } }),
            titleVisibility: .visible
        ) {
            Button(confirmAmps == true ? "Encender en \(fleet.total) nodo\(fleet.total == 1 ? "" : "s")"
                                       : "Apagar en \(fleet.total) nodo\(fleet.total == 1 ? "" : "s")",
                   role: confirmAmps == true ? nil : .destructive) {
                if let on = confirmAmps {
                    confirmAmps = nil
                    Task { await fleet.setAmps(on) }
                }
            }
            Button("Cancelar", role: .cancel) { confirmAmps = nil }
        } message: {
            // Se confirma porque es una acción FÍSICA: son 32 amplificadores por
            // nodo y suenan de inmediato, al volumen que haya quedado.
            Text(confirmAmps == true
                 ? "Suenan al volumen actual, en todos los nodos habilitados."
                 : "Los nodos quedan mudos hasta volver a encenderlos.")
        }
    }

    // MARK: - encabezado

    private var header: some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack(alignment: .firstTextBaseline) {
                Text("FLOTA").font(Theme.label(15)).tracking(4.5).foregroundStyle(Theme.air)
                Spacer()
                if fleet.sending { ProgressView().controlSize(.small) }
            }
            HStack(spacing: 8) {
                Text("\(fleet.online)")
                    .font(Theme.display(34))
                    .foregroundStyle(fleet.online == fleet.total && fleet.total > 0
                                     ? Theme.wave : Theme.wood)
                Text("de \(fleet.total) en línea")
                    .font(Theme.mono(12)).foregroundStyle(Theme.muted)
                    .padding(.bottom, 5)
            }
        }
    }

    // MARK: - avisos

    private var divergenceBanner: some View {
        VStack(alignment: .leading, spacing: 4) {
            Text("LOS NODOS NO COINCIDEN").font(Theme.label(10)).tracking(2)
                .foregroundStyle(Theme.wood)
            Text("Al menos un nodo está en distinto estado que el resto. En una instalación eso no se oye como una falla: se oye como que la obra no funciona.")
                .font(.system(size: 12)).foregroundStyle(Theme.body)
        }
        .padding(12)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(Theme.wood.opacity(0.12))
        .overlay(RoundedRectangle(cornerRadius: 6).stroke(Theme.wood.opacity(0.4)))
        .clipShape(RoundedRectangle(cornerRadius: 6))
    }

    private func fanoutBanner(_ r: FanoutResult) -> some View {
        Text(r.summary)
            .font(Theme.mono(11))
            .foregroundStyle(r.allOK ? Theme.wave : Theme.alert)
            .padding(.vertical, 8).padding(.horizontal, 12)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background((r.allOK ? Theme.wave : Theme.alert).opacity(0.10))
            .clipShape(RoundedRectangle(cornerRadius: 6))
    }

    // MARK: - controles de flota

    private var fleetControls: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text("TODOS A LA VEZ").font(Theme.label(10)).tracking(2)
                .foregroundStyle(Theme.muted)
            HStack(spacing: 10) {
                Button { confirmAmps = true } label: {
                    Label("Amps ON", systemImage: "speaker.wave.2.fill")
                        .font(Theme.label(12)).frame(maxWidth: .infinity)
                }
                .buttonStyle(.borderedProminent).tint(Theme.wave.opacity(0.25))
                Button { confirmAmps = false } label: {
                    Label("Amps OFF", systemImage: "speaker.slash.fill")
                        .font(Theme.label(12)).frame(maxWidth: .infinity)
                }
                .buttonStyle(.bordered).tint(Theme.alert)
            }
            .disabled(fleet.total == 0)

            HStack(spacing: 10) {
                ForEach([-40, -30, -20, -15], id: \.self) { db in
                    Button("\(db) dB") { Task { await fleet.setVolume(db) } }
                        .font(Theme.mono(12))
                        .frame(maxWidth: .infinity)
                        .buttonStyle(.bordered)
                }
            }
            .disabled(fleet.total == 0)
            // −15 dB y no 0: el techo útil medido del amplificador está en
            // −15,4 dB (loopback_debug.md §27). Más arriba sólo se recorta.
            Text("−15 dB es el techo útil medido del amplificador; más arriba recorta.")
                .font(Theme.mono(10)).foregroundStyle(Theme.muted)
        }
    }

    // MARK: - lista de nodos

    private var nodeList: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text("NODOS").font(Theme.label(10)).tracking(2).foregroundStyle(Theme.muted)
            ForEach(fleet.nodes.nodes) { node in
                NodeRow(node: node,
                        state: fleet.stateOf(node),
                        divergences: fleet.divergences(node))
            }
            if fleet.nodes.nodes.isEmpty {
                Text("No hay nodos. Agregalos en Espacial → Nodos, o buscalos acá abajo.")
                    .font(Theme.mono(11)).foregroundStyle(Theme.muted)
            }
        }
    }

    // MARK: - descubrimiento

    private var discovery: some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack {
                Text("EN LA RED").font(Theme.label(10)).tracking(2).foregroundStyle(Theme.muted)
                Spacer()
                Text("_domosonica._tcp").font(Theme.mono(9)).foregroundStyle(Theme.muted)
            }
            if fleet.discovered.isEmpty {
                Text("Buscando nodos… Si no aparece ninguno, puede ser que el firmware del ESP32 sea anterior al anuncio mDNS, o que la app no tenga permiso de red local.")
                    .font(Theme.mono(10)).foregroundStyle(Theme.muted)
            } else {
                ForEach(fleet.discovered, id: \.self) { h in
                    HStack {
                        Circle().fill(Theme.wave).frame(width: 6, height: 6)
                        Text("\(h).local").font(Theme.mono(11)).foregroundStyle(Theme.body)
                        Spacer()
                    }
                }
                Button("Agregar los que falten") {
                    let n = fleet.adoptDiscovered()
                    if n > 0 { Task { await fleet.refreshAll() } }
                }
                .font(Theme.label(11))
                .buttonStyle(.bordered)
            }
        }
    }
}

// MARK: - fila de un nodo

private struct NodeRow: View {
    let node: DSNode
    let state: NodeState
    let divergences: [String]

    var body: some View {
        VStack(alignment: .leading, spacing: 7) {
            HStack(spacing: 8) {
                Circle().fill(state.conn.dot).frame(width: 9, height: 9)
                Text(node.name).font(Theme.label(13)).foregroundStyle(Theme.air)
                Text(node.host).font(Theme.mono(10)).foregroundStyle(Theme.muted)
                Spacer()
                if !node.enabled {
                    Text("deshabilitado").font(Theme.mono(9)).foregroundStyle(Theme.muted)
                } else {
                    Text(state.conn.short).font(Theme.mono(10)).foregroundStyle(state.conn.dot)
                }
            }

            if let s = state.status, s.stmOnline {
                // Lo que se muestra es lo que permite decidir algo: en qué modo
                // está, a qué volumen, si suena, y si algo se está degradando.
                HStack(spacing: 14) {
                    metric("modo", s.modeName.map { short($0) } ?? "\(s.mode ?? -1)")
                    if let v = s.volDb { metric("vol", "\(v) dB") }
                    if let a = s.amps { metric("amps", a ? "ON" : "off") }
                    Spacer()
                }
                HStack(spacing: 14) {
                    metric("under", "\(s.underruns ?? 0)",
                           bad: (s.underruns ?? 0) > 0)
                    metric("SAI", "\(s.saiErrors ?? 0)",
                           bad: (s.saiErrors ?? 0) > 0)
                    if let c = s.cpuFillsPct { metric("cpu", String(format: "%.1f%%", c),
                                                      bad: c > 80) }
                    if let u = s.stm32UptimeMs { metric("uptime", uptime(u)) }
                    Spacer()
                }
                HStack(spacing: 8) {
                    if let fw = s.fwId {
                        Text("fw \(fw)").font(Theme.mono(9)).foregroundStyle(Theme.muted)
                    }
                    /* Sólo se dice algo cuando hay algo que decir. "Arranca
                     * solo" en verde es información; repetirlo en los quince
                     * nodos es ruido, así que el caso normal no imprime nada. */
                    if s.autostartHeld == true {
                        Text("AUTOARRANQUE SUSPENDIDO")
                            .font(Theme.mono(9)).foregroundStyle(Theme.alert)
                    } else if s.autostart == true {
                        Text("arranca solo").font(Theme.mono(9)).foregroundStyle(Theme.wave)
                    }
                }
            } else if let e = state.lastError {
                Text(e).font(Theme.mono(10)).foregroundStyle(Theme.alert)
            }

            if !divergences.isEmpty {
                Text("distinto al resto: \(divergences.joined(separator: " · "))")
                    .font(Theme.mono(10)).foregroundStyle(Theme.wood)
            }
        }
        .padding(12)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(Theme.ink2)
        .overlay(RoundedRectangle(cornerRadius: 6)
            .stroke(divergences.isEmpty ? Theme.line : Theme.wood.opacity(0.55)))
        .clipShape(RoundedRectangle(cornerRadius: 6))
        .opacity(node.enabled ? 1 : 0.45)
    }

    private func metric(_ k: String, _ v: String, bad: Bool = false) -> some View {
        VStack(alignment: .leading, spacing: 1) {
            Text(k).font(Theme.mono(9)).foregroundStyle(Theme.muted)
            Text(v).font(Theme.mono(11)).foregroundStyle(bad ? Theme.alert : Theme.body)
        }
    }

    /// El nombre del firmware es largo ("BARRIDO VERTICAL 800Hz arriba<->abajo
    /// @1m"); en una fila entra la primera parte, que es la que identifica.
    private func short(_ s: String) -> String {
        s.count <= 18 ? s : String(s.prefix(17)) + "…"
    }

    private func uptime(_ ms: Int) -> String {
        let s = ms / 1000
        if s < 60 { return "\(s)s" }
        if s < 3600 { return "\(s / 60)m" }
        return "\(s / 3600)h\((s % 3600) / 60)m"
    }
}

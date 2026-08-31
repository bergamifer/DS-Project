//
//  Fleet.swift
//  Domosonica — la instalación como UNA cosa, no como quince cosas.
//
//  Hasta acá la app hablaba con un nodo (el `host` de Ajustes) y, sólo para
//  mover el foco, con la lista de `NodesStore`. Eran dos fuentes de verdad
//  compitiendo: el modo, el volumen y los amplificadores iban a un solo nodo, y
//  la lista existía únicamente adentro de la pestaña Espacial.
//
//  Acá vive la capa que faltaba: fan-out de TODOS los comandos, estado por nodo,
//  y —lo que más importa en una instalación de 15— la detección de que un nodo
//  se desvió de los demás.
//
//  Lo que esta capa NO hace: coordinar el RELOJ. La coherencia de fase entre
//  nodos depende de compartir MCLK (JP2), no de que los comandos salgan juntos;
//  sin eso la alineación es de envolvente (~ms), no de fase. Está en
//  docs/multinode.md, y conviene no confundir las dos capas.
//

import Foundation
import Network
import SwiftUI

// MARK: - estado de un nodo

struct NodeState: Equatable {
    enum Conn: Equatable {
        case unknown, offline, bridgeOnly, online

        var dot: Color {
            switch self {
            case .unknown:    return Theme.muted
            case .offline:    return Theme.alert
            case .bridgeOnly: return Theme.wood
            case .online:     return Theme.wave
            }
        }
        var short: String {
            switch self {
            case .unknown:    return "—"
            case .offline:    return "sin conexión"
            case .bridgeOnly: return "sin STM32"
            case .online:     return "en línea"
            }
        }
    }

    var conn: Conn = .unknown
    var status: DSStatus?
    var lastSeen: Date?
    var lastError: String?
}

// MARK: - resultado de un envío a la flota

struct FanoutResult: Equatable {
    let action: String
    let ok: [String]          // nombres de los nodos que confirmaron
    let failed: [String]      // los que no

    var total: Int { ok.count + failed.count }
    var allOK: Bool { failed.isEmpty && !ok.isEmpty }

    /// Lo que se le muestra a una persona. Cuando falta alguno, se dice CUÁL:
    /// "13/15" obliga a ir a buscar cuáles son los dos, y en una muestra con
    /// público eso es tiempo que no hay.
    var summary: String {
        if ok.isEmpty && failed.isEmpty { return "no hay nodos habilitados" }
        if failed.isEmpty { return "\(action): \(ok.count) de \(ok.count) ✓" }
        if ok.isEmpty { return "\(action): ningún nodo respondió" }
        return "\(action): \(ok.count)/\(total) — sin responder: \(failed.joined(separator: ", "))"
    }
}

// MARK: - controlador

@MainActor
final class FleetController: ObservableObject {

    let nodes: NodesStore

    @Published private(set) var state: [UUID: NodeState] = [:]
    @Published private(set) var lastFanout: FanoutResult?
    @Published private(set) var discovered: [String] = []   // hosts vistos por mDNS
    @Published var sending = false

    private var pollTask: Task<Void, Never>?
    private var browser: NWBrowser?
    private var pollCursor = 0

    init(nodes: NodesStore) {
        self.nodes = nodes
    }

    func stateOf(_ n: DSNode) -> NodeState { state[n.id] ?? NodeState() }

    // MARK: - salud de la flota

    var online: Int { nodes.enabledNodes.filter { stateOf($0).conn == .online }.count }
    var total: Int { nodes.enabledNodes.count }

    /// El nodo "de referencia" para comparar: el primero que esté en línea.
    /// No es una elección profunda — lo que importa es que TODOS coincidan, no
    /// cuál es el correcto. Si hay desacuerdo, la vista lo muestra y decide una
    /// persona.
    private var reference: DSStatus? {
        nodes.enabledNodes.compactMap { stateOf($0).status }.first { $0.stmOnline }
    }

    /// En qué se desvía este nodo del resto de la flota.
    ///
    /// Es LA función de esta pantalla. Con 15 nodos, que uno se quede en otro
    /// modo o con otro firmware no produce ningún síntoma visible salvo que la
    /// obra suene mal, y a esa altura ya nadie sabe por qué.
    func divergences(_ n: DSNode) -> [String] {
        guard let s = stateOf(n).status, s.stmOnline, let r = reference else { return [] }
        var d: [String] = []
        if let a = s.mode, let b = r.mode, a != b { d.append("modo \(a)") }
        if let a = s.volDb, let b = r.volDb, a != b { d.append("volumen \(a) dB") }
        if let a = s.amps, let b = r.amps, a != b { d.append(a ? "amps ON" : "amps OFF") }
        if let a = s.fwId, let b = r.fwId, a != b { d.append("firmware \(a)") }
        if let a = s.numModes, let b = r.numModes, a != b { d.append("\(a) modos") }
        return d
    }

    var fleetDiverges: Bool { nodes.enabledNodes.contains { !divergences($0).isEmpty } }

    // MARK: - polling escalonado

    /// Un nodo por tick, en round-robin.
    ///
    /// La versión de un solo nodo consultaba a 1 Hz. Quince a 1 Hz son quince
    /// pedidos por segundo sobre el WiFi de la sala, compitiendo con los
    /// comandos que sí importan. Así, cada nodo se refresca cada N segundos y la
    /// red ve un pedido por segundo pase lo que pase.
    func startPolling() {
        guard pollTask == nil else { return }
        pollTask = Task { [weak self] in
            while !Task.isCancelled {
                await self?.pollNext()
                try? await Task.sleep(nanoseconds: 1_000_000_000)
            }
        }
    }

    func stopPolling() {
        pollTask?.cancel()
        pollTask = nil
    }

    private func pollNext() async {
        let ns = nodes.enabledNodes
        guard !ns.isEmpty else { return }
        pollCursor = (pollCursor + 1) % ns.count
        await refresh(ns[pollCursor])
    }

    /// Refresca TODOS a la vez. Para cuando una persona está mirando la pantalla
    /// y quiere una foto del momento, no el barrido.
    func refreshAll() async {
        await withTaskGroup(of: Void.self) { g in
            for n in nodes.enabledNodes { g.addTask { await self.refresh(n) } }
        }
    }

    func refresh(_ n: DSNode) async {
        let client = ESP32Client(host: n.host, port: n.port)
        var s = stateOf(n)
        do {
            let st = try await client.status()
            s.status = st
            s.conn = st.stmOnline ? .online : .bridgeOnly
            s.lastSeen = Date()
            s.lastError = nil
        } catch {
            s.status = nil
            s.conn = .offline
            s.lastError = "sin respuesta"
        }
        state[n.id] = s
    }

    // MARK: - fan-out

    /// Manda la misma acción a todos los nodos habilitados y devuelve QUIÉN
    /// contestó. En paralelo: en serie, quince nodos con 2,5 s de timeout cada
    /// uno serían 37 segundos en el peor caso.
    @discardableResult
    func fanout(_ action: String,
                _ op: @escaping @Sendable (ESP32Client) async throws -> AckResponse)
        async -> FanoutResult
    {
        let targets = nodes.enabledNodes
        sending = true
        defer { sending = false }

        var ok: [String] = [], failed: [String] = []
        await withTaskGroup(of: (String, Bool).self) { g in
            for n in targets {
                g.addTask {
                    let c = ESP32Client(host: n.host, port: n.port)
                    let r = try? await op(c)
                    return (n.name, r?.isOk ?? false)
                }
            }
            for await (name, good) in g { good ? ok.append(name) : failed.append(name) }
        }
        // Orden estable: el TaskGroup devuelve en el orden en que terminan, que
        // depende de la red. Una lista que se reordena sola es ilegible.
        let order = Dictionary(uniqueKeysWithValues: targets.enumerated().map { ($1.name, $0) })
        ok.sort { order[$0, default: 0] < order[$1, default: 0] }
        failed.sort { order[$0, default: 0] < order[$1, default: 0] }

        let res = FanoutResult(action: action, ok: ok, failed: failed)
        lastFanout = res
        await refreshAll()
        return res
    }

    func setMode(_ idx: Int) async { await fanout("modo \(idx)") { try await $0.setMode(idx) } }
    func setVolume(_ db: Int) async { await fanout("volumen \(db) dB") { try await $0.setVolume(db: db) } }
    func setAmps(_ on: Bool) async {
        await fanout(on ? "amplificadores ON" : "amplificadores OFF") {
            try await $0.setAmps(on: on)
        }
    }

    // MARK: - descubrimiento por mDNS

    /// Busca `_domosonica._tcp`, el tipo de servicio propio que anuncia el
    /// ESP32. Con `_http._tcp` a secas la red devuelve también impresoras y
    /// routers, y habría que adivinar cuál es un nodo.
    func startDiscovery() {
        guard browser == nil else { return }
        let b = NWBrowser(for: .bonjour(type: "_domosonica._tcp", domain: nil),
                          using: .tcp)
        b.browseResultsChangedHandler = { [weak self] results, _ in
            let hosts: [String] = results.compactMap {
                if case let .service(name, _, _, _) = $0.endpoint { return name }
                return nil
            }.sorted()
            Task { @MainActor in self?.discovered = hosts }
        }
        b.start(queue: .main)
        browser = b
    }

    func stopDiscovery() {
        browser?.cancel()
        browser = nil
    }

    /// Agrega los nodos descubiertos que todavía no están en la lista.
    /// El nombre mDNS resuelve como `<nombre>.local`, así que no hace falta IP.
    @discardableResult
    func adoptDiscovered() -> Int {
        let known = Set(nodes.nodes.map { $0.host.lowercased() })
        var added = 0
        for h in discovered {
            let host = "\(h).local"
            if !known.contains(host.lowercased()) {
                nodes.nodes.append(DSNode(name: h, host: host,
                                          x: Float(nodes.nodes.count) * 1.0))
                added += 1
            }
        }
        return added
    }
}

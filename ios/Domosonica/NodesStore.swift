//
//  NodesStore.swift
//  Domosonica — registro de nodos (muros) con pose en la sala y envío fan-out.
//
//  Cada nodo DS10/DS11 es un muro de 32 parlantes con su propio ESP32. El foco
//  se define en el marco GLOBAL de la sala; a cada nodo se le envía transformado
//  a su marco LOCAL (el STM32 calcula sus delays respecto de su propio muro).
//
//  Nota de física (documentada en docs/multinode.md): la coherencia de fase es
//  perfecta DENTRO de cada nodo; ENTRE nodos depende de compartir MCLK (JP2 en
//  DS11) — sin eso, la alineación inter-nodo es de envolvente (~ms), no de fase.
//

import Foundation
import simd

struct DSNode: Identifiable, Codable, Equatable {
    var id = UUID()
    var name: String
    var host: String
    var port: Int = 80
    var x: Float = 0      // posición del centro del muro (m, marco global)
    var y: Float = 0
    var z: Float = 0
    var yawDeg: Float = 0 // rotación sobre el eje vertical; 0 = mira a +z
    var enabled: Bool = true

    var position: SIMD3<Float> { SIMD3(x, y, z) }

    /// Foco global → marco local del nodo (el que entiende su STM32).
    func toLocal(_ p: SIMD3<Float>) -> SIMD3<Float> {
        let d = p - position
        let a = yawDeg * .pi / 180
        let ca = cos(a), sa = sin(a)
        return SIMD3(ca * d.x - sa * d.z, d.y, sa * d.x + ca * d.z)
    }
}

@MainActor
final class NodesStore: ObservableObject {
    @Published var nodes: [DSNode] {
        didSet { save() }
    }
    @Published var lastSendSummary: String?

    private static let key = "ds_nodes_v1"
    private var lastSend = Date.distantPast

    init(defaultHost: String) {
        if let data = UserDefaults.standard.data(forKey: Self.key),
           let decoded = try? JSONDecoder().decode([DSNode].self, from: data),
           !decoded.isEmpty {
            nodes = decoded
        } else {
            nodes = [DSNode(name: "Nodo 1", host: defaultHost)]
        }
    }

    private func save() {
        if let data = try? JSONEncoder().encode(nodes) {
            UserDefaults.standard.set(data, forKey: Self.key)
        }
    }

    func add() {
        let n = nodes.count + 1
        // por defecto, cada nodo nuevo aparece corrido 1 m a la derecha
        nodes.append(DSNode(name: "Nodo \(n)", host: "192.168.0.\(49 + n)",
                            x: Float(n - 1) * 1.0))
    }

    var enabledNodes: [DSNode] { nodes.filter(\.enabled) }

    /// Fan-out del foco global a todos los nodos habilitados (rate limit 300 ms).
    /// `force` saltea el rate limit (para el envío al soltar el dedo).
    func sendFocus(_ p: SIMD3<Float>, force: Bool = false) {
        let now = Date()
        guard force || now.timeIntervalSince(lastSend) > 0.3 else { return }
        lastSend = now

        let targets = enabledNodes
        guard !targets.isEmpty else { return }
        Task {
            var oks = 0
            await withTaskGroup(of: Bool.self) { group in
                for node in targets {
                    let l = node.toLocal(p)
                    group.addTask {
                        let c = ESP32Client(host: node.host, port: node.port)
                        let r = try? await c.setBeam(tx: true,
                                                     fx: Int(l.x * 1000),
                                                     fy: Int(l.y * 1000),
                                                     fz: Int(l.z * 1000))
                        return r?.isOk ?? false
                    }
                }
                for await ok in group where ok { oks += 1 }
            }
            lastSendSummary = "\(oks)/\(targets.count) nodos"
        }
    }
}

// MARK: - focos guardados

struct SavedFocus: Identifiable, Codable, Equatable {
    var id = UUID()
    var name: String
    var x: Float
    var y: Float
    var z: Float
    var point: SIMD3<Float> { SIMD3(x, y, z) }
}

// MARK: - trayectoria (partitura espacial mínima)

struct TrajPoint: Codable {
    var t: Float
    var x: Float
    var y: Float
    var z: Float
    var point: SIMD3<Float> { SIMD3(x, y, z) }
}

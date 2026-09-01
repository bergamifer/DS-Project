//
//  AppState.swift
//  Domosonica — estado global: conexión, polling 1 Hz, envío con debounce.
//

import Foundation
import SwiftUI

@MainActor
final class AppState: ObservableObject {

    enum Conn: Equatable {
        case offline            // sin respuesta del ESP32
        case bridgeOnly         // ESP32 responde, STM32 en timeout
        case online             // cadena completa
        var dot: Color {
            switch self {
            case .offline:    return Theme.alert
            case .bridgeOnly: return Theme.wood
            case .online:     return Theme.wave
            }
        }
        var text: String {
            switch self {
            case .offline:    return "sin conexión"
            case .bridgeOnly: return "ESP32 ok · STM32 sin respuesta"
            case .online:     return "instrumento en línea"
            }
        }
    }

    // MARK: publicado
    @Published var status: DSStatus?
    /// Los nombres de modo que informa el nodo (`GET /modes`). Vacío = todavía
    /// no contestó; la grilla usa `num_modes` del status mientras tanto.
    @Published var modeNames: [String] = []
    @Published var conn: Conn = .offline
    @Published var lastError: String?
    @Published var sending = false

    // MARK: ajustes persistidos
    @Published var host: String {
        didSet { UserDefaults.standard.set(host, forKey: "esp32_host") }
    }
    @Published var port: Int {
        didSet { UserDefaults.standard.set(port, forKey: "esp32_port") }
    }

    var client: ESP32Client { ESP32Client(host: host, port: port) }

    private var pollTask: Task<Void, Never>?
    private var beamTask: Task<Void, Never>?

    init() {
        self.host = UserDefaults.standard.string(forKey: "esp32_host") ?? "192.168.0.50"
        let p = UserDefaults.standard.integer(forKey: "esp32_port")
        self.port = p == 0 ? 80 : p
    }

    // MARK: - polling 1 Hz

    func startPolling() {
        guard pollTask == nil else { return }
        pollTask = Task { [weak self] in
            while !Task.isCancelled {
                await self?.refresh()
                try? await Task.sleep(nanoseconds: 1_000_000_000)
            }
        }
    }

    func stopPolling() {
        pollTask?.cancel()
        pollTask = nil
    }

    func refresh() async {
        do {
            let s = try await client.status()
            status = s
            conn = s.stmOnline ? .online : .bridgeOnly
            lastError = nil
            /* La lista se pide UNA vez por nodo vivo, no en cada poll: no cambia
             * salvo que se actualice el firmware, y ese caso lo cubre que se
             * vacíe al cambiar de nodo o al perder la conexión. */
            if modeNames.isEmpty, s.stmOnline {
                modeNames = (try? await client.modes()) ?? []
            }
        } catch {
            status = nil
            conn = .offline
            modeNames = []      // al volver, se vuelve a preguntar
        }
    }

    // MARK: - acciones

    func setMode(_ idx: Int) {
        Task {
            sending = true
            defer { sending = false }
            do {
                let r = try await client.setMode(idx)
                if !r.isOk { lastError = "modo: \(r.status)" }
                await refresh()
            } catch {
                lastError = "sin respuesta del ESP32"
            }
        }
    }

    /// Envío de haz con debounce de 250 ms — apto para sliders.
    func queueBeam(tx: Bool, az: Int = 0, fxMM: Int = 0, fyMM: Int = 0, fzMM: Int = 1000) {
        beamTask?.cancel()
        beamTask = Task { [client] in
            try? await Task.sleep(nanoseconds: 250_000_000)
            guard !Task.isCancelled else { return }
            await MainActor.run { self.sending = true }
            defer { Task { @MainActor in self.sending = false } }
            do {
                let r = try await client.setBeam(tx: tx, az: az, fx: fxMM, fy: fyMM, fz: fzMM)
                if !r.isOk {
                    await MainActor.run { self.lastError = "haz: \(r.status)" }
                }
            } catch {
                await MainActor.run { self.lastError = "sin respuesta del ESP32" }
            }
        }
    }

    func testConnection() async -> String {
        do {
            _ = try await client.ping()
            let s = try? await client.status()
            if let s, s.stmOnline {
                return "✓ cadena completa: ESP32 + STM32 respondiendo"
            }
            return "ESP32 responde · STM32 sin respuesta (¿UART?)"
        } catch {
            return "✗ sin respuesta en http://\(host)\(port == 80 ? "" : ":\(port)")"
        }
    }
}

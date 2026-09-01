//
//  ESP32Client.swift
//  Domosonica — cliente HTTP del puente ESP32-C3 (red local).
//

import Foundation

struct ESP32Client {
    let base: URL
    private let session: URLSession

    init(host: String, port: Int = 80) {
        var comps = URLComponents()
        comps.scheme = "http"
        comps.host = host
        comps.port = port == 80 ? nil : port
        self.base = comps.url ?? URL(string: "http://192.168.0.50")!

        let cfg = URLSessionConfiguration.ephemeral
        cfg.timeoutIntervalForRequest = 2.5
        cfg.timeoutIntervalForResource = 4.0
        cfg.waitsForConnectivity = false
        self.session = URLSession(configuration: cfg)
    }

    private static let decoder: JSONDecoder = {
        let d = JSONDecoder()
        d.keyDecodingStrategy = .convertFromSnakeCase
        return d
    }()

    // MARK: - endpoints

    func ping() async throws -> Bool {
        let (data, _) = try await session.data(from: base.appendingPathComponent("ping"))
        let r = try Self.decoder.decode(AckResponse.self, from: data)
        return r.isOk
    }

    func status() async throws -> DSStatus {
        let (data, _) = try await session.data(from: base.appendingPathComponent("status"))
        return try Self.decoder.decode(DSStatus.self, from: data)
    }

    /// GET /modes — la lista de modos **del nodo**, que sale de `ui_states[]` del STM32.
    ///
    /// Existe para que la app deje de llevar su propia copia. Tener una lista
    /// escrita a mano acá ya costó caro tres veces en el firmware: un modo nuevo
    /// quedaba sin botón, o con botón pero inalcanzable, y sin ningún error
    /// visible. La app no tiene por qué repetir esa historia.
    func modes() async throws -> [String] {
        let (data, _) = try await session.data(from: base.appendingPathComponent("modes"))
        struct R: Decodable { let ok: Bool; let modes: [String]? }
        let r = try JSONDecoder().decode(R.self, from: data)
        guard r.ok, let m = r.modes else { return [] }
        return m
    }

    /// POST /mode {"mode": idx} — el ESP32 reenvía CMD_SET_MODE por UART y espera ACK.
    func setMode(_ idx: Int) async throws -> AckResponse {
        try await post(path: "mode", body: ["mode": idx])
    }

    /// POST /beam — az en grados (RX), fx/fy/fz en mm (TX, marco del oyente).
    func setBeam(tx: Bool, az: Int = 0, fx: Int = 0, fy: Int = 0, fz: Int = 1000) async throws -> AckResponse {
        try await post(path: "beam",
                       body: ["type": tx ? "tx" : "rx",
                              "az": az, "fx": fx, "fy": fy, "fz": fz])
    }

    /// POST /volume {"db": N} — atenuación maestra del PCM1690, 0..−63 dB.
    func setVolume(db: Int) async throws -> AckResponse {
        try await post(path: "volume", body: ["db": db])
    }

    /// POST /amps {"on": bool} — enable de los 32 PAM8302A.
    ///
    /// Es una acción FÍSICA: los amplificadores arrancan apagados a propósito y
    /// encenderlos con el volumen alto y 15 nodos a la vez tiene consecuencias
    /// audibles. Quien la llame debería confirmar antes.
    func setAmps(on: Bool) async throws -> AckResponse {
        try await post(path: "amps", body: ["on": on])
    }

    /// POST /ingain {"db": N} — ganancia de entrada del AK5538, 0..36 dB.
    func setInGain(db: Int) async throws -> AckResponse {
        try await post(path: "ingain", body: ["db": db])
    }

    /// GET /cal — resultado del último scan del AUTOFOCUS. NO dispara uno nuevo.
    ///
    /// `tofQ8` viene crudo, en muestras Q8 (1/256 de muestra = 81 ns = 28 µm de
    /// camino). Separarlo del camino geométrico es un ajuste por mínimos
    /// cuadrados que se hace afuera — ver tools/calfase.py e issue #21.
    func cal() async throws -> CalResult {
        var req = URLRequest(url: base.appendingPathComponent("cal"))
        req.timeoutInterval = 8      // dos tramas por UART, con su espera
        let (data, _) = try await session.data(for: req)
        return try Self.decoder.decode(CalResult.self, from: data)
    }

    private func post(path: String, body: [String: Any]) async throws -> AckResponse {
        var req = URLRequest(url: base.appendingPathComponent(path))
        req.httpMethod = "POST"
        req.setValue("application/json", forHTTPHeaderField: "Content-Type")
        req.httpBody = try JSONSerialization.data(withJSONObject: body)
        let (data, _) = try await session.data(for: req)
        return try Self.decoder.decode(AckResponse.self, from: data)
    }
}

// MARK: - GET /cal

struct CalResult: Decodable {
    struct Channel: Decodable, Identifiable {
        let ch: Int
        let tofQ8: Int
        let sign: Int
        let quality: Int
        var id: Int { ch }
    }
    let ok: Bool
    let channels: [Channel]?
    let nChannels: Int?
    let state: String?
    let progress: Int?
    let okCount: Int?
    let message: String?
}

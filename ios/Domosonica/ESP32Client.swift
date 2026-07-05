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

    private func post(path: String, body: [String: Any]) async throws -> AckResponse {
        var req = URLRequest(url: base.appendingPathComponent(path))
        req.httpMethod = "POST"
        req.setValue("application/json", forHTTPHeaderField: "Content-Type")
        req.httpBody = try JSONSerialization.data(withJSONObject: body)
        let (data, _) = try await session.data(for: req)
        return try Self.decoder.decode(AckResponse.self, from: data)
    }
}

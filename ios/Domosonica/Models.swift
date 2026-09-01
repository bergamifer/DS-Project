//
//  Models.swift
//  Domosonica — espejo de la API HTTP del ESP32 (firmware_esp32c3/main/main.c)
//
//  GET  /ping    -> {"status":"ok","device_id":"..."}
//  GET  /status  -> DSStatus (stm32:"ok" | "timeout")
//  POST /mode    -> {"mode": 0-11}
//  POST /beam    -> {"type":"tx"|"rx","az":deg,"fx":mm,"fy":mm,"fz":mm}
//

import Foundation
import simd

// MARK: - /status

struct DSStatus: Decodable, Equatable {
    let stm32: String                 // "ok" | "timeout"
    let deviceId: String?
    let protoVer: Int?
    let mode: Int?
    let modeName: String?
    let numModes: Int?
    let rxAz: Int?
    let focus: Focus?
    let underruns: Int?
    let saiErrors: Int?
    let ovf: Int?
    let calState: Int?
    let adcPk: [Int]?
    let uptimeMs: Int?

    // Campos que la vista de flota necesita para detectar divergencias entre
    // nodos. Todos opcionales: un ESP32 con firmware viejo no los manda, y un
    // nodo que responde a medias sigue siendo mejor que un nodo invisible.
    let volDb: Int?
    let ingainDb: Int?
    let amps: Bool?
    let custom: Bool?
    let fwId: String?
    let fwBuild: Int?
    let cpuFillsPct: Double?
    let stm32UptimeMs: Int?
    let calSaved: Bool?
    let adcOvf: Int?
    let outClip: Int?

    struct Focus: Decodable, Equatable {
        let fx: Int; let fy: Int; let fz: Int   // mm, marco del oyente
    }

    var stmOnline: Bool { stm32 == "ok" }
}

struct AckResponse: Decodable {
    let status: String                // "ok" | "nack" | "timeout" | "error"
    let message: String?
    var isOk: Bool { status == "ok" }
}

// MARK: - modos (espejo de MODE_NAMES del ESP32 / ui_states del STM32)
// El índice es lo que viaja en POST /mode. Actualizar junto con el firmware.

struct DSMode: Identifiable {
    let id: Int          // índice en ui_states[]
    let short: String    // nombre para la grilla
    let detail: String   // subtítulo
    let kind: Kind

    enum Kind { case monitor, walker, rx, tx, autofocus, diag }
}

enum DSModes {
    /// Etiquetas CURADAS para la app: nombre corto y una línea de contexto.
    ///
    /// Esto **ya no es la lista de modos** — la lista la da el nodo (`GET /modes`,
    /// que sale de `ui_states[]` del STM32; issue #20). Acá quedan sólo los
    /// nombres cortos y las descripciones, que son información de la app: el
    /// firmware llama al 17 "BARRIDO VERTICAL 800Hz arriba<->abajo @1m" y en un
    /// botón entra "Barrido Y".
    ///
    /// Un índice sin entrada acá no desaparece: se muestra con el nombre que
    /// mandó el firmware. Ese es el punto — que la app nunca pueda esconder un
    /// modo que el equipo tiene.
    static let curated: [DSMode] = [

    /// Espejo de `ui_states[]` (firmware/Core/Src/main.c) y de `MODE_NAMES[]`
    /// (firmware_esp32c3). Es la CUARTA copia de la misma lista, y la
    /// duplicación ya causó un bug silencioso: el modo 21 se agregó al STM32,
    /// el ESP32 lo rechazaba devolviendo `{"status":"ok"}`, y costó una
    /// escalera de volumen medida en el modo equivocado.
    ///
    /// Por eso la grilla NO se dibuja desde esta lista sino desde el
    /// `num_modes` que informa el nodo (ver `grid(for:)`): si el firmware
    /// tiene más modos de los que la app conoce, aparecen igual con su índice
    /// en vez de desaparecer sin aviso. La solución de fondo es que el
    /// firmware publique la lista — issue #20.

        DSMode(id: 0,  short: "Monitor",     detail: "ADC→DAC +30dB",        kind: .monitor),
        DSMode(id: 1,  short: "Walker",      detail: "tono 1s por canal",    kind: .walker),
        DSMode(id: 2,  short: "RX 0°",       detail: "escucha frontal",      kind: .rx),
        DSMode(id: 3,  short: "RX +30°",     detail: "escucha derecha",      kind: .rx),
        DSMode(id: 4,  short: "RX −30°",     detail: "escucha izquierda",    kind: .rx),
        DSMode(id: 5,  short: "RX +60°",     detail: "lateral derecha",      kind: .rx),
        DSMode(id: 6,  short: "RX −60°",     detail: "lateral izquierda",    kind: .rx),
        DSMode(id: 7,  short: "Foco lejos",  detail: "TX 2k @ 2 m",          kind: .tx),
        DSMode(id: 8,  short: "Foco izq",    detail: "TX 2k −0,5 m @ 1 m",   kind: .tx),
        DSMode(id: 9,  short: "Foco der",    detail: "TX 2k +0,5 m @ 1 m",   kind: .tx),
        DSMode(id: 10, short: "Foco cerca",  detail: "TX 2k centro @ 0,3 m", kind: .tx),
        DSMode(id: 11, short: "AUTOFOCUS",   detail: "scan con micrófono",   kind: .autofocus),
        DSMode(id: 12, short: "Tono 2k",     detail: "foco @ 1 m, ±14 cm",   kind: .tx),
        DSMode(id: 13, short: "Tono 1k",     detail: "foco @ 1 m, ±27 cm",   kind: .tx),
        DSMode(id: 14, short: "MULTIFOCUS",  detail: "4 tonos, 4 puntos",    kind: .tx),
        DSMode(id: 15, short: "8 entradas",  detail: "cada entrada, su foco", kind: .tx),
        DSMode(id: 16, short: "Barrido X",   detail: "el foco cruza la sala", kind: .tx),
        DSMode(id: 17, short: "Barrido Y",   detail: "800 Hz — el test de aliasing vertical", kind: .tx),
        DSMode(id: 18, short: "Foco medido", detail: "usa el AUTOFOCUS guardado", kind: .autofocus),
        DSMode(id: 19, short: "LOOPBACK",    detail: "medición eléctrica del lazo", kind: .diag),
        DSMode(id: 20, short: "Barrido f",   detail: "respuesta 20 Hz – 20 kHz", kind: .diag),
        DSMode(id: 21, short: "Tono fijo",   detail: "500 Hz — medir con tester", kind: .diag),
    ]
    static let autofocusIndex = 11

    /// La grilla a dibujar: **la lista del nodo**, decorada con lo curado.
    ///
    /// - `names`: lo que devolvió `GET /modes`. Manda esto.
    /// - `numModes`: respaldo si `/modes` todavía no contestó — el `/status` ya
    ///   trae el número, así que al menos la cantidad de botones es correcta.
    ///
    /// Cuando no hay nombre curado se usa el del firmware, acortado. Feo pero
    /// alcanzable, que es lo único que no se negocia.
    static func grid(names: [String], numModes: Int? = nil) -> [DSMode] {
        let n = names.isEmpty ? (numModes ?? curated.count) : names.count
        return (0..<max(n, 1)).map { i in
            if i < curated.count { return curated[i] }
            let raw = i < names.count ? names[i] : "Modo \(i)"
            return DSMode(id: i, short: shorten(raw), detail: raw, kind: .diag)
        }
    }

    /// El firmware nombra para el cable serie, no para un botón. 14 caracteres
    /// es lo que entra sin que la grilla se rompa.
    private static func shorten(_ s: String) -> String {
        s.count <= 14 ? s : String(s.prefix(13)) + "…"
    }
}

// MARK: - estados de calibración AUTOFOCUS (cal_state_t del STM32)

enum CalState: Int {
    case idle = 0, settle, arm, capt, corr, done

    var label: String {
        switch self {
        case .idle:   return "inactivo"
        case .settle: return "silencio (sala decae)"
        case .arm:    return "armado — por disparar chirp"
        case .capt:   return "capturando micrófono"
        case .corr:   return "correlacionando"
        case .done:   return "foco medido ✓ tono enfocado sonando"
        }
    }
    var isActive: Bool { self != .idle && self != .done }
}

// MARK: - geometría real del muro (espejo de firmware/Core/Src/main.c)

enum SpeakerWall {
    static let rows = 4
    static let cols = 8
    static let xPitch: Float = 0.080     // m
    static let yPitch: Float = 0.160     // m
    static let rowDX:  Float = 0.040     // filas 2&4 (r=1,3) corridas +40mm
    static let soundSpeed: Float = 343.0 // m/s

    /// Posiciones de los 32 parlantes, centradas en el origen (marco del oyente,
    /// de frente a los conos). ch = fila*8 + col, fila 0 abajo.
    static let positions: [SIMD3<Float>] = {
        var p: [SIMD3<Float>] = []
        let cx = (Float(cols - 1) * xPitch + rowDX) / 2
        let cy = Float(rows - 1) * yPitch / 2
        for r in 0..<rows {
            for c in 0..<cols {
                let dx: Float = (r == 1 || r == 3) ? rowDX : 0
                p.append(SIMD3(Float(c) * xPitch + dx - cx,
                               Float(r) * yPitch - cy,
                               0))
            }
        }
        return p
    }()

    /// Delays focales por canal (s): el más lejano dispara primero.
    static func delays(to focus: SIMD3<Float>) -> [Float] {
        let d = positions.map { simd_distance($0, focus) }
        let dmax = d.max() ?? 0
        return d.map { (dmax - $0) / soundSpeed }
    }
}

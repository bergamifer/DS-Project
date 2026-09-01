//
//  RIR.swift
//  Domosonica — medición de respuesta al impulso de la sala (método Farina):
//  sweep exponencial por el parlante del iPhone → grabación con el micrófono
//  (modo .measurement) → deconvolución FFT → curva de Schroeder → RT60.
//
//  Uso: diagnóstico "¿esta sala focaliza?" antes de montar el instrumento.
//  Futuro: excitación por el instrumento (chirp del walker) por parlante.
//

import Foundation
import AVFoundation
import Accelerate

// MARK: - resultado

struct RIRResult {
    let fs: Double
    let ir: [Float]                 // impulso normalizado (desde el pico directo)
    let edcDB: [Float]              // curva de decaimiento (dB), muestreada
    let edcDt: Float                // paso temporal de edcDB (s)
    let rt60Broadband: Float?
    let bands: [(hz: Int, rt60: Float?)]
    let verdict: String
    let midRT: Float?
}

// MARK: - buffer compartido con el hilo de audio

/// Lo que graba el tap de AVAudioEngine, protegido por un candado.
///
/// Existe como tipo propio y no como un par `var` + `NSLock` sueltos por una
/// razón concreta: el tap corre en el hilo de audio en tiempo real y el resto
/// del medidor corre en el MainActor, así que tomar el candado a mano desde los
/// dos lados es exactamente el patrón que Swift 6 rechaza —`lock()` en contexto
/// asíncrono es un error, no un aviso—. Con métodos SÍNCRONOS que se encargan
/// del candado adentro, el llamador async nunca lo toca y el compilador queda
/// conforme sin que haya que apagar ninguna comprobación.
///
/// `@unchecked Sendable` es la afirmación explícita de que la seguridad la da el
/// candado y no el compilador. Es el lugar correcto para esa afirmación: acá
/// está, en doce líneas que se pueden auditar de un vistazo.
private final class RecordBuffer: @unchecked Sendable {
    private var samples: [Float] = []
    private let lock = NSLock()

    func reset(capacity: Int) {
        lock.lock(); defer { lock.unlock() }
        samples = []
        samples.reserveCapacity(capacity)
    }

    func append(_ p: UnsafePointer<Float>, count: Int) {
        lock.lock(); defer { lock.unlock() }
        samples.append(contentsOf: UnsafeBufferPointer(start: p, count: count))
    }

    func take() -> [Float] {
        lock.lock(); defer { lock.unlock() }
        return samples
    }
}

// MARK: - medidor

@MainActor
final class RIRMeter: ObservableObject {
    enum State: Equatable {
        case idle
        case running(String)        // etiqueta de progreso
        case done
        case failed(String)
    }

    @Published var state: State = .idle
    @Published var result: RIRResult?

    private let engine = AVAudioEngine()
    private let player = AVAudioPlayerNode()
    private let recorded = RecordBuffer()

    // parámetros del sweep
    private let sweepT = 3.0        // s
    private let tailT = 1.8         // s de cola para capturar la reverberación
    private let f1 = 60.0           // Hz (el parlante del iPhone no baja de ahí)

    private nonisolated static func schedule(_ b: AVAudioPCMBuffer, on p: AVAudioPlayerNode) {
        p.scheduleBuffer(b, at: nil, options: [], completionHandler: nil)
    }

    func measure() async {
        state = .running("pidiendo micrófono…")
        let granted = await AVAudioApplication.requestRecordPermission()
        guard granted else {
            state = .failed("Sin permiso de micrófono (Ajustes → Privacidad).")
            return
        }

        do {
            let session = AVAudioSession.sharedInstance()
            try session.setCategory(.playAndRecord, mode: .measurement,
                                    options: [.defaultToSpeaker])
            try session.setActive(true)

            let inFormat = engine.inputNode.outputFormat(forBus: 0)
            let fs = inFormat.sampleRate
            guard fs > 8000 else {
                state = .failed("Micrófono no disponible.")
                return
            }

            // --- sweep ---
            let f2 = min(16000.0, fs * 0.42)
            let sweep = Self.makeSweep(fs: fs, T: sweepT, f1: f1, f2: f2)
            guard let outFormat = AVAudioFormat(standardFormatWithSampleRate: fs, channels: 1),
                  let buf = AVAudioPCMBuffer(pcmFormat: outFormat,
                                             frameCapacity: AVAudioFrameCount(sweep.count)) else {
                state = .failed("No pude crear el buffer de audio.")
                return
            }
            buf.frameLength = AVAudioFrameCount(sweep.count)
            sweep.withUnsafeBufferPointer { src in
                buf.floatChannelData![0].update(from: src.baseAddress!, count: sweep.count)
            }

            // --- grabar + reproducir ---
            recorded.reset(capacity: Int(fs * (sweepT + tailT + 1)))

            engine.inputNode.removeTap(onBus: 0)
            engine.inputNode.installTap(onBus: 0, bufferSize: 4096, format: inFormat) { [weak self] pcm, _ in
                guard let self, let ch = pcm.floatChannelData else { return }
                self.recorded.append(ch[0], count: Int(pcm.frameLength))
            }

            if engine.attachedNodes.contains(player) == false { engine.attach(player) }
            engine.connect(player, to: engine.mainMixerNode, format: outFormat)
            engine.prepare()
            try engine.start()

            state = .running("silencio — midiendo…")
            /* Se agenda desde un helper síncrono a propósito. La versión async de
             * scheduleBuffer no vuelve hasta que TERMINA de sonar, y acá hace
             * falta que vuelva enseguida para poder ir informando el progreso en
             * el lazo de abajo. */
            Self.schedule(buf, on: player)
            player.play()

            let total = sweepT + tailT
            let t0 = Date()
            while Date().timeIntervalSince(t0) < total {
                try? await Task.sleep(nanoseconds: 100_000_000)
                let p = Date().timeIntervalSince(t0) / total
                state = .running(String(format: "midiendo… %d%%", Int(p * 100)))
            }

            player.stop()
            engine.inputNode.removeTap(onBus: 0)
            engine.stop()
            try? session.setActive(false, options: .notifyOthersOnDeactivation)

            state = .running("procesando…")
            let rec = recorded.take()
            guard rec.count > sweep.count else {
                state = .failed("Grabación demasiado corta.")
                return
            }

            // el procesamiento pesado, fuera del main actor
            let res = await Task.detached(priority: .userInitiated) {
                Self.process(rec: rec, sweep: sweep, fs: fs)
            }.value

            if let res {
                result = res
                state = .done
            } else {
                state = .failed("No encontré un impulso claro (¿volumen muy bajo?).")
            }
        } catch {
            engine.inputNode.removeTap(onBus: 0)
            engine.stop()
            state = .failed("Audio: \(error.localizedDescription)")
        }
    }

    // MARK: - DSP (nonisolated, corre en background)

    nonisolated static func makeSweep(fs: Double, T: Double, f1: Double, f2: Double) -> [Float] {
        let n = Int(fs * T)
        let L = T / log(f2 / f1)
        var out = [Float](repeating: 0, count: n)
        let fadeN = Int(fs * 0.03)
        for i in 0..<n {
            let t = Double(i) / fs
            let ph = 2 * .pi * f1 * L * (exp(t / L) - 1)
            var a = 0.6
            if i < fadeN { a *= 0.5 * (1 - cos(.pi * Double(i) / Double(fadeN))) }
            if i > n - fadeN { a *= 0.5 * (1 - cos(.pi * Double(n - i) / Double(fadeN))) }
            out[i] = Float(a * sin(ph))
        }
        return out
    }

    nonisolated static func process(rec: [Float], sweep: [Float], fs: Double) -> RIRResult? {
        // filtro inverso (Farina): sweep invertido en el tiempo con envolvente -6 dB/oct
        let T = Double(sweep.count) / fs
        let L = T / log(min(16000.0, fs * 0.42) / 60.0)
        var inv = [Float](repeating: 0, count: sweep.count)
        for i in 0..<sweep.count {
            let t = Double(i) / fs
            inv[sweep.count - 1 - i] = sweep[i] * Float(exp(-t / L))
        }

        guard var full = fftConvolve(rec, inv) else { return nil }

        // pico directo
        var maxV: Float = 0
        var maxI = 0
        for (i, v) in full.enumerated() where abs(v) > maxV { maxV = abs(v); maxI = i }
        guard maxV > 0 else { return nil }

        // IR desde el pico (2 s máx)
        let irLen = min(Int(fs * 2.0), full.count - maxI)
        guard irLen > Int(fs * 0.25) else { return nil }
        var ir = Array(full[maxI..<(maxI + irLen)])
        var scale = 1.0 as Float / maxV
        vDSP_vsmul(ir, 1, &scale, &ir, 1, vDSP_Length(ir.count))
        full = []

        // chequeo de SNR: energía del impulso vs cola final
        let head = ir.prefix(Int(fs * 0.05)).reduce(Float(0)) { $0 + $1 * $1 }
        let tail = ir.suffix(Int(fs * 0.2)).reduce(Float(0)) { $0 + $1 * $1 }
        guard head > tail * 4 else { return nil }

        // RT60 broadband + por bandas
        let broad = rt60(from: ir, fs: fs)
        var bands: [(Int, Float?)] = []
        for hz in [250, 500, 1000, 2000, 4000] {
            let f = bandpass(ir, fs: fs, fc: Double(hz))
            bands.append((hz, rt60(from: f, fs: fs)))
        }

        // EDC para graficar (downsample a ~300 puntos)
        let (edc, dt) = edcCurve(ir, fs: fs, points: 300)

        let mids = bands.filter { [500, 1000, 2000].contains($0.0) }.compactMap(\.1)
        let mid: Float? = mids.isEmpty ? nil : mids.reduce(0, +) / Float(mids.count)
        let verdict: String
        switch mid {
        case .none:                 verdict = "No pude estimar RT60 con confianza — probá con más volumen."
        case .some(let m) where m < 0.6:  verdict = "Sala seca (RT60 ≈ \(String(format: "%.1f", m)) s) — foco nítido, ideal."
        case .some(let m) where m < 1.2:  verdict = "Sala apta (RT60 ≈ \(String(format: "%.1f", m)) s) — el foco se forma bien."
        case .some(let m) where m < 1.8:  verdict = "Sala reverberante (RT60 ≈ \(String(format: "%.1f", m)) s) — el foco se ensancha; conviene absorber."
        case .some(let m):          verdict = "Muy reverberante (RT60 ≈ \(String(format: "%.1f", m)) s) — la sala va a dominar sobre el foco."
        }

        return RIRResult(fs: fs, ir: downsample(ir, to: 600), edcDB: edc, edcDt: dt,
                         rt60Broadband: broad, bands: bands, verdict: verdict, midRT: mid)
    }

    // convolución por DFT compleja (vDSP_DFT: simple y sin formato empaquetado)
    nonisolated static func fftConvolve(_ a: [Float], _ b: [Float]) -> [Float]? {
        let outLen = a.count + b.count - 1
        var n = 1
        while n < outLen { n <<= 1 }
        guard let fwd = vDSP_DFT_zop_CreateSetup(nil, vDSP_Length(n), .FORWARD),
              let inv = vDSP_DFT_zop_CreateSetup(fwd, vDSP_Length(n), .INVERSE) else { return nil }
        defer { vDSP_DFT_DestroySetup(fwd); vDSP_DFT_DestroySetup(inv) }

        func dft(_ x: [Float], _ setup: OpaquePointer,
                 _ re: inout [Float], _ im: inout [Float]) {
            var inRe = [Float](repeating: 0, count: n)
            var inIm = [Float](repeating: 0, count: n)
            inRe.replaceSubrange(0..<x.count, with: x)
            vDSP_DFT_Execute(setup, &inRe, &inIm, &re, &im)
        }

        var aRe = [Float](repeating: 0, count: n), aIm = [Float](repeating: 0, count: n)
        var bRe = [Float](repeating: 0, count: n), bIm = [Float](repeating: 0, count: n)
        dft(a, fwd, &aRe, &aIm)
        dft(b, fwd, &bRe, &bIm)

        // multiplicación compleja
        var pRe = [Float](repeating: 0, count: n)
        var pIm = [Float](repeating: 0, count: n)
        for i in 0..<n {
            pRe[i] = aRe[i] * bRe[i] - aIm[i] * bIm[i]
            pIm[i] = aRe[i] * bIm[i] + aIm[i] * bRe[i]
        }

        var oRe = [Float](repeating: 0, count: n)
        var oIm = [Float](repeating: 0, count: n)
        vDSP_DFT_Execute(inv, &pRe, &pIm, &oRe, &oIm)
        var scale = 1.0 as Float / Float(n)
        vDSP_vsmul(oRe, 1, &scale, &oRe, 1, vDSP_Length(n))
        return Array(oRe[0..<outLen])
    }

    /// Curva de Schroeder + ajuste T20 → RT60.
    nonisolated static func rt60(from ir: [Float], fs: Double) -> Float? {
        var energy = [Double](repeating: 0, count: ir.count)
        var acc = 0.0
        for i in (0..<ir.count).reversed() {
            acc += Double(ir[i]) * Double(ir[i])
            energy[i] = acc
        }
        guard acc > 0 else { return nil }
        let total = acc
        func timeAt(_ db: Double) -> Double? {
            let target = total * pow(10, db / 10)
            for i in 0..<energy.count where energy[i] <= target {
                return Double(i) / fs
            }
            return nil
        }
        guard let t5 = timeAt(-5), let t25 = timeAt(-25), t25 > t5 else { return nil }
        return Float(3 * (t25 - t5))
    }

    nonisolated static func edcCurve(_ ir: [Float], fs: Double, points: Int) -> ([Float], Float) {
        var energy = [Double](repeating: 0, count: ir.count)
        var acc = 0.0
        for i in (0..<ir.count).reversed() {
            acc += Double(ir[i]) * Double(ir[i])
            energy[i] = acc
        }
        guard acc > 0 else { return ([], 0) }
        let step = max(ir.count / points, 1)
        var out: [Float] = []
        var i = 0
        while i < ir.count {
            out.append(Float(10 * log10(max(energy[i] / acc, 1e-9))))
            i += step
        }
        return (out, Float(Double(step) / fs))
    }

    /// Pasabanda RBJ (1 octava aprox) aplicado hacia adelante.
    nonisolated static func bandpass(_ x: [Float], fs: Double, fc: Double) -> [Float] {
        let w0 = 2 * Double.pi * fc / fs
        let q = 1.414
        let alpha = sin(w0) / (2 * q)
        let b0 = alpha, b1 = 0.0, b2 = -alpha
        let a0 = 1 + alpha, a1 = -2 * cos(w0), a2 = 1 - alpha
        var y = [Float](repeating: 0, count: x.count)
        var x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0
        for i in 0..<x.count {
            let xi = Double(x[i])
            let yi = (b0 * xi + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2) / a0
            y[i] = Float(yi)
            x2 = x1; x1 = xi
            y2 = y1; y1 = yi
        }
        return y
    }

    nonisolated static func downsample(_ x: [Float], to n: Int) -> [Float] {
        guard x.count > n else { return x }
        let step = x.count / n
        var out: [Float] = []
        out.reserveCapacity(n)
        var i = 0
        while i < x.count {
            // pico local para que el gráfico no pierda transientes
            var m: Float = 0
            for j in i..<min(i + step, x.count) where abs(x[j]) > abs(m) { m = x[j] }
            out.append(m)
            i += step
        }
        return out
    }
}

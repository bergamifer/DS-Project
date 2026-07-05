//
//  SpatialView.swift
//  Domosonica — preview de la v2: la sala en 3D con el muro real (4×8 ladrillo,
//  80/160 mm, filas 2&4 +40 mm) y un foco arrastrable. Cada parlante se ilumina
//  según su delay focal — la misma cuenta que hace el firmware.
//
//  v2 completa (futuro): ARKit + anclaje acústico → colocar focos caminando la
//  sala. Ver docs/app_ios.md.
//

import SwiftUI
import SceneKit
import simd

// MARK: - modelo de escena

@MainActor
final class SpatialModel: ObservableObject {
    let scene = SCNScene()
    let cameraNode = SCNNode()
    private var speakerNodes: [SCNNode] = []
    private var focusNode = SCNNode()

    @Published var focus = SIMD3<Float>(0, 0, 1.0) {
        didSet { applyFocus() }
    }
    @Published var dtMaxMs: Float = 0
    @Published var followMe = false {
        didSet { followMe ? startFollow() : stopFollow() }
    }
    @Published var streamWhileFollow = false

    var sendBeam: ((SIMD3<Float>) -> Void)?
    private var followTask: Task<Void, Never>?
    private var lastStream = Date.distantPast

    init() { buildScene() }

    private func buildScene() {
        scene.background.contents = UIColor(Theme.ink)

        // cámara
        let cam = SCNCamera()
        cam.zNear = 0.01
        cam.fieldOfView = 55
        cameraNode.camera = cam
        cameraNode.position = SCNVector3(1.15, 0.42, 2.9)
        cameraNode.look(at: SCNVector3(0, -0.05, 0.7))
        scene.rootNode.addChildNode(cameraNode)

        // luces
        let amb = SCNLight(); amb.type = .ambient; amb.intensity = 260
        amb.color = UIColor(white: 0.9, alpha: 1)
        let ambNode = SCNNode(); ambNode.light = amb
        scene.rootNode.addChildNode(ambNode)

        let omni = SCNLight(); omni.type = .omni; omni.intensity = 480
        let omniNode = SCNNode(); omniNode.light = omni
        omniNode.position = SCNVector3(1.2, 1.6, 2.2)
        scene.rootNode.addChildNode(omniNode)

        // piso
        let floor = SCNPlane(width: 7, height: 7)
        floor.firstMaterial?.diffuse.contents = UIColor(Theme.ink2)
        let floorNode = SCNNode(geometry: floor)
        floorNode.eulerAngles.x = -.pi / 2
        floorNode.position = SCNVector3(0, -0.48, 1.5)
        scene.rootNode.addChildNode(floorNode)

        // panel de fondo (la caja MDF)
        let panel = SCNBox(width: 0.72, height: 0.58, length: 0.03, chamferRadius: 0.008)
        panel.firstMaterial?.diffuse.contents = UIColor(red: 0.42, green: 0.33, blue: 0.23, alpha: 1)
        let panelNode = SCNNode(geometry: panel)
        panelNode.position = SCNVector3(0, 0, -0.02)
        scene.rootNode.addChildNode(panelNode)

        // 32 parlantes en la geometría real del firmware
        for p in SpeakerWall.positions {
            let g = SCNBox(width: 0.055, height: 0.12, length: 0.035, chamferRadius: 0.012)
            let m = SCNMaterial()
            m.diffuse.contents = UIColor(red: 0.20, green: 0.16, blue: 0.12, alpha: 1)
            m.emission.contents = UIColor.black
            g.firstMaterial = m
            let n = SCNNode(geometry: g)
            n.position = SCNVector3(p.x, p.y, p.z)
            scene.rootNode.addChildNode(n)
            speakerNodes.append(n)
        }

        // el foco: esfera que respira
        let sphere = SCNSphere(radius: 0.045)
        sphere.firstMaterial?.diffuse.contents = UIColor(Theme.wave)
        sphere.firstMaterial?.emission.contents = UIColor(Theme.waveHot)
        focusNode = SCNNode(geometry: sphere)
        let up = SCNAction.scale(to: 1.14, duration: 0.9)
        up.timingMode = .easeInEaseOut
        let down = SCNAction.scale(to: 1.0, duration: 0.9)
        down.timingMode = .easeInEaseOut
        focusNode.runAction(.repeatForever(.sequence([up, down])))
        scene.rootNode.addChildNode(focusNode)

        applyFocus()
    }

    /// Foco → posición de la esfera + emisión de cada parlante según su delay.
    /// Misma cuenta que Beam_UpdateTxFocus() en el STM32.
    private func applyFocus() {
        focusNode.position = SCNVector3(focus.x, focus.y, focus.z)
        let delays = SpeakerWall.delays(to: focus)
        let dmax = delays.max() ?? 1
        dtMaxMs = dmax * 1000
        for (i, n) in speakerNodes.enumerated() {
            let t = dmax > 0 ? CGFloat(delays[i] / dmax) : 0
            // el que dispara primero (delay máximo) brilla más
            n.geometry?.firstMaterial?.emission.contents =
                UIColor(red: 0.10 + 0.34 * t, green: 0.22 + 0.66 * t, blue: 0.20 + 0.61 * t,
                        alpha: 1).withAlphaComponent(0.15 + 0.85 * t)
        }
    }

    // MARK: follow-me (demo): el foco camina una lissajous por la sala

    private func startFollow() {
        followTask = Task { [weak self] in
            var t: Float = 0
            while !Task.isCancelled {
                t += 0.05
                let f = SIMD3<Float>(0.65 * sin(0.50 * t),
                                     0.14 * sin(0.21 * t),
                                     1.05 + 0.55 * sin(0.32 * t + 1.0))
                await MainActor.run { [weak self] in
                    guard let self else { return }
                    self.focus = f
                    if self.streamWhileFollow,
                       Date().timeIntervalSince(self.lastStream) > 0.5 {
                        self.lastStream = Date()
                        self.sendBeam?(f)
                    }
                }
                try? await Task.sleep(nanoseconds: 50_000_000)
            }
        }
    }

    private func stopFollow() {
        followTask?.cancel()
        followTask = nil
    }
}

// MARK: - vista

struct SpatialView: View {
    @EnvironmentObject var app: AppState
    @StateObject private var model = SpatialModel()
    @State private var dragBase: SIMD3<Float>?

    var body: some View {
        VStack(spacing: 0) {
            ScreenHeader(subtitle: "vista espacial · preview de la v2 (AR)")
                .padding(16)

            ZStack(alignment: .bottom) {
                SceneView(scene: model.scene,
                          pointOfView: model.cameraNode,
                          options: [.rendersContinuously])
                    .gesture(dragGesture)

                // lectura física
                VStack(spacing: 8) {
                    PhysReadout(text: String(format: "foco  x %+.2f  y %+.2f  z %.2f m   ·   Δt máx %.2f ms",
                                             model.focus.x, model.focus.y, model.focus.z, model.dtMaxMs))
                    HStack(spacing: 10) {
                        Toggle(isOn: $model.followMe) {
                            Text("follow-me (demo)").font(Theme.mono(11))
                        }
                        .toggleStyle(.button)
                        .tint(Theme.wave)

                        if model.followMe {
                            Toggle(isOn: $model.streamWhileFollow) {
                                Text("→ instrumento").font(Theme.mono(11))
                            }
                            .toggleStyle(.button)
                            .tint(Theme.wood)
                        }

                        Spacer()

                        Button {
                            sendCurrent()
                        } label: {
                            Label("Enviar", systemImage: "dot.radiowaves.up.forward")
                                .font(Theme.mono(11, weight: .medium))
                        }
                        .buttonStyle(.borderedProminent)
                        .tint(Theme.wave.opacity(0.85))
                        .disabled(app.conn == .offline)
                    }
                }
                .padding(12)
                .background(.ultraThinMaterial.opacity(0.85))
            }

            // pie: qué es esto
            VStack(alignment: .leading, spacing: 4) {
                Text("ARRASTRÁ PARA MOVER EL FOCO — cada parlante se ilumina según su delay focal (la cuenta real del firmware).")
                Text("v2 completa: ARKit + un anclaje acústico → colocar focos caminando la sala. Ver docs/app_ios.md.")
            }
            .font(Theme.mono(9.5))
            .foregroundStyle(Theme.muted)
            .frame(maxWidth: .infinity, alignment: .leading)
            .padding(14)
        }
        .background(Theme.ink)
        .onAppear {
            model.sendBeam = { [weak app] f in
                app?.queueBeam(tx: true,
                               fxMM: Int(f.x * 1000),
                               fyMM: Int(f.y * 1000),
                               fzMM: Int(f.z * 1000))
            }
        }
    }

    private var dragGesture: some Gesture {
        DragGesture(minimumDistance: 2)
            .onChanged { g in
                if dragBase == nil { dragBase = model.focus }
                guard let base = dragBase else { return }
                let nx = base.x + Float(g.translation.width) / 160.0
                let nz = base.z - Float(g.translation.height) / 130.0
                model.focus = SIMD3(max(-1.5, min(1.5, nx)),
                                    base.y,
                                    max(0.20, min(3.0, nz)))
            }
            .onEnded { _ in
                dragBase = nil
                if app.conn != .offline { sendCurrent() }
            }
    }

    private func sendCurrent() {
        app.queueBeam(tx: true,
                      fxMM: Int(model.focus.x * 1000),
                      fyMM: Int(model.focus.y * 1000),
                      fzMM: Int(model.focus.z * 1000))
    }
}

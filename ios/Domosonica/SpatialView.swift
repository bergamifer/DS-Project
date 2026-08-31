//
//  SpatialView.swift
//  Domosonica — la sala en 3D: muros multi-nodo, foco arrastrable, cámara
//  orbital, focos guardados y trayectorias grabables (partitura espacial v0).
//
//  Cada parlante se ilumina según su delay focal — la misma cuenta que hace el
//  firmware (Beam_UpdateTxFocus). Con varios nodos, cada muro calcula su
//  gradiente en su marco local y el envío se hace en fan-out (NodesStore).
//
//  v2 completa (futuro): ARKit + anclaje acústico → docs/app_ios.md.
//

import SwiftUI
import SceneKit
import simd

// MARK: - modelo de escena

@MainActor
final class SpatialModel: ObservableObject {
    let scene = SCNScene()
    let cameraNode = SCNNode()
    let nodesStore: NodesStore

    private var wallParent = SCNNode()
    private var speakersByNode: [UUID: [SCNNode]] = [:]
    private var focusNode = SCNNode()
    private var trailNode = SCNNode()

    // cámara orbital
    @Published var camAz: Float = 0.35 { didSet { updateCamera() } }
    @Published var camEl: Float = 0.16 { didSet { updateCamera() } }
    @Published var camR: Float = 3.4 { didSet { updateCamera() } }
    private let camTarget = SIMD3<Float>(0, 0.05, 0.8)

    @Published var focus = SIMD3<Float>(0, 0, 1.0) {
        didSet { applyFocus() }
    }
    @Published var dtMaxMs: Float = 0

    // focos guardados
    @Published var saved: [SavedFocus] { didSet { persistSaved() } }

    // trayectoria
    @Published var traj: [TrajPoint] { didSet { persistTraj(); rebuildTrail() } }
    @Published var isRecording = false
    @Published var isPlaying = false
    @Published var playT: Float = 0
    @Published var streamToInstrument = false
    @Published var followMe = false { didSet { followMe ? startFollow() : stopFollow() } }

    private var recStart: Date?
    private var playTask: Task<Void, Never>?
    private var followTask: Task<Void, Never>?

    init(nodesStore: NodesStore) {
        self.nodesStore = nodesStore
        if let d = UserDefaults.standard.data(forKey: "ds_saved_focus"),
           let s = try? JSONDecoder().decode([SavedFocus].self, from: d) { saved = s } else { saved = [] }
        if let d = UserDefaults.standard.data(forKey: "ds_traj"),
           let t = try? JSONDecoder().decode([TrajPoint].self, from: d) { traj = t } else { traj = [] }
        buildScene()
        rebuildWalls()
        rebuildTrail()
        applyFocus()
    }

    private func persistSaved() {
        if let d = try? JSONEncoder().encode(saved) { UserDefaults.standard.set(d, forKey: "ds_saved_focus") }
    }
    private func persistTraj() {
        if let d = try? JSONEncoder().encode(traj) { UserDefaults.standard.set(d, forKey: "ds_traj") }
    }

    // MARK: escena base

    private func buildScene() {
        scene.background.contents = UIColor(Theme.ink)

        let cam = SCNCamera()
        cam.zNear = 0.01
        cam.fieldOfView = 55
        cameraNode.camera = cam
        scene.rootNode.addChildNode(cameraNode)
        updateCamera()

        let amb = SCNLight(); amb.type = .ambient; amb.intensity = 260
        amb.color = UIColor(white: 0.9, alpha: 1)
        let ambNode = SCNNode(); ambNode.light = amb
        scene.rootNode.addChildNode(ambNode)

        let omni = SCNLight(); omni.type = .omni; omni.intensity = 480
        let omniNode = SCNNode(); omniNode.light = omni
        omniNode.position = SCNVector3(1.2, 1.8, 2.4)
        scene.rootNode.addChildNode(omniNode)

        // piso con grilla sutil (líneas cada 0,5 m)
        let floor = SCNPlane(width: 10, height: 10)
        floor.firstMaterial?.diffuse.contents = UIColor(Theme.ink2)
        let floorNode = SCNNode(geometry: floor)
        floorNode.eulerAngles.x = -.pi / 2
        floorNode.position = SCNVector3(0, -0.55, 1.5)
        scene.rootNode.addChildNode(floorNode)
        for i in -6...12 {
            let z = Float(i) * 0.5
            scene.rootNode.addChildNode(gridLine(from: SCNVector3(-5, -0.548, z),
                                                 to: SCNVector3(5, -0.548, z)))
        }
        for i in -10...10 {
            let x = Float(i) * 0.5
            scene.rootNode.addChildNode(gridLine(from: SCNVector3(x, -0.548, -1.5),
                                                 to: SCNVector3(x, -0.548, 4.5)))
        }

        scene.rootNode.addChildNode(wallParent)
        scene.rootNode.addChildNode(trailNode)

        // el foco: esfera que respira
        let sphere = SCNSphere(radius: 0.045)
        sphere.firstMaterial?.diffuse.contents = UIColor(Theme.wave)
        sphere.firstMaterial?.emission.contents = UIColor(Theme.waveHot)
        focusNode = SCNNode(geometry: sphere)
        let up = SCNAction.scale(to: 1.14, duration: 0.9); up.timingMode = .easeInEaseOut
        let down = SCNAction.scale(to: 1.0, duration: 0.9); down.timingMode = .easeInEaseOut
        focusNode.runAction(.repeatForever(.sequence([up, down])))
        scene.rootNode.addChildNode(focusNode)
    }

    private func gridLine(from a: SCNVector3, to b: SCNVector3) -> SCNNode {
        let src = SCNGeometrySource(vertices: [a, b])
        let idx: [Int32] = [0, 1]
        let el = SCNGeometryElement(indices: idx, primitiveType: .line)
        let g = SCNGeometry(sources: [src], elements: [el])
        g.firstMaterial?.diffuse.contents = UIColor(Theme.line).withAlphaComponent(0.5)
        g.firstMaterial?.lightingModel = .constant
        return SCNNode(geometry: g)
    }

    private func updateCamera() {
        let p = camTarget + camR * SIMD3(sin(camAz) * cos(camEl), sin(camEl), cos(camAz) * cos(camEl))
        cameraNode.position = SCNVector3(p.x, p.y, p.z)
        cameraNode.look(at: SCNVector3(camTarget.x, camTarget.y, camTarget.z))
    }

    // MARK: muros (uno por nodo habilitado)

    func rebuildWalls() {
        wallParent.childNodes.forEach { $0.removeFromParentNode() }
        speakersByNode = [:]
        for node in nodesStore.enabledNodes {
            let wall = SCNNode()
            wall.position = SCNVector3(node.x, node.y, node.z)
            wall.eulerAngles.y = node.yawDeg * .pi / 180

            let panel = SCNBox(width: 0.72, height: 0.58, length: 0.03, chamferRadius: 0.008)
            panel.firstMaterial?.diffuse.contents = UIColor(red: 0.42, green: 0.33, blue: 0.23, alpha: 1)
            let panelNode = SCNNode(geometry: panel)
            panelNode.position = SCNVector3(0, 0, -0.02)
            wall.addChildNode(panelNode)

            var spk: [SCNNode] = []
            for p in SpeakerWall.positions {
                let g = SCNBox(width: 0.055, height: 0.12, length: 0.035, chamferRadius: 0.012)
                let m = SCNMaterial()
                m.diffuse.contents = UIColor(red: 0.20, green: 0.16, blue: 0.12, alpha: 1)
                m.emission.contents = UIColor.black
                g.firstMaterial = m
                let n = SCNNode(geometry: g)
                n.position = SCNVector3(p.x, p.y, p.z)
                wall.addChildNode(n)
                spk.append(n)
            }
            speakersByNode[node.id] = spk
            wallParent.addChildNode(wall)
        }
        applyFocus()
    }

    /// Foco → esfera + emisión de cada parlante de cada muro (marco local).
    private func applyFocus() {
        focusNode.position = SCNVector3(focus.x, focus.y, focus.z)
        var globalMax: Float = 0
        for node in nodesStore.enabledNodes {
            guard let spk = speakersByNode[node.id] else { continue }
            let local = node.toLocal(focus)
            let delays = SpeakerWall.delays(to: local)
            let dmax = delays.max() ?? 1
            globalMax = max(globalMax, dmax)
            for (i, n) in spk.enumerated() {
                let t = dmax > 0 ? CGFloat(delays[i] / dmax) : 0
                n.geometry?.firstMaterial?.emission.contents =
                    UIColor(red: 0.10 + 0.34 * t, green: 0.22 + 0.66 * t, blue: 0.20 + 0.61 * t,
                            alpha: 1).withAlphaComponent(0.15 + 0.85 * t)
            }
        }
        dtMaxMs = globalMax * 1000
    }

    // MARK: focos guardados

    func saveCurrentFocus() {
        let n = saved.count + 1
        saved.append(SavedFocus(name: "F\(n)", x: focus.x, y: focus.y, z: focus.z))
        if saved.count > 12 { saved.removeFirst() }
    }

    func recall(_ f: SavedFocus) {
        stopMotion()
        focus = f.point
        nodesStore.sendFocus(f.point, force: true)
    }

    // MARK: grabación de trayectoria

    func toggleRecording() {
        if isRecording {
            isRecording = false
            recStart = nil
        } else {
            stopMotion()
            traj = []
            recStart = Date()
            isRecording = true
        }
    }

    /// Llamado desde el drag mientras se graba (throttle del lado del caller).
    func recordSample() {
        guard isRecording, let start = recStart, traj.count < 900 else { return }
        let t = Float(Date().timeIntervalSince(start))
        traj.append(TrajPoint(t: t, x: focus.x, y: focus.y, z: focus.z))
    }

    func togglePlay() {
        isPlaying ? stopPlay() : startPlay()
    }

    private func startPlay() {
        guard traj.count > 1 else { return }
        stopMotion()
        isPlaying = true
        let pts = traj
        let dur = pts.last!.t
        playTask = Task { [weak self] in
            let t0 = Date()
            var i = 0
            while !Task.isCancelled {
                let t = Float(Date().timeIntervalSince(t0)).truncatingRemainder(dividingBy: max(dur, 0.1))
                if t < pts[max(i - 1, 0)].t { i = 0 }             // loop reiniciado
                while i < pts.count - 1 && pts[i + 1].t < t { i += 1 }
                let a = pts[max(i, 0)], b = pts[min(i + 1, pts.count - 1)]
                let span = max(b.t - a.t, 1e-4)
                let f = min(max((t - a.t) / span, 0), 1)
                let p = simd_mix(a.point, b.point, SIMD3(repeating: f))
                await MainActor.run { [weak self] in
                    guard let self else { return }
                    self.focus = p
                    self.playT = t
                    if self.streamToInstrument { self.nodesStore.sendFocus(p) }
                }
                try? await Task.sleep(nanoseconds: 33_000_000)
            }
        }
    }

    private func stopPlay() {
        playTask?.cancel(); playTask = nil
        isPlaying = false
        playT = 0
    }

    func clearTraj() {
        stopPlay()
        traj = []
    }

    private func rebuildTrail() {
        trailNode.childNodes.forEach { $0.removeFromParentNode() }
        guard traj.count > 1 else { return }
        let stride = max(traj.count / 120, 1)
        var pts: [SCNVector3] = []
        for i in Swift.stride(from: 0, to: traj.count, by: stride) {
            pts.append(SCNVector3(traj[i].x, traj[i].y, traj[i].z))
        }
        guard pts.count > 1 else { return }
        var idx: [Int32] = []
        for i in 0..<(pts.count - 1) { idx.append(Int32(i)); idx.append(Int32(i + 1)) }
        let src = SCNGeometrySource(vertices: pts)
        let el = SCNGeometryElement(indices: idx, primitiveType: .line)
        let g = SCNGeometry(sources: [src], elements: [el])
        g.firstMaterial?.diffuse.contents = UIColor(Theme.wave).withAlphaComponent(0.55)
        g.firstMaterial?.lightingModel = .constant
        trailNode.addChildNode(SCNNode(geometry: g))
    }

    // MARK: follow-me demo (lissajous autónoma)

    private func startFollow() {
        stopPlay()
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
                    if self.streamToInstrument { self.nodesStore.sendFocus(f) }
                }
                try? await Task.sleep(nanoseconds: 50_000_000)
            }
        }
    }

    private func stopFollow() {
        followTask?.cancel(); followTask = nil
    }

    func stopMotion() {
        if followMe { followMe = false }
        stopPlay()
    }
}

// MARK: - vista

struct SpatialView: View {
    @EnvironmentObject var app: AppState
    /// Del entorno, no creado acá: la lista de nodos es de la app entera.
    @EnvironmentObject var nodes: NodesStore
    @StateObject private var model: SpatialModel

    @State private var dragBase: SIMD3<Float>?
    @State private var camBase: (az: Float, el: Float)?
    @State private var zoomBase: Float?
    @State private var dragMode: DragMode = .focus
    @State private var showNodes = false
    @State private var lastRecSample = Date.distantPast

    enum DragMode: String, CaseIterable { case focus = "Foco", camera = "Cámara" }

    init() {
        // El modelo 3D necesita el store en su init, y un @EnvironmentObject no
        // está disponible todavía acá. Se le pasa uno con la MISMA persistencia
        // (mismo UserDefaults), así que ve la misma lista de nodos.
        _model = StateObject(wrappedValue: SpatialModel(nodesStore:
            NodesStore(defaultHost: UserDefaults.standard.string(forKey: "esp32_host")
                       ?? "192.168.0.50")))
    }

    var body: some View {
        VStack(spacing: 0) {
            HStack(alignment: .top) {
                ScreenHeader(subtitle: "sala · \(nodes.enabledNodes.count) nodo\(nodes.enabledNodes.count == 1 ? "" : "s") · \(nodes.lastSendSummary ?? "—")")
                Spacer()
            }
            .padding([.horizontal, .top], 16)
            .padding(.bottom, 8)

            HStack(spacing: 10) {
                Picker("", selection: $dragMode) {
                    ForEach(DragMode.allCases, id: \.self) { Text($0.rawValue).tag($0) }
                }
                .pickerStyle(.segmented)
                .frame(width: 170)

                Spacer()

                Button {
                    model.saveCurrentFocus()
                } label: {
                    Label("Guardar foco", systemImage: "plus.circle")
                        .font(Theme.mono(11))
                }
                .tint(Theme.wave)

                Button {
                    showNodes = true
                } label: {
                    Label("Nodos", systemImage: "square.grid.3x3")
                        .font(Theme.mono(11))
                }
                .tint(Theme.wood)
            }
            .padding(.horizontal, 16)
            .padding(.bottom, 8)

            ZStack(alignment: .bottom) {
                SceneView(scene: model.scene,
                          pointOfView: model.cameraNode,
                          options: [.rendersContinuously])
                    .gesture(dragGesture.simultaneously(with: zoomGesture))

                VStack(spacing: 8) {
                    if !model.saved.isEmpty { savedChips }
                    trajectoryBar
                    PhysReadout(text: String(format: "foco  x %+.2f  y %+.2f  z %.2f m   ·   Δt máx %.2f ms",
                                             model.focus.x, model.focus.y, model.focus.z, model.dtMaxMs))
                }
                .padding(12)
                .background(.ultraThinMaterial.opacity(0.88))
            }

            Text("FOCO: arrastrá para mover · CÁMARA: orbitá la sala · pinch = zoom. Cada parlante se ilumina según su delay focal real.")
                .font(Theme.mono(9.5))
                .foregroundStyle(Theme.muted)
                .frame(maxWidth: .infinity, alignment: .leading)
                .padding(14)
        }
        .background(Theme.ink)
        .sheet(isPresented: $showNodes) {
            NodesEditor(nodes: nodes, onChange: { model.rebuildWalls() })
        }
    }

    // MARK: subvistas

    private var savedChips: some View {
        ScrollView(.horizontal, showsIndicators: false) {
            HStack(spacing: 8) {
                ForEach(model.saved) { f in
                    HStack(spacing: 6) {
                        Button {
                            model.recall(f)
                        } label: {
                            Text("\(f.name)  \(String(format: "%.1f/%.1f/%.1f", f.x, f.y, f.z))")
                                .font(Theme.mono(10))
                        }
                        Button {
                            model.saved.removeAll { $0.id == f.id }
                        } label: {
                            Image(systemName: "xmark").font(.system(size: 8))
                        }
                    }
                    .padding(.horizontal, 9)
                    .padding(.vertical, 5)
                    .foregroundStyle(Theme.wave)
                    .background(Theme.ink)
                    .overlay(Capsule().stroke(Theme.wave.opacity(0.4), lineWidth: 1))
                    .clipShape(Capsule())
                }
            }
        }
    }

    private var trajectoryBar: some View {
        HStack(spacing: 10) {
            Button {
                model.toggleRecording()
            } label: {
                Label(model.isRecording ? "grabando…" : "grabar",
                      systemImage: model.isRecording ? "stop.circle.fill" : "record.circle")
                    .font(Theme.mono(11))
            }
            .tint(model.isRecording ? Theme.alert : Theme.wood)
            .buttonStyle(.bordered)

            Button {
                model.togglePlay()
            } label: {
                Label(model.isPlaying ? String(format: "%.1fs", model.playT) : "reproducir",
                      systemImage: model.isPlaying ? "pause.circle" : "play.circle")
                    .font(Theme.mono(11))
            }
            .tint(Theme.wave)
            .buttonStyle(.bordered)
            .disabled(model.traj.count < 2)

            if !model.traj.isEmpty {
                Button {
                    model.clearTraj()
                } label: {
                    Image(systemName: "trash").font(.system(size: 11))
                }
                .tint(Theme.muted)
            }

            Spacer()

            Toggle(isOn: $model.followMe) {
                Text("follow-me").font(Theme.mono(10))
            }
            .toggleStyle(.button)
            .tint(Theme.wave)

            Toggle(isOn: $model.streamToInstrument) {
                Text("→ nodos").font(Theme.mono(10))
            }
            .toggleStyle(.button)
            .tint(Theme.wood)
            .disabled(app.conn == .offline && nodes.enabledNodes.count <= 1)
        }
    }

    // MARK: gestos

    private var dragGesture: some Gesture {
        DragGesture(minimumDistance: 2)
            .onChanged { g in
                switch dragMode {
                case .focus:
                    if dragBase == nil { dragBase = model.focus; model.stopMotion() }
                    guard let base = dragBase else { return }
                    // arrastre en el plano de cámara: x lateral, y vertical = distancia
                    let ca = cos(model.camAz), sa = sin(model.camAz)
                    let dxs = Float(g.translation.width) / 160.0
                    let dzs = -Float(g.translation.height) / 130.0
                    let nx = base.x + dxs * ca + dzs * sa
                    let nz = base.z - dxs * sa + dzs * ca
                    model.focus = SIMD3(max(-3, min(3, nx)), base.y, max(-1.0, min(4.0, nz)))
                    if model.isRecording, Date().timeIntervalSince(lastRecSample) > 0.06 {
                        lastRecSample = Date()
                        model.recordSample()
                    }
                    nodes.sendFocus(model.focus)
                case .camera:
                    if camBase == nil { camBase = (model.camAz, model.camEl) }
                    guard let base = camBase else { return }
                    model.camAz = base.az - Float(g.translation.width) * 0.008
                    model.camEl = min(max(base.el + Float(g.translation.height) * 0.006, 0.03), 1.25)
                }
            }
            .onEnded { _ in
                if dragMode == .focus { nodes.sendFocus(model.focus, force: true) }
                dragBase = nil
                camBase = nil
            }
    }

    private var zoomGesture: some Gesture {
        MagnificationGesture()
            .onChanged { v in
                if zoomBase == nil { zoomBase = model.camR }
                if let base = zoomBase {
                    model.camR = min(max(base / Float(v), 1.4), 8.0)
                }
            }
            .onEnded { _ in zoomBase = nil }
    }
}

// MARK: - editor de nodos

struct NodesEditor: View {
    @ObservedObject var nodes: NodesStore
    let onChange: () -> Void
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        NavigationStack {
            List {
                ForEach($nodes.nodes) { $node in
                    Section {
                        HStack {
                            TextField("nombre", text: $node.name)
                                .font(Theme.mono(13))
                            Toggle("", isOn: $node.enabled).labelsHidden()
                        }
                        TextField("host (IP del ESP32)", text: $node.host)
                            .font(Theme.mono(12))
                            .keyboardType(.URL)
                            .autocorrectionDisabled()
                            .textInputAutocapitalization(.never)
                        PoseRow(label: "x (m)", value: $node.x, step: 0.1, range: -8...8)
                        PoseRow(label: "z (m)", value: $node.z, step: 0.1, range: -4...8)
                        PoseRow(label: "yaw (°)", value: $node.yawDeg, step: 15, range: -180...180)
                    }
                }
                .onDelete { idx in nodes.nodes.remove(atOffsets: idx) }

                Button {
                    nodes.add()
                } label: {
                    Label("Agregar nodo", systemImage: "plus")
                        .font(Theme.mono(12))
                }
                .tint(Theme.wave)
            }
            .navigationTitle("Nodos")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .confirmationAction) {
                    Button("Listo") {
                        onChange()
                        dismiss()
                    }
                }
            }
        }
        .preferredColorScheme(.dark)
        .onDisappear { onChange() }
    }
}

struct PoseRow: View {
    let label: String
    @Binding var value: Float
    let step: Float
    let range: ClosedRange<Float>

    var body: some View {
        HStack {
            Text(label).font(Theme.mono(11)).foregroundStyle(Theme.muted)
            Spacer()
            Text(String(format: "%+.1f", value))
                .font(Theme.mono(12, weight: .medium))
            Stepper("", onIncrement: {
                value = min(value + step, range.upperBound)
            }, onDecrement: {
                value = max(value - step, range.lowerBound)
            })
            .labelsHidden()
        }
    }
}

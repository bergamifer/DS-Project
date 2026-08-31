//
//  DomosonicaApp.swift
//  Domosonica — control remoto del instrumento (nodo DS10/DS11 vía ESP32).
//

import SwiftUI

@main
struct DomosonicaApp: App {
    @StateObject private var app = AppState()
    @StateObject private var nodes: NodesStore
    @StateObject private var fleet: FleetController

    /// `NodesStore` se creaba adentro de SpatialView, así que la lista de nodos
    /// vivía sólo en esa pestaña y el resto de la app le hablaba a un único
    /// `host` de Ajustes. Eran dos fuentes de verdad. Ahora se crea acá, una
    /// vez, y todas las pantallas ven la misma.
    init() {
        let ns = NodesStore(defaultHost:
            UserDefaults.standard.string(forKey: "esp32_host") ?? "192.168.0.50")
        _nodes = StateObject(wrappedValue: ns)
        _fleet = StateObject(wrappedValue: FleetController(nodes: ns))
    }

    var body: some Scene {
        WindowGroup {
            RootView()
                .environmentObject(app)
                .environmentObject(nodes)
                .environmentObject(fleet)
                .preferredColorScheme(.dark)   // el instrumento vive a oscuras
                .tint(Theme.wave)
        }
    }
}

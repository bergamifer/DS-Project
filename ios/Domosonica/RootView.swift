//
//  RootView.swift
//  Domosonica — tabs + ciclo de vida del polling.
//

import SwiftUI

struct RootView: View {
    @EnvironmentObject var app: AppState
    @EnvironmentObject var fleet: FleetController
    @Environment(\.scenePhase) private var scenePhase

    /// La app se abre donde la dejaste. Operando una instalación uno vuelve a
    /// la misma pantalla muchas veces seguidas, y arrancar siempre en Control
    /// obliga a un toque de más cada vez.
    @AppStorage("ui_tab") private var tab = 0

    var body: some View {
        TabView(selection: $tab) {
            ControlView()
                .tabItem { Label("Control", systemImage: "dial.medium") }.tag(0)
            FleetView()
                .tabItem { Label("Flota", systemImage: "square.grid.3x3.fill") }.tag(1)
            SpatialView()
                .tabItem { Label("Espacial", systemImage: "cube.transparent") }.tag(2)
            RIRView()
                .tabItem { Label("Sala", systemImage: "waveform.badge.mic") }.tag(3)
            SettingsView()
                .tabItem { Label("Ajustes", systemImage: "gearshape") }.tag(4)
        }
        .onAppear { app.startPolling(); fleet.startPolling() }
        .onChange(of: scenePhase) { _, phase in
            switch phase {
            case .active:
                app.startPolling(); fleet.startPolling()
            case .background, .inactive:
                app.stopPolling(); fleet.stopPolling(); fleet.stopDiscovery()
            @unknown default: break
            }
        }
    }
}

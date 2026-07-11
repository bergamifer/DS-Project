//
//  RootView.swift
//  Domosonica — tabs + ciclo de vida del polling.
//

import SwiftUI

struct RootView: View {
    @EnvironmentObject var app: AppState
    @Environment(\.scenePhase) private var scenePhase

    var body: some View {
        TabView {
            ControlView()
                .tabItem { Label("Control", systemImage: "dial.medium") }
            SpatialView()
                .tabItem { Label("Espacial", systemImage: "cube.transparent") }
            RIRView()
                .tabItem { Label("Sala", systemImage: "waveform.badge.mic") }
            SettingsView()
                .tabItem { Label("Ajustes", systemImage: "gearshape") }
        }
        .onAppear { app.startPolling() }
        .onChange(of: scenePhase) { _, phase in
            switch phase {
            case .active:   app.startPolling()
            case .background, .inactive: app.stopPolling()
            @unknown default: break
            }
        }
    }
}

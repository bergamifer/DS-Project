//
//  DomosonicaApp.swift
//  Domosonica — control remoto del instrumento (nodo DS10/DS11 vía ESP32).
//

import SwiftUI

@main
struct DomosonicaApp: App {
    @StateObject private var app = AppState()

    var body: some Scene {
        WindowGroup {
            RootView()
                .environmentObject(app)
                .preferredColorScheme(.dark)   // el instrumento vive a oscuras
                .tint(Theme.wave)
        }
    }
}

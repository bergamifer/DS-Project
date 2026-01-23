/**
 * DS Beamforming Control - Frontend JavaScript
 */

// Estado global
const state = {
    socket: null,
    currentDevice: null,
    deviceConfig: null,
    isMuted: false
};

// Elementos del DOM
const elements = {
    connectionStatus: document.getElementById('connection-status'),
    deviceSelector: document.getElementById('device-selector'),
    deviceStatus: document.getElementById('device-status'),
    ipConfig: document.getElementById('ip-config'),
    esp32Ip: document.getElementById('esp32-ip'),
    saveIpBtn: document.getElementById('save-ip-btn'),
    pingBtn: document.getElementById('ping-btn'),
    mainControls: document.getElementById('main-controls'),
    channelsSection: document.getElementById('channels-section'),
    channelsGrid: document.getElementById('channels-grid'),
    masterGain: document.getElementById('master-gain'),
    masterGainValue: document.getElementById('master-gain-value'),
    focalDistance: document.getElementById('focal-distance'),
    muteBtn: document.getElementById('mute-btn'),
    sendConfigBtn: document.getElementById('send-config-btn'),
    presetName: document.getElementById('preset-name'),
    savePresetBtn: document.getElementById('save-preset-btn'),
    presetsList: document.getElementById('presets-list'),
    log: document.getElementById('log')
};

// ============================================================================
// INICIALIZACIÓN
// ============================================================================

document.addEventListener('DOMContentLoaded', () => {
    initWebSocket();
    loadDevices();
    loadPresets();
    setupEventListeners();
});

function initWebSocket() {
    state.socket = io();

    state.socket.on('connect', () => {
        elements.connectionStatus.textContent = 'Conectado';
        elements.connectionStatus.className = 'status-online';
        log('Conectado al servidor', 'success');
    });

    state.socket.on('disconnect', () => {
        elements.connectionStatus.textContent = 'Desconectado';
        elements.connectionStatus.className = 'status-offline';
        log('Desconectado del servidor', 'error');
    });

    state.socket.on('device_updated', (data) => {
        log(`Dispositivo ${data.device_id} actualizado`);
        if (data.device_id === state.currentDevice) {
            loadDeviceConfig(data.device_id);
        }
    });

    state.socket.on('device_response', (data) => {
        log(`Respuesta de ${data.device_id}: ${data.message}`,
            data.status === 'ok' ? 'success' : 'error');
    });

    state.socket.on('device_status', (data) => {
        updateDeviceStatus(data);
    });
}

function setupEventListeners() {
    // Selector de dispositivo
    elements.deviceSelector.addEventListener('change', (e) => {
        if (e.target.value) {
            state.currentDevice = e.target.value;
            loadDeviceConfig(e.target.value);
            elements.ipConfig.style.display = 'block';
            elements.mainControls.style.display = 'block';
            elements.channelsSection.style.display = 'block';
        } else {
            elements.ipConfig.style.display = 'none';
            elements.mainControls.style.display = 'none';
            elements.channelsSection.style.display = 'none';
        }
    });

    // Guardar IP del ESP32
    elements.saveIpBtn.addEventListener('click', saveDeviceIp);

    // Ping al ESP32
    elements.pingBtn.addEventListener('click', pingDevice);

    // Master gain slider
    elements.masterGain.addEventListener('input', (e) => {
        elements.masterGainValue.textContent = parseFloat(e.target.value).toFixed(2);
    });

    // Botón mute
    elements.muteBtn.addEventListener('click', () => {
        state.isMuted = !state.isMuted;
        elements.muteBtn.classList.toggle('active', state.isMuted);
        elements.muteBtn.textContent = state.isMuted ? 'UNMUTE' : 'MUTE';
    });

    // Enviar configuración
    elements.sendConfigBtn.addEventListener('click', sendConfiguration);

    // Guardar preset
    elements.savePresetBtn.addEventListener('click', savePreset);
}

// ============================================================================
// API CALLS
// ============================================================================

async function loadDevices() {
    try {
        const response = await fetch('/api/devices');
        const devices = await response.json();

        elements.deviceSelector.innerHTML = '<option value="">Seleccionar dispositivo...</option>';
        devices.forEach(device => {
            const option = document.createElement('option');
            option.value = device.device_id;
            option.textContent = `${device.device_id} - ${device.name || 'Sin nombre'}`;
            elements.deviceSelector.appendChild(option);
        });

        log(`Cargados ${devices.length} dispositivos`);
    } catch (error) {
        log('Error cargando dispositivos: ' + error.message, 'error');
    }
}

async function loadDeviceConfig(deviceId) {
    try {
        const response = await fetch(`/api/devices/${deviceId}`);
        const config = await response.json();

        state.deviceConfig = config;

        // Actualizar IP del ESP32
        elements.esp32Ip.value = config.ip_address || '';

        // Actualizar controles principales
        elements.masterGain.value = config.master_gain;
        elements.masterGainValue.textContent = config.master_gain.toFixed(2);
        elements.focalDistance.value = config.focal_distance;
        state.isMuted = config.mute === 1;
        elements.muteBtn.classList.toggle('active', state.isMuted);
        elements.muteBtn.textContent = state.isMuted ? 'UNMUTE' : 'MUTE';

        // Renderizar canales
        renderChannels(config.channels);

        // Solicitar estado del dispositivo
        state.socket.emit('request_device_status', { device_id: deviceId });

        log(`Configuración cargada: ${deviceId}`);
    } catch (error) {
        log('Error cargando configuración: ' + error.message, 'error');
    }
}

async function saveDeviceIp() {
    if (!state.currentDevice) {
        log('Selecciona un dispositivo primero', 'error');
        return;
    }

    const ip = elements.esp32Ip.value.trim();
    if (!ip) {
        log('Ingresa una IP válida', 'error');
        return;
    }

    try {
        const response = await fetch(`/api/devices/${state.currentDevice}/ip`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ ip_address: ip })
        });

        if (response.ok) {
            log(`IP guardada: ${ip}`, 'success');
        } else {
            log('Error guardando IP', 'error');
        }
    } catch (error) {
        log('Error: ' + error.message, 'error');
    }
}

async function pingDevice() {
    if (!state.currentDevice) {
        log('Selecciona un dispositivo primero', 'error');
        return;
    }

    log(`Haciendo ping a ${state.currentDevice}...`);

    try {
        const response = await fetch(`/api/devices/${state.currentDevice}/ping`);
        const result = await response.json();

        updateDeviceStatus(result);
        log(result.message, result.online ? 'success' : 'error');
    } catch (error) {
        log('Error: ' + error.message, 'error');
    }
}

function renderChannels(channels) {
    elements.channelsGrid.innerHTML = '';

    channels.forEach(ch => {
        const delayMs = (ch.delay_samples / 48000 * 1000).toFixed(2);

        const row = document.createElement('div');
        row.className = 'channel-row';
        row.innerHTML = `
            <span class="channel-num">${ch.channel_num}</span>
            <input type="number"
                   class="channel-delay"
                   data-channel="${ch.channel_num}"
                   value="${delayMs}"
                   min="0"
                   max="341"
                   step="0.01">
            <input type="range"
                   class="channel-gain"
                   data-channel="${ch.channel_num}"
                   value="${ch.channel_gain}"
                   min="0"
                   max="1"
                   step="0.01">
            <input type="checkbox"
                   class="channel-mute"
                   data-channel="${ch.channel_num}"
                   ${ch.mute ? 'checked' : ''}>
        `;
        elements.channelsGrid.appendChild(row);
    });
}

async function sendConfiguration() {
    if (!state.currentDevice) {
        log('Selecciona un dispositivo primero', 'error');
        return;
    }

    // Recopilar configuración de canales
    const channels = [];
    for (let i = 0; i < 16; i++) {
        const delayInput = document.querySelector(`.channel-delay[data-channel="${i}"]`);
        const gainInput = document.querySelector(`.channel-gain[data-channel="${i}"]`);
        const muteInput = document.querySelector(`.channel-mute[data-channel="${i}"]`);

        const delayMs = parseFloat(delayInput.value);
        const delaySamples = Math.round(delayMs / 1000 * 48000);

        channels.push({
            channel_num: i,
            delay_samples: delaySamples,
            channel_gain: parseFloat(gainInput.value),
            mute: muteInput.checked ? 1 : 0,
            radiation_compensation: 1
        });
    }

    const config = {
        mute: state.isMuted ? 1 : 0,
        master_gain: parseFloat(elements.masterGain.value),
        focal_distance: parseFloat(elements.focalDistance.value),
        position_x: 0,
        position_y: 0,
        position_z: 0,
        channels: channels
    };

    try {
        // Guardar en base de datos
        const response = await fetch(`/api/devices/${state.currentDevice}`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(config)
        });

        if (response.ok) {
            log('Configuración guardada en base de datos', 'success');

            // Enviar al dispositivo via WebSocket
            state.socket.emit('send_to_device', {
                device_id: state.currentDevice,
                command: 'update_config',
                config: config
            });
        } else {
            log('Error guardando configuración', 'error');
        }
    } catch (error) {
        log('Error: ' + error.message, 'error');
    }
}

async function loadPresets() {
    try {
        const response = await fetch('/api/presets');
        const presets = await response.json();

        elements.presetsList.innerHTML = '';
        presets.forEach(preset => {
            const item = document.createElement('div');
            item.className = 'preset-item';
            item.innerHTML = `
                <span>${preset.name}</span>
                <span style="color: var(--text-secondary); font-size: 0.8rem;">
                    ${preset.description || ''}
                </span>
            `;
            item.addEventListener('click', () => loadPreset(preset.id));
            elements.presetsList.appendChild(item);
        });
    } catch (error) {
        log('Error cargando presets: ' + error.message, 'error');
    }
}

async function savePreset() {
    const name = elements.presetName.value.trim();
    if (!name) {
        log('Ingresa un nombre para el preset', 'error');
        return;
    }

    if (!state.deviceConfig) {
        log('Carga un dispositivo primero', 'error');
        return;
    }

    try {
        const response = await fetch('/api/presets', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                name: name,
                description: `Preset para ${state.currentDevice}`,
                config: state.deviceConfig
            })
        });

        if (response.ok) {
            log(`Preset "${name}" guardado`, 'success');
            elements.presetName.value = '';
            loadPresets();
        } else {
            const error = await response.json();
            log(error.error || 'Error guardando preset', 'error');
        }
    } catch (error) {
        log('Error: ' + error.message, 'error');
    }
}

function loadPreset(presetId) {
    log(`Cargar preset ${presetId} - TODO: implementar`);
}

// ============================================================================
// UTILIDADES
// ============================================================================

function updateDeviceStatus(data) {
    const ipInfo = data.ip_address ? ` (${data.ip_address})` : '';
    elements.deviceStatus.textContent = data.online
        ? `ESP32 conectado${ipInfo}`
        : (data.message || 'ESP32 no conectado');
    elements.deviceStatus.className = 'device-status ' + (data.online ? 'online' : 'offline');
}

function log(message, type = 'info') {
    const entry = document.createElement('div');
    entry.className = 'log-entry' + (type !== 'info' ? ` ${type}` : '');

    const time = new Date().toLocaleTimeString();
    entry.textContent = `[${time}] ${message}`;

    elements.log.insertBefore(entry, elements.log.firstChild);

    // Limitar a 50 entradas
    while (elements.log.children.length > 50) {
        elements.log.removeChild(elements.log.lastChild);
    }
}

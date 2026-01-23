"""
DS Beamforming Control Server
Servidor web para controlar hasta 8 sistemas DS via WiFi/ESP32
"""

from flask import Flask, render_template, request, jsonify
from flask_socketio import SocketIO, emit
from flask_cors import CORS
import json
import sqlite3
import os
import requests
from datetime import datetime

# Constantes de audio
SPEED_OF_SOUND = 343.0  # m/s @ 20°C
SAMPLE_RATE = 48000
NUM_CHANNELS = 16
MAX_DEVICES = 8

# Crear aplicación Flask
app = Flask(__name__)
app.config['SECRET_KEY'] = 'ds-beamforming-secret-key'
CORS(app)

# WebSocket para comunicación en tiempo real (modo threading, compatible con Python 3.13)
socketio = SocketIO(app, cors_allowed_origins="*", async_mode='threading')

# Base de datos SQLite (se crea automáticamente)
DB_PATH = os.path.join(os.path.dirname(__file__), 'ds_config.db')


# ============================================================================
# BASE DE DATOS
# ============================================================================

def init_db():
    """Inicializa la base de datos SQLite (se ejecuta una sola vez)"""
    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()

    # Tabla de dispositivos DS
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS devices (
            device_id TEXT PRIMARY KEY,
            name TEXT,
            ip_address TEXT,
            mute INTEGER DEFAULT 0,
            master_gain REAL DEFAULT 0.8,
            focal_distance REAL DEFAULT 3.0,
            position_x REAL DEFAULT 0.0,
            position_y REAL DEFAULT 0.0,
            position_z REAL DEFAULT 0.0,
            last_seen TEXT,
            is_online INTEGER DEFAULT 0
        )
    ''')

    # Tabla de canales (16 por dispositivo)
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS channels (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            device_id TEXT,
            channel_num INTEGER,
            delay_samples INTEGER DEFAULT 0,
            channel_gain REAL DEFAULT 1.0,
            mute INTEGER DEFAULT 0,
            radiation_compensation INTEGER DEFAULT 1,
            FOREIGN KEY (device_id) REFERENCES devices(device_id),
            UNIQUE(device_id, channel_num)
        )
    ''')

    # Tabla de presets guardados
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS presets (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT UNIQUE,
            description TEXT,
            config_json TEXT,
            created_at TEXT
        )
    ''')

    # Crear dispositivos por defecto (DS_01 a DS_08)
    for i in range(1, MAX_DEVICES + 1):
        device_id = f"DS_{i:02d}"
        cursor.execute('''
            INSERT OR IGNORE INTO devices (device_id, name)
            VALUES (?, ?)
        ''', (device_id, f"Sistema DS {i}"))

        # Crear 16 canales por dispositivo
        for ch in range(NUM_CHANNELS):
            cursor.execute('''
                INSERT OR IGNORE INTO channels (device_id, channel_num)
                VALUES (?, ?)
            ''', (device_id, ch))

    conn.commit()
    conn.close()
    print(f"Base de datos inicializada: {DB_PATH}")


def get_db():
    """Obtiene conexión a la base de datos"""
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row  # Para acceder por nombre de columna
    return conn


# ============================================================================
# CALCULOS DE BEAMFORMING
# ============================================================================

def distance_to_delay_samples(distance_m):
    """Convierte distancia (metros) a delay (muestras a 48kHz)"""
    delay_seconds = distance_m / SPEED_OF_SOUND
    return int(delay_seconds * SAMPLE_RATE)


def delay_samples_to_ms(samples):
    """Convierte muestras a milisegundos"""
    return (samples / SAMPLE_RATE) * 1000


def ms_to_delay_samples(ms):
    """Convierte milisegundos a muestras"""
    return int((ms / 1000) * SAMPLE_RATE)


def calculate_radiation_compensation(gains, focal_distance, channel_distances):
    """
    Aplica compensación de radiación (ley inversa del cuadrado)
    Normaliza para que el canal más lejano tenga ganancia máxima
    """
    if not channel_distances or focal_distance <= 0:
        return gains

    max_distance = max(channel_distances)
    compensated = []

    for i, gain in enumerate(gains):
        if channel_distances[i] > 0:
            # Normalizar respecto al más lejano para evitar valores > 1
            ratio = channel_distances[i] / max_distance
            compensated.append(min(gain * ratio, 1.0))
        else:
            compensated.append(gain)

    return compensated


# ============================================================================
# RUTAS HTTP (API REST)
# ============================================================================

@app.route('/')
def index():
    """Página principal"""
    return render_template('index.html')


@app.route('/api/devices', methods=['GET'])
def get_devices():
    """Obtiene lista de todos los dispositivos"""
    conn = get_db()
    devices = conn.execute('SELECT * FROM devices').fetchall()
    conn.close()
    return jsonify([dict(d) for d in devices])


@app.route('/api/devices/<device_id>', methods=['GET'])
def get_device(device_id):
    """Obtiene configuración de un dispositivo específico"""
    conn = get_db()
    device = conn.execute(
        'SELECT * FROM devices WHERE device_id = ?', (device_id,)
    ).fetchone()

    if not device:
        conn.close()
        return jsonify({'error': 'Dispositivo no encontrado'}), 404

    channels = conn.execute(
        'SELECT * FROM channels WHERE device_id = ? ORDER BY channel_num',
        (device_id,)
    ).fetchall()
    conn.close()

    result = dict(device)
    result['channels'] = [dict(ch) for ch in channels]
    return jsonify(result)


@app.route('/api/devices/<device_id>', methods=['POST'])
def update_device(device_id):
    """Actualiza configuración de un dispositivo"""
    data = request.get_json()
    conn = get_db()

    # Actualizar dispositivo
    conn.execute('''
        UPDATE devices SET
            mute = ?, master_gain = ?, focal_distance = ?,
            position_x = ?, position_y = ?, position_z = ?
        WHERE device_id = ?
    ''', (
        data.get('mute', 0),
        data.get('master_gain', 0.8),
        data.get('focal_distance', 3.0),
        data.get('position_x', 0.0),
        data.get('position_y', 0.0),
        data.get('position_z', 0.0),
        device_id
    ))

    # Actualizar canales si vienen en el request
    if 'channels' in data:
        for ch_data in data['channels']:
            conn.execute('''
                UPDATE channels SET
                    delay_samples = ?, channel_gain = ?,
                    mute = ?, radiation_compensation = ?
                WHERE device_id = ? AND channel_num = ?
            ''', (
                ch_data.get('delay_samples', 0),
                ch_data.get('channel_gain', 1.0),
                ch_data.get('mute', 0),
                ch_data.get('radiation_compensation', 1),
                device_id,
                ch_data['channel_num']
            ))

    conn.commit()
    conn.close()

    # Notificar a todos los clientes conectados
    socketio.emit('device_updated', {'device_id': device_id, 'config': data})

    return jsonify({'success': True, 'device_id': device_id})


@app.route('/api/presets', methods=['GET'])
def get_presets():
    """Obtiene lista de presets guardados"""
    conn = get_db()
    presets = conn.execute('SELECT id, name, description, created_at FROM presets').fetchall()
    conn.close()
    return jsonify([dict(p) for p in presets])


@app.route('/api/presets', methods=['POST'])
def save_preset():
    """Guarda un nuevo preset"""
    data = request.get_json()
    conn = get_db()

    try:
        conn.execute('''
            INSERT INTO presets (name, description, config_json, created_at)
            VALUES (?, ?, ?, ?)
        ''', (
            data['name'],
            data.get('description', ''),
            json.dumps(data['config']),
            datetime.now().isoformat()
        ))
        conn.commit()
        conn.close()
        return jsonify({'success': True})
    except sqlite3.IntegrityError:
        conn.close()
        return jsonify({'error': 'Ya existe un preset con ese nombre'}), 400


@app.route('/api/calculate_delays', methods=['POST'])
def calculate_delays():
    """Calcula delays basados en distancias de cada canal al punto focal"""
    data = request.get_json()
    distances = data.get('distances', [])

    delays = [distance_to_delay_samples(d) for d in distances]
    delays_ms = [delay_samples_to_ms(d) for d in delays]

    return jsonify({
        'delays_samples': delays,
        'delays_ms': delays_ms
    })


# ============================================================================
# COMUNICACIÓN CON ESP32
# ============================================================================

def send_to_esp32(ip_address, config):
    """Envía configuración al ESP32 via HTTP POST"""
    if not ip_address:
        return {'status': 'error', 'message': 'IP no configurada'}

    url = f"http://{ip_address}/command"
    try:
        response = requests.post(url, json=config, timeout=5)
        return response.json()
    except requests.exceptions.Timeout:
        return {'status': 'error', 'message': 'Timeout - ESP32 no responde'}
    except requests.exceptions.ConnectionError:
        return {'status': 'error', 'message': 'No se pudo conectar al ESP32'}
    except Exception as e:
        return {'status': 'error', 'message': str(e)}


def ping_esp32(ip_address):
    """Verifica si el ESP32 está online"""
    if not ip_address:
        return False

    url = f"http://{ip_address}/ping"
    try:
        response = requests.get(url, timeout=2)
        return response.status_code == 200
    except:
        return False


# ============================================================================
# RUTAS PARA CONFIGURAR IP DE DISPOSITIVOS
# ============================================================================

@app.route('/api/devices/<device_id>/ip', methods=['POST'])
def set_device_ip(device_id):
    """Configura la IP del ESP32 para un dispositivo"""
    data = request.get_json()
    ip_address = data.get('ip_address', '')

    conn = get_db()
    conn.execute('UPDATE devices SET ip_address = ? WHERE device_id = ?',
                 (ip_address, device_id))
    conn.commit()
    conn.close()

    return jsonify({'success': True, 'device_id': device_id, 'ip_address': ip_address})


@app.route('/api/devices/<device_id>/ping', methods=['GET'])
def ping_device(device_id):
    """Verifica si el ESP32 del dispositivo está online"""
    conn = get_db()
    device = conn.execute('SELECT ip_address FROM devices WHERE device_id = ?',
                          (device_id,)).fetchone()
    conn.close()

    if not device or not device['ip_address']:
        return jsonify({'online': False, 'message': 'IP no configurada'})

    is_online = ping_esp32(device['ip_address'])

    # Actualizar estado en DB
    conn = get_db()
    conn.execute('UPDATE devices SET is_online = ?, last_seen = ? WHERE device_id = ?',
                 (1 if is_online else 0, datetime.now().isoformat() if is_online else None, device_id))
    conn.commit()
    conn.close()

    return jsonify({
        'online': is_online,
        'ip_address': device['ip_address'],
        'message': 'ESP32 conectado' if is_online else 'ESP32 no responde'
    })


# ============================================================================
# WEBSOCKET (Comunicación en tiempo real)
# ============================================================================

@socketio.on('connect')
def handle_connect():
    """Cliente se conecta"""
    print(f"Cliente conectado: {request.sid}")
    emit('connection_status', {'status': 'connected'})


@socketio.on('disconnect')
def handle_disconnect():
    """Cliente se desconecta"""
    print(f"Cliente desconectado: {request.sid}")


@socketio.on('send_to_device')
def handle_send_to_device(data):
    """Envía comando a un dispositivo DS específico via ESP32"""
    device_id = data.get('device_id')
    config = data.get('config', {})

    print(f"Enviando a {device_id}...")

    # Obtener IP del dispositivo
    conn = get_db()
    device = conn.execute('SELECT ip_address FROM devices WHERE device_id = ?',
                          (device_id,)).fetchone()
    conn.close()

    if not device or not device['ip_address']:
        emit('device_response', {
            'device_id': device_id,
            'status': 'error',
            'message': f'IP no configurada para {device_id}. Configurala primero.'
        })
        return

    # Preparar payload para ESP32
    payload = {
        'command': 'update_config',
        'device_id': device_id,
        'mute': config.get('mute', 0),
        'master_gain': config.get('master_gain', 0.8),
        'focal_distance': config.get('focal_distance', 3.0),
        'channels': config.get('channels', [])
    }

    # Enviar al ESP32
    result = send_to_esp32(device['ip_address'], payload)

    emit('device_response', {
        'device_id': device_id,
        'status': result.get('status', 'error'),
        'message': result.get('message', 'Sin respuesta')
    })


@socketio.on('request_device_status')
def handle_status_request(data):
    """Solicita estado de un dispositivo"""
    device_id = data.get('device_id')

    conn = get_db()
    device = conn.execute('SELECT ip_address FROM devices WHERE device_id = ?',
                          (device_id,)).fetchone()
    conn.close()

    if not device or not device['ip_address']:
        emit('device_status', {
            'device_id': device_id,
            'online': False,
            'message': 'IP no configurada'
        })
        return

    is_online = ping_esp32(device['ip_address'])

    emit('device_status', {
        'device_id': device_id,
        'online': is_online,
        'ip_address': device['ip_address'],
        'message': 'ESP32 conectado' if is_online else 'ESP32 no responde'
    })


# ============================================================================
# INICIO DEL SERVIDOR
# ============================================================================

if __name__ == '__main__':
    # Inicializar base de datos
    init_db()

    print("\n" + "="*50)
    print("DS Beamforming Control Server")
    print("="*50)
    print(f"Abrí en tu navegador: http://localhost:5001")
    print(f"O desde otro dispositivo: http://<tu-ip>:5001")
    print("="*50 + "\n")

    # Ejecutar servidor con WebSocket
    # host='0.0.0.0' permite acceso desde otros dispositivos en la red
    # Puerto 5001 (el 5000 lo usa AirPlay en macOS)
    socketio.run(app, host='0.0.0.0', port=5001, debug=True)

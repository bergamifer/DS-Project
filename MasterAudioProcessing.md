## 🖥️ Control Central - Web Local Server

### Arquitectura General

Sistema de control centralizado para gestionar hasta **8 dispositivos DS independientes** desde una única interfaz web accesible desde computadora o dispositivo móvil en la red local.

**Componentes:**
1. **Backend (Flask en Mac/Linux)**: Cálculos de beamforming, gestión de dispositivos, comunicación UART/WiFi.
2. **Frontend (HTML/JavaScript)**: Interfaz web responsiva para controlar parámetros.
3. **Comunicación**: Cada DS tiene un ESP32 conectado vía UART al STM32; el servidor web contacta al ESP32 vía WiFi.
4. **Escalabilidad futura**: Migración opcional a Raspberry Pi 4 o ESP32-S3 para independencia de computadora.

### Instalación - Flask en macOS

#### Prerequisites:
- Python 3.8+ instalado
- Terminal acceso a Homebrew

#### Pasos:

1. **Instala Homebrew** (si no lo tienes):
   ```bash
   /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
   ```

2. **Instala Python 3**:
   ```bash
   brew install python@3.11
   ```

3. **Verifica instalación**:
   ```bash
   python3 --version
   ```

4. **Crea proyecto Flask**:
   ```bash
   mkdir ~/beamforming_control
   cd ~/beamforming_control
   python3 -m venv venv
   source venv/bin/activate
   ```

5. **Instala dependencias**:
   ```bash
   pip install flask flask-cors numpy
   ```

6. **Estructura de carpetas**:
   ```
   beamforming_control/
   ├── app.py                 # Backend Flask
   ├── templates/
   │   └── index.html         # Frontend web
   ├── static/
   │   ├── css/
   │   │   └── style.css
   │   └── js/
   │       └── beamforming.js
   └── venv/
   ```

#### Ejecución:
```bash
source venv/bin/activate
python3 app.py
# Accede a http://localhost:5000 desde navegador
```

### Parámetros de Control

#### Por Sistema DS (hasta 8 dispositivos):

| Parámetro | Tipo | Rango | Descripción |
|-----------|------|-------|-------------|
| **device_id** | string | "DS_01" - "DS_08" | Identificador único del sistema |
| **mute** | bool | true/false | Silencia salida de audio del sistema |
| **master_gain** | float | 0.0 - 1.0 | Ganancia global del sistema (-∞ a 0 dB) |
| **focal_distance** | float | 0.5 - 50.0 (m) | Distancia del punto focal (beam center) al parlante más cercano |
| **system_position** | [x, y, z] | coords (m) | Posición 3D del sistema en el espacio |

#### Por Canal (16 canales × 2 DACs):

| Parámetro | Tipo | Rango | Descripción |
|-----------|------|-------|-------------|
| **delay_samples** | int | 0 - 16384 | Retardo en muestras (48 kHz) |
| **delay_ms** | float | 0 - 341 | Retardo en milisegundos (auto-convierte a samples) |
| **channel_gain** | float | 0.0 - 1.0 | Ganancia individual del canal |
| **mute_channel** | bool | true/false | Silencia canal individual |
| **radiation_compensation** | bool | true/false | Aplica compensación por distancia radiante |

#### Radiación (Radiation Pattern):

Si **radiation_compensation = true**, el servidor calcula automáticamente la ganancia por canal basada en:
- Distancia del punto focal a cada parlante individual
- Patrón de radiación acústica (directividad)
- Curva de atenuación sonora (ley inversa del cuadrado)

**Fórmula básica**:
```
gain_channel = (focal_distance / distance_to_channel)^2 × master_gain
```

Esto produce un beam más **tenue cerca del foco** (distancias cortas) y **más potente lejos** (distancias largas), creando un perfil acústico natural.

### Ejemplo de Payload JSON

```json
{
  "command": "update_system",
  "device_id": "DS_01",
  "mute": false,
  "master_gain": 0.8,
  "focal_distance": 3.5,
  "system_position": [0.0, 0.0, 0.0],
  "channels": [
    {
      "channel": 0,
      "delay_ms": 0.0,
      "channel_gain": 1.0,
      "mute_channel": false,
      "radiation_compensation": true
    },
    {
      "channel": 1,
      "delay_ms": 5.2,
      "channel_gain": 0.95,
      "mute_channel": false,
      "radiation_compensation": true
    }
    // ... 14 canales más
  ]
}
```

### Interfaz Web - Características

**Panel Principal:**
- Selector de dispositivo DS (dropdown 1-8)
- Indicador de conexión WiFi/UART
- Control master gain (slider 0-1)
- Botón mute/unmute global
- Toggle de compensación radiante

**Configuración Avanzada:**
- Campo focal_distance (valor manual o cálculo automático desde posiciones 3D)
- Grid de 16 canales (uno por fila):
  - Delay (ms o samples)
  - Ganancia individual
  - Mute individual
  - Visualización de delay en tiempo real
- Botón calcular delays automáticamente desde distancias
- Botón enviar a dispositivo (HTTP POST al backend)

**Monitoreo:**
- Estado de conexión del ESP32
- Último comando enviado (timestamp)
- Log de errores UART

### Cálculos Backend (app.py)

```python
# Constantes
SPEED_OF_SOUND = 343  # m/s @ 20°C
SAMPLE_RATE = 48000
SPEED_OF_SOUND_SAMPLES = SPEED_OF_SOUND / SAMPLE_RATE  # m/sample

def calculate_delays_from_distances(distances_list):
    """Convierte distancias (m) a delays (muestras) para beamforming"""
    delays = []
    for dist in distances_list:
        delay_samples = int((dist / SPEED_OF_SOUND_SAMPLES))
        delays.append(delay_samples)
    return delays

def apply_radiation_compensation(gains, focal_distance, channel_distances):
    """Aplica compensación de radiación (ley inversa del cuadrado)"""
    compensated_gains = []
    for i, gain in enumerate(gains):
        if channel_distances[i] > 0:
            radiation_factor = (focal_distance / channel_distances[i]) ** 2
            compensated_gain = min(gain * radiation_factor, 1.0)  # Limita a 1.0
        else:
            compensated_gain = gain
        compensated_gains.append(compensated_gain)
    return compensated_gains
```

### Comunicación con ESP32

Cada DS contiene un **ESP32** con firmware que:
1. Recibe comandos HTTP/WebSocket desde el servidor Flask
2. Parsea parámetros (delays, ganancias, mute)
3. Envía vía UART al STM32H743 (protocolo binario o JSON)
4. Reporta estado de vuelta al servidor

Ver: [datasheets/uart_protocol.md](datasheets/uart_protocol.md)

### Roadmap de Migración

**Fase 1 (Actual):** Flask en Mac
- ✅ Desarrollo rápido
- ✅ Pruebas locales
- ❌ Requiere Mac encendida

**Fase 2 (Futuro):** Raspberry Pi 4
- ✅ Independencia de computadora
- ✅ Bajo costo (~$50)
- ✅ GPIO para control adicional
- ⚠️ Más complejo de instalar

**Fase 3 (Optativo):** ESP32-S3 en board de control
- ✅ Todo integrado en hardware
- ✅ Escalable a 8 sistemas sin computadora
- ❌ RAM/CPU limitados (solo para configs simples)

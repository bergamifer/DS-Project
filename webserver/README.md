## 🖥️ Control Central - Web Local Server

### Arquitectura General

Sistema de control centralizado para gestionar hasta **8 dispositivos DS independientes** desde una única interfaz web accesible desde computadora o dispositivo móvil en la red local.

**Componentes:**
1. **Backend (Flask en Mac/Linux)**: Cálculos de beamforming, gestión de dispositivos, comunicación UART/WiFi.
2. **Frontend (HTML/JavaScript)**: Interfaz web responsiva para controlar parámetros.
3. **Comunicación**: Cada DS tiene un ESP32 conectado vía UART al STM32; el servidor web contacta al ESP32 vía WiFi.
4. **Escalabilidad futura**: Migración opcional a Raspberry Pi 4 o ESP32-S3 para independencia de computadora.

### Instalación Rápida (macOS/Linux/Raspberry Pi)

#### Prerequisitos:
- Python 3.8+ (ya viene en macOS)
- Todas las dependencias son **gratuitas y open source**
- SQLite ya viene incluido en Python (no hay que instalar nada extra)

#### Verificar Python:
```bash
python3 --version
# Debería mostrar 3.8 o superior
```

> Si no tenés Python, en Mac: `brew install python@3.11`

#### Ejecución (primera vez):
```bash
cd webserver
./run.sh
```

El script automáticamente:
1. Crea un entorno virtual
2. Instala todas las dependencias
3. Inicia el servidor

#### Ejecuciones siguientes:
```bash
cd webserver
./run.sh
```

#### Acceso:
- Desde tu Mac: http://localhost:5001
- Desde celular/tablet en la misma red: http://[IP-de-tu-Mac]:5001

> **Nota:** El puerto 5000 está ocupado por AirPlay en macOS, por eso usamos 5001.

#### Detener el servidor:
Presioná `Ctrl+C` en la terminal donde está corriendo.

#### Estructura de carpetas:
```
webserver/
├── app.py                 # Backend Flask + WebSockets
├── run.sh                 # Script de ejecución
├── requirements.txt       # Dependencias
├── ds_config.db           # Base de datos SQLite (se crea automáticamente)
├── templates/
│   └── index.html         # Frontend web
└── static/
    ├── css/
    │   └── style.css
    └── js/
        └── main.js
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

### Configuración de Dispositivos ESP32

#### Paso 1: Configurar IP del ESP32
1. Seleccionar dispositivo (DS_01 - DS_08) en el dropdown
2. Ingresar la IP del ESP32 correspondiente (ej: `192.168.1.231`)
3. Click en **Guardar IP**
4. Click en **Ping** para verificar conexión

#### Paso 2: Enviar configuración
1. Ajustar parámetros (gain, delays, mute)
2. Click en **Enviar a Dispositivo**
3. Verificar en el log que llegó correctamente

### Comunicación con ESP32

Cada DS contiene un **ESP32** con firmware que:
1. Recibe comandos HTTP POST en `/command` desde el servidor Flask
2. Parsea parámetros JSON (delays, ganancias, mute)
3. Envía vía UART al STM32H743 (protocolo binario)
4. Responde con status OK/ERROR al servidor

#### Endpoints del ESP32

| Método | Endpoint | Descripción |
|--------|----------|-------------|
| POST | `/command` | Recibe configuración JSON completa |
| GET | `/status` | Retorna estado del dispositivo |
| GET | `/ping` | Health check simple |

#### Ejemplo de comando enviado al ESP32

```json
{
  "command": "update_config",
  "device_id": "DS_01",
  "mute": 0,
  "master_gain": 0.8,
  "focal_distance": 3.5,
  "channels": [
    {"channel_num": 0, "delay_samples": 1440, "channel_gain": 1.0, "mute": 0},
    {"channel_num": 1, "delay_samples": 1560, "channel_gain": 0.95, "mute": 0}
  ]
}
```

#### Respuesta esperada del ESP32

```json
{"status": "ok", "device_id": "DS_01", "message": "Config applied"}
```

Ver también: [Protocolo UART ESP32-STM32](#protocolo-uart-esp32-stm32) (próximamente)

### Proyecto ESP32 (firmware separado)

El firmware del ESP32 se mantiene en un **proyecto separado** para evitar conflictos con el toolchain de STM32.

**Razones para mantenerlo separado:**
- STM32CubeIDE usa ARM GCC toolchain
- ESP32 usa Xtensa toolchain (ESP-IDF) o Arduino
- Diferentes sistemas de build (Makefile vs CMake vs PlatformIO)
- Evita contaminación de includes y configuraciones

**Ubicación sugerida:**
```
~/STM32CubeIDE/
├── DS-Firmware/          # Este proyecto (STM32 + WebServer)
└── DS-ESP32-Firmware/    # Proyecto separado para ESP32
```

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
/Users/bergamifer/STM32CubeIDE/DS-Firmware/WebServer.md
# Estado Actual del Código y Funcionalidad del Sistema: Enlatadora IoT

Este documento presenta una descripción técnica exhaustiva de la arquitectura, el estado funcional y los módulos que componen el firmware del controlador **Enlatadora IoT**, implementado sobre un microcontrolador **ESP32-S3** con el framework Arduino/FreeRTOS.

---

## 1. Arquitectura General y Flujo Principal (`main.cpp`)

El archivo `main.cpp` constituye el punto de entrada de la aplicación. Su función principal es coordinar la inicialización secuencial del hardware, los sistemas de archivos y memoria, las interfaces de red, los servicios de telemetría y el controlador central de la máquina enlatadora.

```
                                  +-------------------+
                                  |     main.cpp      |
                                  +---------+---------+
                                            |
      +---------------------+---------------+---------------+---------------------+
      |                     |                               |                     |
      v                     v                               v                     v
[Core Hardware]    [Interfaces Serial]              [Red y Telemetría]     [Control de Máquina]
- NVS Flash        - Monitor (UART0)                - WifiManager          - MachineMaster (UART1)
- PSRAM & Heap     - Callbacks JSON / Comandos      - MqttManager          - Control de FSM
- SdCardManager                                     - WebSeverManager      - E/S Digitales
- EvenTheRGBLed                                     - VoiceKeywordModule
```

### 1.1 Secuencia de Inicialización en `setup()`

1. **Almacenamiento No Volátil (NVS):**
   - Se invoca `nvs_flash_init()`. En caso de truncamiento de páginas o cambio de versión, se formatea la memoria NVS y se reintenta la inicialización para garantizar persistencia de configuraciones (credenciales WiFi, preferencias de arranque).

2. **Diagnóstico y Monitor Serial (`Monitor::init()`):**
   - Configura la consola serial principal a 115200 baudios y realiza un volcado de diagnóstico de la memoria (PSRAM detectada, tamaño total, heap libre y bloques contiguos más grandes disponibles).

3. **Almacenamiento Secundario (`SdCardManager::init("/CanningMachine")`):**
   - Monta la tarjeta SD mediante bus SPI asignando el directorio base `/CanningMachine`.

4. **Indicador Visual de Estado (`EvenTheRGBLedHasManagers::init()`):**
   - Inicia la tarea FreeRTOS encargada del control no bloqueante del LED RGB direccionable (GPIO 48). Se establece inicialmente en color Blanco.

5. **Módulo de Comandos por Voz (`VoiceKeywordModule`):**
   - Asigna la clave API de Groq (`GROQ_API_KEY`) e inicializa el hardware de audio I2S (micrófono INMP441).

6. **Registro de Callbacks Seriales (`Monitor`):**
   - Se registran los manejadores de eventos recibidos por la consola serie:
     - `WifiManager::wifiConfigJsonCallback` (configuración de red por JSON).
     - `MachineMaster::outputsWriteJsonCallback` (forzado manual de salidas por JSON).
     - `MachineMaster::processCommands` (comandos de texto para la máquina).
     - `VoiceKeywordModule::processCommands` (activación/desactivación del módulo de voz).

7. **Conexión de Red (`WifiManager`):**
   - Define el hostname mDNS como `enlatadora-master`.
   - Inicializa el modo de red configurado (STA o AP). Si la conexión de red falla, se visualiza en el LED en color Rojo.

8. **Broker MQTT y Telemetría (`MqttManager`):**
   - Configura e inicia el servidor MQTT embebido en TCP (puerto 1883) y WebSockets (puerto 8080).
   - Suscribe tópicos de control:
     - `config/wifi/set`: Configuración remota de WiFi vía JSON.
     - `admin/reset/request`: Reinicio remoto del ESP32.
     - `voice/keyword/command`: Control de encendido/apagado del reconocimiento de voz.
     - `machine/IO/outputs/write`: Control manual de actuadores/salidas.
     - `machine/command`: Comandos operativos de la máquina (Start, Stop, Pause, Resume).
   - Registra publicaciones periódicas automáticas de telemetría:
     - `config/wifi/state` (cada 3000 ms).
     - `machine/state/read` (cada 1000 ms).
     - `machine/IO/outputs/read` (cada 200 ms).
     - `machine/IO/inputs/read` (cada 200 ms).
     - `voice/keyword/listening` (cada 200 ms).

9. **Servidor Web y Dashboard (`WebSeverManager`):**
   - Habilita el endpoint REST `/wifi/set` para configuración de red.
   - Habilita soporte para actualizaciones de firmware vía OTA (`/firmware`) con credenciales administrativas (`admin:admin`).
   - Sirve archivos estáticos del panel de control desde la tarjeta SD (`/CanningMachine/web`).
   - Dispara destello amarillo en el LED al solicitar la raíz web.

10. **Integración de Voz a Máquina (`VoiceKeywordModule`):**
    - Enlaza el vocabulario de comandos de `MachineMaster` para que al reconocer palabras clave de voz se ejecute la acción en la máquina y se notifique por MQTT en `voice/keyword/match`.
    - Publica transcripciones completas en `voice/keyword/transcription`.

11. **Arranque del Controlador de Máquina (`MachineMaster`):**
    - Inicia la comunicación UART con el PLC/controlador esclavo de hardware.
    - Asigna callbacks de transición de estado para señalización en el LED RGB:
      - Parada (`Stop`): Rojo (1000 ms).
      - Arranque (`Start`): Verde (1000 ms).
      - Pausa / Emergencia (`Pause`): Magenta (1000 ms).
    - Lanza la tarea de control `MachineMaster::begin()`.

12. **Bucle Principal (`loop()`):**
    - Mantiene una suspensión pasiva `vTaskDelay(pdMS_TO_TICKS(1000))` cediendo todo el tiempo de CPU a las tareas de FreeRTOS.

---

## 2. Controlador Central de la Máquina: `MachineMaster`

`MachineMaster` (`src/machine/MachineMaster.h` y `src/machine/MachineMaster.cpp`) es el componente de mayor prioridad del sistema. Actúa como el cerebro coordinador entre las interfaces remotas (MQTT, Web, Voz, Serial) y el hardware físico de enlatado (controlado mediante un bus serie dedicado UART1).

### 2.1 Especificaciones de Hardware y Pines
- **Bus UART con Controlador Esclavo:** `Serial1` en pines GPIO 18 (TX) y GPIO 17 (RX).
- **Entradas Digitales de Sensores (`NUM_INPUTS = 8`):** Canales `I0` a `I7`.
- **Salidas Digitales a Actuadores (`NUM_OUTPUTS = 19`):** Canales `Q0` a `Q18`.

### 2.2 Máquina de Estados Finita (FSM) y Etapas de Enlatado
El proceso de enlatado industrial se divide en 26 etapas secuenciales definidas en el enum `Stage`:

| Índice | Etapa (`Stage`) | Descripción Funcional |
|---|---|---|
| 0 | `Reposo` | Estado inicial de espera y reposo de actuadores. |
| 1 | `EsperaLata` | Espera la detección de una lata en la posición de entrada. |
| 2 | `TopeActivo` | Activación del tope mecánico para retener la lata. |
| 3 | `FrenadoYBajarTope` | Frenado de la lata y descenso del tope. |
| 4 | `SubirPlataformaTolva` | Elevación de la plataforma hacia la tolva dosificadora. |
| 5 | `Llenado` | Apertura de válvula y dosificación del producto en la lata. |
| 6 | `BajarPlataformaTolva` | Descenso de la plataforma tras completarse el llenado. |
| 7 | `TrasladoAPlataformaSalida` | Movimiento de la lata llena hacia la estación de salida/tapado. |
| 8 | `MantenerCadena2s` | Temporización de estabilización de la cadena de transporte (2 s). |
| 9 | `CadenaSalida10s` | Avance de la cadena de salida (10 s). |
| 10 | `ExtenderPiston` | Extensión del pistón dispensador de tapas. |
| 11 | `BajarVentosaYTapas` | Descenso del mecanismo de ventosas para tomar una tapa. |
| 12 | `RetraerPiston` | Retracción del pistón dispensador. |
| 13 | `SoltarTapa` | Liberación de la tapa sobre la lata posicionada. |
| 14 | `RetraerVentosaYBajarPlataforma` | Retracción de ventosas y descenso de plataforma. |
| 15 | `BandaTransportadora3` | Activación de la banda transportadora hacia la selladora. |
| 16 | `DesactivarBanda3` | Detención de la banda transportadora en posición de sellado. |
| 17 | `AsegurarPaletYSoltarTope` | Enclavamiento del palet y liberación del tope. |
| 18 | `LevantarLata` | Elevación de la lata hacia el cabezal de engargolado/sellado. |
| 19 | `VerificacionEstadoLata` | Sensorización de correcta altura y alineación. |
| 20 | `Sellar` | Accionamiento del mandril y rodillos de sellado de costura. |
| 21 | `ApagarMotorSellado` | Detención del motor del cabezal de sellado. |
| 22 | `BajarLata` | Descenso de la lata ya sellada. |
| 23 | `DesasegurarPalet` | Liberación del enclavamiento del palet. |
| 24 | `ActivarBanda3` | Evacuación de la lata sellada. |
| 25 | `ApagarBanda3YRepetir` | Finalización del ciclo y retorno a reposo/siguiente ciclo. |

### 2.3 Procesamiento de Estado del Controlador (`processMachineState`)
El controlador esclavo reporta su estado enviando paquetes JSON por UART. `MachineMaster` decodifica:
- `running` (bool): Indica si el ciclo automático está en ejecución.
- `emergencyStop` (bool): Bandera de parada de emergencia.
- `paused` (bool): Estado de pausa de la máquina.
- `authorizedSystem` (bool): Autorización de operación del sistema.
- `forcedAuthorization` (bool): Autorización manual forzada.
- `stage_name` (String) y `stage_num` (int): Etapa actual del proceso.
- `inputs` (Objeto JSON o String serializado): Mapea campos `I0` a `I7` hacia el arreglo booleano `inputState`.
- `outputs` (Objeto JSON o String serializado): Mapea campos `Q0` a `Q18` hacia el arreglo booleano `outputState`.

### 2.4 Control Manual Seguro de Salidas (`outputsWriteJsonCallback`)
Permite comandar actuadores de forma individual con estrictas validaciones de seguridad:
1. **Verificación de Parada de Emergencia:** Si `emergencyStop == true`, rechaza cualquier intento de escritura.
2. **Verificación de Override:** Si `allowOutputOverride == false`, bloquea la orden.
3. **Verificación de Enclavamiento Operativo:** Si la máquina está en ejecución y no está en pausa (`machineRunning && !machinePaused`), se impide la sobreescritura para evitar colisiones mecánicas.
4. **Transmisión Segura:** Si pasa las condiciones de seguridad, construye un JSON con las salidas solicitadas y lo envía al controlador UART.

### 2.5 Despacho de Comandos y Soporte Multilingüe
El método `processCommands` admite comandos directos de texto y comandos de control por voz en español e inglés:
- **Parada (`Stop`):** `stop`, `disable`, `detener`, `desactivar`.
- **Arranque (`Start`):** `start`, `enable`, `inicia`, `arranca`.
- **Pausa / Emergencia (`Pause`):** `pause`, `emergency`, `emergencia`, `pausa`.
- **Reanudación (`Resume`):** `resume`, `continue`, `continua`, `reanuda`.
- **Administración Especial:** `ForceAuthorization`, `AuthorizeOutputOverride`, `DisableOutputOverride`.

---

## 3. Módulos de Hardware y Controladores (Drivers)

### 3.1 `Monitor` y `UARTManager`
`UARTManager` (`src/drivers/universal/UARTManager.*`) implementa un canal de comunicación serie asíncrono y thread-safe con FreeRTOS:
- **Transmisión no bloqueante:** Mantiene una cola interna `std::queue<std::string>` protegida por un semáforo Mutex (`queueMutex`) con límite de 100 mensajes para prevenir saturación de RAM.
- **Tarea FreeRTOS:** Ejecuta `loop()` con período configurable (20 ms / 50 Hz) vaciando la cola hacia el puerto `HardwareSerial`.
- **Recepción y Demultiplexación:** Lee caracteres del puerto serie hasta encontrar `\r` o `\n`. Intenta deserializar el mensaje como JSON; si es válido, invoca a los `serialJsonCallbacks`, y si es texto plano, invoca a los `serialCallbacks`.
- **`Monitor`:** Es un envoltorio singleton sobre `UARTManager` que maneja el puerto serial principal `Serial` (UART0, USB-CDC o pines por defecto a 115200 baudios).

### 3.2 `SdCardManager`
`SdCardManager` (`src/drivers/specific/SdCardManager.*`) gestiona el almacenamiento en tarjeta SD utilizando la librería `SdFat`:
- **Pines SPI dedicados:**
  - `SD_CS`: GPIO 11
  - `SD_MOSI`: GPIO 12
  - `SD_SCK`: GPIO 13
  - `SD_MISO`: GPIO 14
- **Concurrencia:** Utiliza un Mutex recursivo (`xSemaphoreCreateRecursiveMutex`) y una clase RAII `LockGuard` para evitar condiciones de carrera entre tareas que leen archivos web y tareas de logging.
- **Resolución de Rutas:** Permite establecer una raíz fija (`/CanningMachine`) y normaliza las rutas relativas o absolutas.

### 3.3 `EvenTheRGBLedHasManagers`
`EvenTheRGBLedHasManagers` (`src/drivers/s3/EvenTheRGBLedHasManagers.*`) controla el LED RGB direccionable WS2812/Neopixel en GPIO 48:
- **Tarea Dedicada:** Corre en la tarea `RGB-Loop` (prioridad 3, actualización cada 50 ms) invocando `neopixelWrite`.
- **Mecanismo de Prioridad:**
  - **Color Fijo Temporal (`holdColor`):** Si se invoca `setColor(color, duracionMs)`, el color se mantiene prioritariamente durante el tiempo especificado.
  - **Generador Dinámico (`colorGenerator`):** Si no hay un color temporal activo, evalúa una función lambda registrada (p. ej., Azul si la voz escucha, Magenta si la máquina está en pausa, Verde si está corriendo).

### 3.4 `VoiceKeywordModule`
`VoiceKeywordModule` (`src/audio/VoiceKeywordModule.*`) implementa control por voz mediante inteligencia artificial en la nube:
- **Hardware I2S (Micrófono INMP441):**
  - `I2S_MIC_SD_PIN`: GPIO 41
  - `I2S_MIC_WS_PIN`: GPIO 40
  - `I2S_MIC_SCK_PIN`: GPIO 42
  - Parámetros: 16 kHz de frecuencia de muestreo, 32 bits de ranura (24 bits alineados a MSB con ganancia digital aplicada), mono canal izquierdo.
- **Detección de Hardware:** Durante `initI2S()`, valida la presencia del micrófono analizando el piso de ruido.
- **Arquitectura Multitarea en Núcleos:**
  - `VoiceCapture` (Core 1, Prioridad 5): Lee ráfagas DMA de 5 segundos (80,000 muestras), compensa el offset de DC, calcula el valor RMS y descarta fragmentos silenciosos (`RMS < 500`) para ahorrar ancho de banda y memoria.
  - `VoiceUpload` (Core 0, Prioridad 4): Recibe fragmentos a través de una cola FreeRTOS (`audioQueue`), encapsula el audio en formato WAV estándar (44 bytes de cabecera PCM) y realiza una petición HTTP POST multipart a la API de transcripción de Groq (`whisper-large-v3-turbo`).
- **Emparejamiento de Comandos:** Analiza el texto devuelto y ejecuta los callbacks registrados en `keywordSets`.

---

## 4. Módulos de Red y Conectividad

### 4.1 `WifiManager`
`WifiManager` (`src/drivers/universal/WifiManager.*`) administra la conectividad inalámbrica y la resolución de nombres mDNS:
- **Modos de Operación:**
  - **Modo Estación (STA):** Se conecta a una red existente utilizando credenciales almacenadas en NVS (`Preferences`) o definidas en compilación.
  - **Modo Punto de Acceso (AP):** Inicia un AP propio (`Enlatadora-s3` con IP `192.168.4.1`) como fallback si no hay conexión o si se fuerza por configuración.
- **mDNS:** Publica el servicio bajo `enlatadora-master.local` anunciando los servicios HTTP (puerto 80) y MQTT (puerto 1883).

### 4.2 `MqttManager`
`MqttManager` (`src/network/MqttManager.*`) convierte al ESP32 en un servidor/broker de mensajería:
- **Servidores Embebidos:**
  - `PicoMQTT::Server` sobre `WiFiServer` en puerto 1883 (TCP estándar para clientes MQTT).
  - `PicoWebsocket::Server` sobre `WiFiServer` en puerto 8080 (WebSockets para dashboards web en tiempo real).
- **Procesamiento Asíncrono en Core 0:**
  - Bucle `mqttManagerLoop` con stack de 24 KB y alimentación continua al Watchdog de tareas (`esp_task_wdt_reset`).
  - Cola de mensajes salientes `publishQueue` protegida por mutex.
  - Temporizador de publicaciones periódicas (`periodicPublishes`) para telemetría continua de estados y E/S.

### 4.3 `WebSeverManager`
`WebSeverManager` (`src/network/WebSeverManager.*`) implementa un servidor web asíncrono con `ESPAsyncWebServer` en el puerto 80:
- **Panel de Control:** Sirve archivos web estáticos almacenados en la tarjeta SD (`/CanningMachine/web`), gestionando cabeceras `Content-Type`, compresión `gzip` y caché.
- **Configuración REST:** Endpoint POST `/wifi/set` para actualizar credenciales de red vía JSON sin reiniciar inmediatamente.
- **Actualización de Firmware OTA:** Endpoint POST `/firmware` protegido por HTTP Basic Auth (`admin:admin`) para flashear nuevos binarios mediante `Update.h` desde scripts remotos (`updateMultipartOTA.sh` o `updateRawOTA.sh`).

---

## 5. Tabla Resumen de Tópicos MQTT y Comunicaciones

| Tópico MQTT | Tipo / Dirección | Frecuencia / Trigger | Descripción / Payload |
|---|---|---|---|
| `config/wifi/set` | Suscripción (In) | Bajo demanda | JSON con `{"ssid": "...", "pass": "...", "start_ap": bool}` |
| `config/wifi/state` | Publicación (Out) | Periódica (3000 ms) | JSON con `{"start_ap": bool, "sta_connected": bool}` |
| `admin/reset/request` | Suscripción (In) | Bajo demanda | `"1"` para reiniciar el ESP32 |
| `machine/command` | Suscripción (In) | Bajo demanda | Texto: `Start`, `Stop`, `Pause`, `Resume`, etc. |
| `machine/state/read` | Publicación (Out) | Periódica (1000 ms) | JSON con estado operativo, etapa actual y bandera de override |
| `machine/IO/outputs/write` | Suscripción (In) | Bajo demanda | JSON con valores manuales de salida `{"Q0": true, ...}` |
| `machine/IO/outputs/read` | Publicación (Out) | Periódica (200 ms) | JSON con estado de las 19 salidas `Q0` - `Q18` |
| `machine/IO/inputs/read` | Publicación (Out) | Periódica (200 ms) | JSON con estado de las 8 entradas `I0` - `I7` |
| `voice/keyword/command` | Suscripción (In) | Bajo demanda | `voiceOn` / `voiceOff` |
| `voice/keyword/listening` | Publicación (Out) | Periódica (200 ms) | `"1"` o `"0"` indicando estado del micrófono |
| `voice/keyword/transcription` | Publicación (Out) | Por evento | Texto completo transcrito por Groq |
| `voice/keyword/match` | Publicación (Out) | Por evento | Palabra clave detectada y ejecutada |

---

## 6. Estado Actual y Directivas de Compilación

El proyecto soporta banderas de compilación condicional (`src/config/build_flags.h`) para pruebas de laboratorio y desarrollo:
- `INPUT_LOGIC_TEST`: Permite simular o diagnosticar la lógica de entradas.
- `DISABLE_REAL_OUTPUT`: Inhibe la activación física de actuadores en fases de prueba.
- `DISABLE_AUDIO`: Desactiva completamente el módulo de audio I2S y las peticiones a Groq si no se dispone del micrófono conectado.
- `DISABLE_DASHBOARD_CACHE`: Invalida la caché web para facilitar el desarrollo frontend sobre la tarjeta SD.

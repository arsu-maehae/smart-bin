# Smart Bin Monitoring System

An ESP32-based IoT system for monitoring medical-waste bins in real time. The system measures bin fill level, temperature, and humidity; displays local status on a TFT screen; publishes telemetry over MQTT; identifies workers with RFID; shows a browser dashboard; and can send alerts through the LINE Messaging API.

## Features

- Measures bin fill level with an ultrasonic sensor
- Monitors temperature and humidity with an AHT20 sensor
- Reads atmospheric pressure from a BMP280 sensor
- Displays live status on a 240 × 320 ST7789 TFT screen
- Publishes sensor readings to HiveMQ over MQTT
- Raises a full-bin alert when the fill level exceeds 80%
- Uses RFID to confirm an authorized worker
- Sounds a buzzer for startup, warning, approval, and denial events
- Shows up to four bins on a real-time browser dashboard
- Sends LINE alerts for:
  - Full bins
  - High temperature
  - High humidity
  - Worker acknowledgement
- Includes a simulator for bins 2–4

## System Architecture

```text
Ultrasonic + AHT20 + BMP280 + RFID
                  │
                  ▼
                ESP32
          ┌───────┴────────┐
          │                │
          ▼                ▼
   TFT + Buzzer      HiveMQ MQTT Broker
                           │
              ┌────────────┴────────────┐
              ▼                         ▼
       Web Dashboard             Python Monitor
                                      │
                                      ▼
                             LINE Messaging API
```

The ESP32 currently represents **bin 1**. The simulator publishes sample telemetry for bins 2–4.

## Hardware

- ESP32 development board
- HC-SR04-compatible ultrasonic distance sensor
- ST7789 TFT display (240 × 320)
- MFRC522 RFID reader
- AHT20 temperature and humidity sensor
- BMP280 pressure sensor
- Buzzer
- RFID card or tag
- Breadboard, jumper wires, and suitable power supply

## Pin Configuration

The active pin assignments are defined in `src/main.cpp`.

| Component | Signal | ESP32 pin |
|---|---|---:|
| Ultrasonic sensor | TRIG | GPIO 32 |
| Ultrasonic sensor | ECHO | GPIO 33 |
| Buzzer | Signal | GPIO 14 |
| MFRC522 | SS/SDA | GPIO 5 |
| MFRC522 | RST | GPIO 27 |
| ST7789 TFT | CS | GPIO 15 |
| ST7789 TFT | DC | GPIO 2 |
| ST7789 TFT | RST | GPIO 4 |
| AHT20/BMP280 | SDA | GPIO 25 |
| AHT20/BMP280 | SCL | GPIO 26 |

The TFT and RFID modules use the ESP32 hardware SPI bus. Confirm your board's SPI pins and voltage requirements before wiring. The active firmware uses the Adafruit ST7789 library; `include/TFT_eSPI_Setup.h` belongs to an earlier display configuration.

> **Important:** ESP32 GPIO pins are 3.3 V devices. If the ultrasonic sensor's ECHO output is 5 V, use a voltage divider or level shifter.

## MQTT Configuration

The project currently uses the public HiveMQ broker:

| Setting | Value |
|---|---|
| Broker | `broker.hivemq.com` |
| ESP32/Python port | `1883` |
| Dashboard WebSocket port | `8884` |
| WebSocket path | `/mqtt` |
| Dashboard subscription | `smartbin/fleet/#` |
| LINE monitor subscription | `smartbin/fleet/1/#` |

### Topic Structure

```text
smartbin/fleet/<bin-id>/<data-type>
```

| Topic example | Payload example | Description |
|---|---|---|
| `smartbin/fleet/1/level` | `72` | Fill level in percent |
| `smartbin/fleet/1/temp` | `28.4` | Temperature in °C |
| `smartbin/fleet/1/humidity` | `61.2` | Relative humidity in percent |
| `smartbin/fleet/1/alert` | `FULL` | Full-bin alert |
| `smartbin/fleet/1/worker` | `Arsu Maehae` | Worker acknowledgement |

Because the broker and topic namespace are public, use this configuration for demonstrations only. For production, use a private authenticated broker, TLS, access controls, and a unique topic prefix.

## Project Structure

```text
smart-bin/
├── include/
│   └── TFT_eSPI_Setup.h      # Legacy TFT_eSPI configuration
├── scripts/
│   ├── index.html             # Real-time four-bin MQTT dashboard
│   ├── line_message.py        # MQTT monitor and LINE alert service
│   └── simulate_bins.py       # Sample data publisher for bins 2–4
├── src/
│   └── main.cpp               # ESP32 firmware
├── .gitignore
├── latest_data.json           # Output from the legacy backend
├── mqtt_backend.py            # Legacy single-bin prototype
└── platformio.ini             # PlatformIO board and library configuration
```

## Getting Started

### 1. Clone the repository

```bash
git clone https://github.com/arsu-maehae/smart-bin.git
cd smart-bin
```

### 2. Install the development tools

Install:

- [Visual Studio Code](https://code.visualstudio.com/)
- [PlatformIO IDE](https://platformio.org/install/ide?install=vscode)
- Python 3.9 or newer

PlatformIO installs the firmware libraries declared in `platformio.ini` automatically.

### 3. Configure Wi-Fi credentials

Create `src/secrets.h`:

```cpp
#pragma once

#define SECRET_WIFI_SSID "YOUR_WIFI_NAME"
#define SECRET_WIFI_PASS "YOUR_WIFI_PASSWORD"
```

This file is ignored by Git. Do not commit real credentials.

### 4. Configure the authorized RFID card

Open `src/main.cpp` and replace the example UID:

```cpp
String knownCard = "c6 fb 34 06";
```

Use the UID printed by your RFID-reading workflow and keep the same space-separated hexadecimal format.

### 5. Build and upload the firmware

Using the PlatformIO toolbar in VS Code:

1. Connect the ESP32 by USB.
2. Run **PlatformIO: Build**.
3. Run **PlatformIO: Upload**.
4. Open **PlatformIO: Serial Monitor** at `115200` baud.

Equivalent terminal commands:

```bash
pio run
pio run --target upload
pio device monitor --baud 115200
```

## Run the Web Dashboard

The dashboard connects directly to HiveMQ over secure WebSockets and listens for every topic under `smartbin/fleet/#`.

From the repository root:

```bash
python -m http.server 8000 --directory scripts
```

Then open:

```text
http://localhost:8000
```

The dashboard contains cards for bins 1–4 and shows fill level, temperature, humidity, full-bin status, and worker events.

## Run the LINE Alert Service

### 1. Install Python dependencies

```bash
python -m pip install paho-mqtt requests python-dotenv
```

### 2. Create a `.env` file

```env
LINE_TOKEN=YOUR_LINE_CHANNEL_ACCESS_TOKEN
LINE_USER_ID=YOUR_LINE_USER_ID
```

The `.env` file is ignored by Git. Never publish the token.

### 3. Start monitoring

```bash
python scripts/line_message.py
```

The service monitors bin 1 and uses these alert thresholds:

| Condition | Alert threshold | Reset threshold |
|---|---:|---:|
| Temperature | ≥ 35 °C | < 33 °C |
| Humidity | ≥ 80% | < 75% |
| Fill level | Firmware publishes `FULL` above 80% | RFID acknowledgement resets the full state |

The temperature and humidity reset thresholds provide hysteresis to reduce repeated notifications.

## Simulate Additional Bins

To publish sample values for bins 2–4:

```bash
python scripts/simulate_bins.py
```

Keep the dashboard open while the simulator runs. Press `Ctrl+C` to stop it.

## How the Device Works

1. The ESP32 connects to Wi-Fi and the MQTT broker.
2. Every three seconds, it reads distance, temperature, humidity, and pressure.
3. Distance is mapped from approximately 30 cm (empty) to 5 cm (full).
4. Fill level, temperature, and humidity are published for bin 1.
5. When the fill level exceeds 80%, the device:
   - Publishes a `FULL` alert
   - Shows a red warning screen
   - Sounds the buzzer
   - Waits for an RFID card
6. An authorized card publishes the worker's acknowledgement and resets the local full-bin state.
7. The dashboard updates from MQTT messages, while the Python monitor sends LINE notifications.

## Current Limitations

- The public MQTT broker has no project-specific authentication.
- Wi-Fi reconnect handling blocks until a connection is restored.
- The authorized RFID UID and worker name are currently hard-coded.
- The firmware reads BMP280 pressure but does not publish or display it.
- The LINE monitor subscribes only to bin 1.
- Alert state in the Python monitor is stored in memory and resets when the process restarts.
- `mqtt_backend.py` uses the older `arsu/smartbin/zoneA` JSON topic and is not part of the current fleet-based workflow.
- Automated tests and a Python dependency file have not yet been added.

## Possible Improvements

- Move device identity, thresholds, RFID users, and MQTT settings into configuration files
- Use MQTT authentication and TLS on all clients
- Add retained messages or persistent storage for the latest readings
- Publish pressure readings
- Expand LINE monitoring to every bin with per-bin anti-spam state
- Add offline-device detection and last-seen timestamps
- Add a database and historical charts
- Add automated firmware and Python tests
- Add over-the-air firmware updates

## Author

**Arsu Maehae**

GitHub: [@arsu-maehae](https://github.com/arsu-maehae)

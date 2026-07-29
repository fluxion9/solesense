# SoleSense — Smart Diabetic Foot Insole Monitor

> A wearable IoT insole system for real-time plantar pressure distribution analysis and diabetic foot risk detection, built on ESP32 with BLE connectivity and a Progressive Web App interface.

---

## Table of Contents

- [Overview](#overview)
- [Objectives](#objectives)
- [System Architecture](#system-architecture)
- [Hardware](#hardware)
  - [Components](#components)
  - [Wiring & Voltage Divider](#wiring--voltage-divider)
  - [Sensor Placement](#sensor-placement)
  - [Schematic](#schematic)
  - [PCB Design](#pcb-design)
  - [Final Product](#final-product)
- [Firmware](#firmware)
  - [BLE Protocol](#ble-protocol)
  - [FSR Calibration](#fsr-calibration)
  - [LED Status Codes](#led-status-codes)
  - [Battery SOC Estimation](#battery-soc-estimation)
  - [BLE Security](#ble-security)
- [Web App (PWA)](#web-app-pwa)
  - [Features](#features)
  - [Pressure Zones](#pressure-zones)
  - [Alert System](#alert-system)
  - [Installation](#installation)
  - [Browser Compatibility](#browser-compatibility)
- [File Structure](#file-structure)
- [Getting Started](#getting-started)
- [Calibration Guide](#calibration-guide)
- [Clinical Thresholds](#clinical-thresholds)
- [Roadmap](#roadmap)
- [License](#license)

---

## Overview

SoleSense is a smart insole monitoring system designed for patients with **diabetes mellitus** and clinicians monitoring diabetic foot syndrome. It uses five Force Sensitive Resistors (FSRs) and a DS18B20 temperature sensor embedded in an insole, connected to an ESP32 microcontroller that streams real-time data to a mobile-compatible Progressive Web App over Bluetooth Low Energy (BLE).

The system detects abnormal plantar pressure patterns and localized temperature elevation — two of the earliest indicators of diabetic foot complications including ulceration, neuropathy, and Charcot foot.

---

## Objectives

| # | Objective |
|---|-----------|
| 1 | Measure and analyze plantar pressure distribution across five anatomical foot zones |
| 2 | Monitor basic gait parameters including pressure symmetry and zone loading patterns |
| 3 | Enable real-time data collection and wireless transmission via BLE |
| 4 | Detect abnormal pressure patterns clinically linked to diabetic foot complications |

---

## System Architecture

```
┌─────────────────────────────────────────┐
│              INSOLE HARDWARE            │
│                                         │
│  FSR 1 (Heel)           ──┐             │
│  FSR 2 (Arch)           ──┤             │
│  FSR 3 (Metatarsal Lat) ──┤── ESP32 ───────── BLE ──── PWA / App │
│  FSR 4 (Metatarsal Med) ──┤             │
│  FSR 5 (Large Toe)      ──┤             │
│  DS18B20 (Temperature)  ──┘             │
│  Battery + Fuel Gauge   ──┘             │
│                                         │
└─────────────────────────────────────────┘
```

---

## Hardware

### Components

| Component | Specification | Quantity |
|-----------|--------------|----------|
| Microcontroller | ESP32 (DevKit or custom PCB) | 1 |
| FSR — Pressure | 10 kg Force Sensitive Resistor | 4 |
| FSR — Pressure | 20 kg Force Sensitive Resistor | 1 |
| Temperature Sensor | DS18B20 (waterproof variant) | 1 |
| Fixed Resistors | 100 kΩ (voltage divider, FSRs) | 5 |
| Pull-up Resistor | 4.7 kΩ (DS18B20 1-Wire bus) | 1 |
| Battery | LiPo single cell 3.7V | 1 |
| Battery Divider R1 | 100 kΩ | 1 |
| Battery Divider R2 | 100 kΩ | 1 |
| Status LED | Any colour, 3.3V compatible | 1 |
| LED Resistor | 220–470 Ω | 1 |

### Wiring & Voltage Divider

Each FSR is wired in a voltage divider configuration with the **fixed resistor on top** (VCC side) and the **FSR on the bottom** (GND side):

```
3.3V ──[100 kΩ]──┬── ADC Pin (ESP32)
                 │
              [FSR]
                 │
               GND
```

As pressure increases, FSR resistance decreases, causing ADC voltage to rise toward 3.3V. The firmware converts ADC readings to FSR resistance using:

```
R_fsr = R_fixed × (VCC − V_out) / V_out
```

Resistance is then mapped to grams via a piecewise linear calibration table.

### Sensor Placement

```
        ┌──────────────┐
        │   [FSR 5]    │  ← Large Toe (Hallux)
        │  [4]   [3]   │  ← Metatarsal Medial / Lateral
        │              │
        │   [FSR 2]    │  ← Arch (Medial Longitudinal)
        │              │
        │   [FSR 1]    │  ← Heel
        └──────────────┘
```

| FSR | Zone | Max Load | ADC Pin |
|-----|------|----------|---------|
| FSR 1 | Heel | 10 kg | GPIO 36 |
| FSR 2 | Arch | 10 kg | GPIO 39 |
| FSR 3 | Metatarsal (Lateral) | 10 kg | GPIO 34 |
| FSR 4 | Metatarsal (Medial) | 10 kg | GPIO 35 |
| FSR 5 | Large Toe | 20 kg | GPIO 32 |

> **Note:** GPIO 36 and 39 are input-only pins on ESP32 — ideal for ADC. All FSR pins use ADC1 to avoid conflicts with the BLE radio (ADC2 is unreliable when BLE is active).

---

### Schematic

> 📐 **Schematic diagram to be added here**
>
> _Place your schematic image below. Recommended formats: PNG, PDF, or SVG exported from KiCad, EasyEDA, or Altium._

<!-- Replace this comment with your schematic image -->
<!-- Example: ![Schematic](hardware/schematic/solesense_schematic_v1.png) -->

```
hardware/
└── schematic/
    └── solesense_schematic_v1.png   ← place your schematic here
```

---

### PCB Design

> 🖥️ **PCB layout to be added here**
>
> _Place your PCB layout image and/or Gerber files below. Include top layer, bottom layer, and 3D render if available._

<!-- Replace this comment with your PCB images -->
<!-- Example: -->
<!-- ![PCB Top](hardware/pcb/solesense_pcb_top.png) -->
<!-- ![PCB Bottom](hardware/pcb/solesense_pcb_bottom.png) -->

```
hardware/
└── pcb/
    ├── solesense_pcb_top.png        ← top copper layer
    ├── solesense_pcb_bottom.png     ← bottom copper layer
    ├── solesense_pcb_3d.png         ← 3D render (optional)
    └── gerbers/                     ← Gerber files for fabrication
        ├── solesense.gbr
        └── ...
```

| PCB Spec | Value |
|----------|-------|
| Board dimensions | TBD |
| Layers | 2 |
| Surface finish | HASL / ENIG |
| Min trace width | 0.2 mm |
| Connector | JST for battery, headers for sensors |

---

### Final Product

> 📸 **Product photos to be added here**
>
> _Place photos of the assembled insole, enclosure, and wearable device below._

<!-- Replace this comment with your product images -->
<!-- Example: -->
<!-- ![Assembled Device](images/product/solesense_assembled.jpg) -->
<!-- ![Insole Installed](images/product/solesense_in_shoe.jpg) -->
<!-- ![App Screenshot](images/product/solesense_app.jpg) -->

```
images/
└── product/
    ├── solesense_assembled.jpg      ← assembled PCB + sensors
    ├── solesense_in_shoe.jpg        ← insole installed in footwear
    ├── solesense_enclosure.jpg      ← enclosure / housing
    └── solesense_app.jpg            ← app screenshot on phone
```

---

## Firmware

**File:** `insole_7char.ino`
**Platform:** Arduino ESP32 Core
**Language:** C++

### BLE Protocol

The device advertises as **`InsoleMonitor`** and exposes one GATT service with 7 notify characteristics:

| Characteristic | UUID | Data | Format | Update Rate |
|----------------|------|------|--------|-------------|
| FSR 1 (Heel) | `AA01...` | Grams | ASCII float | 10 Hz |
| FSR 2 (Arch) | `AA02...` | Grams | ASCII float | 10 Hz |
| FSR 3 (Met. Lateral) | `AA03...` | Grams | ASCII float | 10 Hz |
| FSR 4 (Met. Medial) | `AA04...` | Grams | ASCII float | 10 Hz |
| FSR 5 (Large Toe) | `AA05...` | Grams | ASCII float | 10 Hz |
| Temperature | `AA06...` | °C | ASCII float | 0.5 Hz |
| Battery SOC | `AA07...` | % | ASCII float | 0.2 Hz |

**Service UUID:** `4FAFC201-1FB5-459E-8FCC-C5C9C331914B`

Each characteristic value is a small ASCII string (e.g. `"342"`, `"36.50"`) — small enough to fit within the default 20-byte BLE MTU with no fragmentation.

### FSR Calibration

The firmware maps FSR resistance to force (grams) using a piecewise linear lookup table. Default tables are included for typical 10 kg and 20 kg FSRs. **Replace the calibration table breakpoints with your own measured values** for best accuracy:

```cpp
const CalPoint CAL_FSR[] = {
  {1000000.0f,   0.0f},  // unloaded
  { 100000.0f,  10.0f},
  {  10000.0f, 200.0f},
  //  ... add your measured points ...
  {    200.0f,9000.0f}   // near max load
};
```

### LED Status Codes

| State | Pattern | Timing |
|-------|---------|--------|
| BLE advertising (not connected) | Slow single blink | 100ms ON / 900ms OFF |
| BLE connected | Double blink | 80ms / 120ms / 80ms / 1720ms |

### Battery SOC Estimation

Battery percentage is estimated from terminal voltage through a LiPo discharge curve lookup table. A resistor voltage divider (R1 = R2 = 100 kΩ) halves the battery voltage to keep the ADC input within 3.3V:

```
V_adc = V_battery × R2 / (R1 + R2)
```

For a more accurate SOC reading, replace the voltage divider with a dedicated fuel gauge IC (e.g. MAX17048, LC709203F).

### BLE Security

Optional BLE pairing is implemented using ESP32's native security stack. Two modes are supported:

- **Just Works** — silent auto-pair, encrypted link, no PIN (`ESP_IO_CAP_NONE`)
- **Fixed Passkey** — 6-digit PIN entry on first connection (`ESP_IO_CAP_OUT`)

> **Note:** BLE encryption is incompatible with Chrome's Web Bluetooth API on desktop Linux. For full encrypted operation, use the native Flutter app. The PWA works without encryption enabled.

---

## Web App (PWA)

**File:** `solesense.html`
**Type:** Progressive Web App (installable)
**Connectivity:** Web Bluetooth API

### Features

- Real-time anatomical foot heatmap (plantar view, right foot)
- Colour-coded pressure zones: blue (low) → green → amber → red (high)
- Live temperature gauge with clinical threshold indicators
- Raw sensor readings panel (all 5 FSRs + battery %)
- Overall status banner with plain-language clinical assessment
- Sustained pressure alert system with audible beep and push notification
- Installable as a standalone app on Android and iOS (via Bluefy)
- Fully offline-capable via service worker cache

### Pressure Zones

| Zone # | Anatomical Region | Warn Threshold | Danger Threshold |
|--------|-------------------|---------------|-----------------|
| 1 | Heel | 2000 g | 3500 g |
| 2 | Arch | 1200 g | 2200 g |
| 3 | Metatarsal (Lateral) | 1800 g | 3200 g |
| 4 | Metatarsal (Medial) | 1800 g | 3200 g |
| 5 | Large Toe (Hallux) | 1200 g | 2200 g |

### Alert System

The app monitors each zone for **sustained high pressure**. If any zone remains at danger level for more than **30 continuous seconds**, it triggers:

1. An audible triple-beep tone (Web Audio API, no external files)
2. An in-app slide-up toast notification
3. An OS-level push notification (if permission granted and app is backgrounded)

After an alert fires, the same zone is silenced for **60 seconds** to prevent spam. The alert clock resets immediately if pressure drops below the danger threshold.

### Installation

**Android (Chrome):**
1. Open `https://your-server/solesense.html` in Chrome
2. Tap the browser menu → "Add to Home screen"
3. The app installs and opens fullscreen like a native app

**iOS (Bluefy browser — required for BLE):**
1. Install **Bluefy** from the App Store
2. Open `https://your-server/solesense.html` in Bluefy
3. Tap Share → "Add to Home Screen"

**Desktop (Chrome/Edge):**
1. Open the URL in Chrome or Edge
2. Click the install icon (⊕) in the address bar

> The app must be served over **HTTPS or localhost** for both Web Bluetooth and PWA installation to work. Opening the `.html` file directly via `file://` will not work.

### Browser Compatibility

| Platform | Browser | BLE Support | PWA Install |
|----------|---------|------------|-------------|
| Android | Chrome | ✅ | ✅ |
| Android | Edge | ✅ | ✅ |
| iOS | Bluefy (App Store) | ✅ | ✅ |
| iOS | Safari | ❌ | ✅ |
| iOS | Chrome | ❌ | ✅ |
| Desktop Linux | Chrome (flag required) | ⚠️ | ✅ |
| Desktop Windows | Chrome / Edge | ✅ | ✅ |

> **iOS Note:** Apple forces all iOS browsers to use WebKit, which does not support Web Bluetooth. Bluefy is the only iOS browser that implements Web Bluetooth through a native BLE bridge.

---

## File Structure

```
solesense/
│
├── firmware/
│   ├── insole_7char.ino              # Main ESP32 firmware (7 BLE characteristics)
│   ├── insole_option1_multi_char.ino # Alternative: multi-char approach
│   └── insole_option2_mtu.ino       # Alternative: MTU + chunked JSON
│
├── webapp/
│   ├── solesense.html                # Main PWA app (single file)
│   ├── manifest.json                 # PWA manifest
│   ├── sw.js                         # Service worker (offline cache)
│   └── icons/
│       ├── icon-192.png              # PWA icon (192×192)
│       └── icon-512.png             # PWA icon (512×512)
│
├── hardware/
│   ├── schematic/                    # ← add schematic files here
│   ├── pcb/                          # ← add PCB layout + Gerbers here
│   └── bom/                          # ← bill of materials
│
├── images/
│   └── product/                      # ← add product photos here
│
└── README.md
```

---

## Getting Started

### 1. Flash the Firmware

Install required libraries in Arduino IDE:
- `OneWire` (by Paul Stoffregen)
- `DallasTemperature` (by Miles Burton)
- ESP32 board package via Board Manager

Open `insole_7char.ino`, select your ESP32 board, and flash.

### 2. Verify BLE

Open Arduino Serial Monitor at 115200 baud. You should see:
```
[BOOT] Insole Monitor — 7 Characteristics (5x FSR, Temp, BattSOC)
[BLE]  Advertising as 'InsoleMonitor' (7 characteristics)
[TEMP] DS18B20 initialised
```

### 3. Serve the Web App

```bash
# Option A — Python
cd webapp/
python3 -m http.server 8080
# Open http://localhost:8080/solesense.html in Chrome

# Option B — Node
npx serve webapp/
```

### 4. Connect

1. Open `solesense.html` in Chrome (Android) or Bluefy (iOS)
2. Tap **Connect**
3. Select **InsoleMonitor** from the device list
4. Allow notification permission when prompted
5. Live data begins streaming immediately

---

## Calibration Guide

The default FSR calibration tables are based on datasheet curves for typical Interlink FSR 402 sensors. For accurate gram readings:

1. Place a known weight (e.g. 500 g) on each FSR
2. Record the ADC raw value from Serial Monitor
3. Convert to resistance: `R = 100000 × (4095 - raw) / raw`
4. Add the `{resistance, grams}` pair to the calibration table in firmware
5. Repeat at 5–8 load points across the sensor's full range
6. Re-flash firmware

---

## Clinical Thresholds

| Metric | Normal | Elevated | High / At Risk |
|--------|--------|----------|---------------|
| Zone pressure (heel/forefoot) | < 2000 g | 2000–3500 g | > 3500 g |
| Zone pressure (arch/toe) | < 1200 g | 1200–2200 g | > 2200 g |
| Plantar temperature | 33–36.8 °C | 36.8–37.5 °C | > 37.5 °C |
| Sustained high pressure | < 30 s | 30–60 s | > 60 s → alert |

> ⚠️ These thresholds are indicative starting points. Clinical deployment should involve threshold validation with a qualified podiatrist or diabetologist.

---

## Roadmap

- [ ] Flutter mobile app (Android + iOS native BLE)
- [ ] Gait analysis: step count, cadence, stance/swing phase detection
- [ ] Multi-session history and trend logging
- [ ] TinyML on-device anomaly detection (pressure pattern classification)
- [ ] Cloud sync and clinician dashboard
- [ ] Custom PCB with LiPo charging, fuel gauge IC, and slim form factor
- [ ] Left/right foot support (dual device pairing)
- [ ] LoRaWAN long-range telemetry variant (STM32WL55 platform)

---

## License

This project is developed by an independent hardware startup based in Nigeria.
All rights reserved © 2025.

> For collaboration, licensing enquiries, or clinical partnership interest, please open an issue or reach out directly.
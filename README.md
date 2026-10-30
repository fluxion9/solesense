# SoleSense — Smart Diabetic Foot Insole Monitor

> A wearable IoT insole prototype that streams plantar force and foot temperature from an ESP32 over BLE to a Progressive Web App, with threshold-based risk indicators and sustained-pressure alerts.

---

## Table of Contents

- [Overview](#overview)
- [Project Status](#project-status)
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
  - [Sampling Loop](#sampling-loop)
  - [BLE Protocol](#ble-protocol)
  - [Serial Output](#serial-output)
  - [FSR Calibration](#fsr-calibration)
  - [LED Status Codes](#led-status-codes)
  - [Battery SOC Estimation](#battery-soc-estimation)
  - [BLE Security](#ble-security)
- [Web App (PWA)](#web-app-pwa)
  - [Features](#features)
  - [Pressure Zones](#pressure-zones)
  - [Temperature Bands](#temperature-bands)
  - [Alert System](#alert-system)
  - [Service Worker & Hosting](#service-worker--hosting)
  - [Installation](#installation)
  - [Browser Compatibility](#browser-compatibility)
- [File Structure](#file-structure)
- [Getting Started](#getting-started)
- [Calibration Guide](#calibration-guide)
- [Clinical Thresholds](#clinical-thresholds)
- [Known Limitations](#known-limitations)
- [Roadmap](#roadmap)
- [License](#license)

---

## Overview

SoleSense is a smart insole monitoring system intended for patients with **diabetes mellitus** and clinicians monitoring diabetic foot syndrome. Five Force Sensitive Resistors (FSRs) and a DS18B20 temperature sensor in one insole connect to an ESP32, which sends readings once per second to a Progressive Web App over Bluetooth Low Energy (BLE).

The app flags high load in any zone and elevated foot temperature, and alerts the wearer when a zone stays at danger level for 30 seconds. Data is shown live only: nothing is stored or sent anywhere other than the connected browser.

---

## Project Status

SoleSense is a prototype and is **not a validated medical device**.

| Area | Status |
|------|--------|
| Firmware | Single sketch. Does not build as committed on ESP32 Arduino core 3.3.12 — see [Getting Started](#getting-started) |
| Web app | Single-page PWA, no build step and no dependencies |
| Insoles | One foot only |
| Data history | None — live view only |
| Hardware design files | Not yet in this repository |
| Automated tests | None |

---

## Objectives

| # | Objective | Current state |
|---|-----------|---------------|
| 1 | Measure and analyze plantar pressure distribution across five anatomical foot zones | Implemented as force (grams) per zone |
| 2 | Monitor basic gait parameters including pressure symmetry and zone loading patterns | Not implemented — live per-zone load only |
| 3 | Enable real-time data collection and wireless transmission via BLE | Implemented at 1 Hz |
| 4 | Detect abnormal pressure patterns clinically linked to diabetic foot complications | Fixed per-zone thresholds; not clinically validated |

---

## System Architecture

```
┌─────────────────────────────────────────┐
│              INSOLE HARDWARE            │
│                                         │
│  FSR 1 (Heel)           ──┐             │
│  FSR 2 (Arch)           ──┤             │
│  FSR 3 (Metatarsal Lat) ──┤             │
│  FSR 4 (Metatarsal Med) ──┤── ESP32 ───────── BLE ──── PWA
│  FSR 5 (Large Toe)      ──┤             │
│  DS18B20 (Temperature)  ──┤             │
│  Battery voltage divider ─┘             │
│                                         │
└─────────────────────────────────────────┘
```

All analysis happens in the browser. The firmware converts sensor readings to grams, °C and battery %, and the app applies every threshold and alert.

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

Each FSR is wired in a voltage divider with the **fixed resistor on top** (VCC side) and the **FSR on the bottom** (GND side):

```
3.3V ──[100 kΩ]──┬── ADC Pin (ESP32)
                 │
              [FSR]
                 │
               GND
```

As pressure increases, FSR resistance decreases, so the ADC voltage **falls** toward 0 V. The firmware inverts each reading (`raw = 4095 - adc`) so that a higher `raw` value means more load, then calculates resistance from the inverted value:

```
R_fsr = R_fixed × (4095 − raw) / raw
```

Resistance is then mapped to grams through a piecewise linear calibration table. See [Known Limitations](#known-limitations) for how this divider interacts with the ESP32 ADC.

The ADC is set to 12-bit resolution with 11 dB attenuation. Each reading averages 8 samples taken 200 µs apart.

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

| Signal | Zone | Calibration table | GPIO |
|--------|------|-------------------|------|
| FSR 1 | Heel | 20 kg | 36 |
| FSR 2 | Arch | 10 kg | 39 |
| FSR 3 | Metatarsal (Lateral) | 10 kg | 34 |
| FSR 4 | Metatarsal (Medial) | 10 kg | 35 |
| FSR 5 | Large Toe | 10 kg | 32 |
| DS18B20 | Temperature (1-Wire) | — | 23 |
| Battery | Divider midpoint | — | 33 |
| Status LED | — | — | 19 |

> **Note:** The firmware applies the 20 kg table to the heel. The 20 kg FSR must be fitted at the heel for its readings to be correct.

> **Note:** GPIO 36 and 39 are input-only pins on ESP32. All analog pins use ADC1, because ADC2 is unreliable while the radio is active.

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

**File:** `firmware/insole_monitor_project/insole_monitor_project.ino`
**Platform:** Arduino ESP32 Core
**Language:** C++

### Sampling Loop

Every 1000 ms the firmware reads the temperature, all five FSRs and the battery, prints a JSON line to Serial, and sends BLE notifications if a client is connected. The LED runs on a non-blocking state machine between samples.

The DS18B20 runs at 12-bit resolution in non-blocking mode. Each cycle reads the result of the previous conversion and starts the next one. If the sensor can't be read, the firmware logs `[TEMP] Sensor disconnected or not found` and keeps sending the last good value, which is `0.0` if no reading has succeeded since boot.

### BLE Protocol

The device advertises as **`Insole Monitor v1.0`** and exposes one GATT service with 7 notify-only characteristics:

| Characteristic | UUID | Data | Format | Update Rate |
|----------------|------|------|--------|-------------|
| FSR 1 (Heel) | `AA010000-1FB5-459E-8FCC-C5C9C331914B` | Grams | ASCII float | 1 Hz |
| FSR 2 (Arch) | `AA020000-1FB5-459E-8FCC-C5C9C331914B` | Grams | ASCII float | 1 Hz |
| FSR 3 (Met. Lateral) | `AA030000-1FB5-459E-8FCC-C5C9C331914B` | Grams | ASCII float | 1 Hz |
| FSR 4 (Met. Medial) | `AA040000-1FB5-459E-8FCC-C5C9C331914B` | Grams | ASCII float | 1 Hz |
| FSR 5 (Large Toe) | `AA050000-1FB5-459E-8FCC-C5C9C331914B` | Grams | ASCII float | 1 Hz |
| Temperature | `AA060000-1FB5-459E-8FCC-C5C9C331914B` | °C | ASCII float | 1 Hz |
| Battery SOC | `AA070000-1FB5-459E-8FCC-C5C9C331914B` | % | ASCII float | 1 Hz |

**Service UUID:** `4FAFC201-1FB5-459E-8FCC-C5C9C331914B`

Each value is an ASCII string with one decimal place (for example `"342.5"` or `"36.5"`), at most 7 characters long.

### Serial Output

Serial runs at 115200 baud. At boot you should see:

```
[BOOT] Insole Pressure Monitor
[TEMP] DS18B20 initialised
[BLE] Advertising as 'Insole Monitor v1.0' (7 characteristics)
```

A JSON line follows every second, whether or not a client is connected:

```
Data: {"raw":[0,0,0,0,0],"grams":[0.00,0.00,0.00,0.00,0.00],"temp":[0.00],"vbat":0.00,"bsoc":0.0}		Size: 91
```

The values above show the format only. `raw` holds the inverted ADC readings used by the [Calibration Guide](#calibration-guide), and `vbat` is the battery voltage. This JSON is only sent to Serial, not over BLE.

### FSR Calibration

The firmware maps FSR resistance to force through piecewise linear lookup tables. It includes generic default tables, `CAL_10KG` and `CAL_20KG`. The 20 kg table is the 10 kg table with each gram value doubled:

```cpp
const CalPoint CAL_10KG[] = {
  {1000000.0f,   0.0f},
  { 100000.0f,  10.0f},
  {  30000.0f,  50.0f},
  {  10000.0f, 200.0f},
  {   5000.0f, 500.0f},
  {   2000.0f,1500.0f},
  {    500.0f,4500.0f},
  {    200.0f,9000.0f}
};
```

Entries must stay in descending resistance order. Resistances outside the table return the first or last gram value.

### LED Status Codes

| State | Pattern | Timing |
|-------|---------|--------|
| BLE advertising (not connected) | Single blink every 6 s | 100 ms ON / 5900 ms OFF |
| BLE connected | Double blink every 6 s | 80 ms ON / 120 ms OFF / 80 ms ON / 5720 ms OFF |

### Battery SOC Estimation

A resistor divider (R1 = R2 = 100 kΩ) halves the battery voltage to keep it within ADC range. The firmware calculates:

```
V_battery = adc × 6.6 / 4095
SOC %     = (V_battery − 3.0) / (4.2 − 3.0) × 100
```

This is a straight-line estimate and the result is not clamped, so it can read below 0% or above 100%. The sketch also contains a LiPo discharge curve (`BATT_CURVE`) and `voltageToSOC()`, but that call is commented out.

### BLE Security

BLE pairing and encryption are **enabled** in the firmware:

- Encryption required on the link, with Secure Connections and bonding
- Fixed 6-digit passkey: **`123456`**, set in `setup()` and also printed to Serial during pairing
- IO capability `ESP_IO_CAP_OUT`, so the phone or computer asks the user to enter the passkey

The app has no pairing code of its own; the operating system or browser shows the passkey prompt. Change the fixed passkey before any real-world use. For Just Works pairing, use the commented-out `ESP_IO_CAP_NONE` line in `setup()` instead.

> **Note:** BLE encryption has been reported as incompatible with Chrome's Web Bluetooth on desktop Linux. If pairing fails there, use another platform or disable the security settings in `setup()`.

---

## Web App (PWA)

**File:** `app/index.html`
**Type:** Progressive Web App (installable)
**Connectivity:** Web Bluetooth API

HTML, CSS and JavaScript are all in `index.html`.

### Features

- Live foot heatmap for a single foot
- Zone colours scale with load from 0 to 4500 g: blue → green → amber → red. Colour uses this one scale for every zone, while the Normal / Elevated / High tags use each zone's own thresholds
- Temperature gauge with status bands
- Sensor Readings panel showing all 5 FSRs in grams, plus battery %
- Overall status banner with a plain-language assessment
- Battery badge: green above 60%, amber above 20%, red otherwise
- Sustained pressure alerts with an audible beep, in-app toast and desktop-style notification
- Installable as a standalone app, portrait orientation

### Pressure Zones

| Zone # | Anatomical Region | Warn Threshold | Danger Threshold |
|--------|-------------------|---------------|-----------------|
| 1 | Heel | 2000 g | 3500 g |
| 2 | Arch | 1200 g | 2200 g |
| 3 | Metatarsal (Lateral) | 1800 g | 3200 g |
| 4 | Metatarsal (Medial) | 1800 g | 3200 g |
| 5 | Large Toe (Hallux) | 1200 g | 2200 g |

### Temperature Bands

| Temperature | Label | Effect on status banner |
|-------------|-------|------------------------|
| ≥ 37.5 °C | High temperature | Danger |
| 36.8–37.5 °C | Mildly elevated | Warning |
| 33–36.8 °C | Normal | None |
| < 33 °C | Cool — check sensor contact | None |

### Alert System

The app checks every zone each time a pressure value arrives. If a zone stays at danger level for **30 continuous seconds**, it triggers:

1. An in-app slide-up toast, which dismisses after 8 seconds
2. A browser notification, only if permission was granted and the page is hidden
3. An audible triple beep (Web Audio API, no external files)

If the zone stays in danger, the alert repeats every **60 seconds**. The 30-second timer resets as soon as that zone drops below its danger threshold. Disconnecting clears all alert timers.

Notification permission is requested when the device connects. Alerts depend on the page continuing to receive BLE data, so they stop if the device disconnects or the browser suspends the page.

### Service Worker & Hosting

`app/sw.js` caches `/index.html`, `/manifest.json` and both icons in the `solesense-v1` cache, then serves everything cache-first.

- **Deploying updates:** because cached files are served first, change `CACHE_NAME` in `sw.js` with each release, or returning users will keep the old `index.html`.
- **Hosting path:** the service worker expects the app at the domain root (`/sw.js`, `/index.html`), while `manifest.json` sets `start_url` to `/solesense/app`. These must be made to agree for your host; see [Known Limitations](#known-limitations).

### Installation

Host the contents of `app/`, then:

**Android (Chrome):**
1. Open the hosted app in Chrome
2. Tap the browser menu → "Add to Home screen"
3. The app installs and opens fullscreen like a native app

**iOS (Bluefy browser — required for BLE):**
1. Install **Bluefy** from the App Store
2. Open the hosted app in Bluefy
3. Tap Share → "Add to Home Screen"

**Desktop (Chrome/Edge):**
1. Open the URL in Chrome or Edge
2. Click the install icon (⊕) in the address bar

> The app must be served over **HTTPS or localhost** for both Web Bluetooth and PWA installation to work. Opening the `.html` file directly via `file://` will not work.

### Browser Compatibility

Expected support, based on each browser's Web Bluetooth availability:

| Platform | Browser | BLE Support | PWA Install |
|----------|---------|------------|-------------|
| Android | Chrome | ✅ | ✅ |
| Android | Edge | ✅ | ✅ |
| iOS | Bluefy (App Store) | ✅ | ✅ |
| iOS | Safari | ❌ | ✅ |
| iOS | Chrome | ❌ | ✅ |
| Desktop Linux | Chrome (flag required) | ⚠️ | ✅ |
| Desktop Windows | Chrome / Edge | ✅ | ✅ |

> **iOS Note:** Apple forces all iOS browsers to use WebKit, which does not support Web Bluetooth. Bluefy implements Web Bluetooth through a native BLE bridge.

---

## File Structure

```
solesense/
│
├── firmware/
│   └── insole_monitor_project/
│       ├── insole_monitor_project.ino   # ESP32 firmware
│       ├── debug.cfg                    # OpenOCD debugger config
│       ├── debug_custom.json            # Arduino IDE debugger config
│       └── esp32.svd                    # ESP32 register definitions for debugging
│
├── app/
│   ├── index.html                       # PWA (HTML, CSS and JS)
│   ├── manifest.json                    # PWA manifest
│   ├── sw.js                            # Service worker (app shell cache)
│   └── icons/
│       ├── icon-192.png                 # PWA icon (192×192)
│       └── icon-512.png                 # PWA icon (512×512)
│
└── README.md
```

---

## Getting Started

### 1. Flash the Firmware

Install in Arduino IDE or `arduino-cli`:
- ESP32 board package via Board Manager
- `OneWire` by Paul Stoffregen
- `DallasTemperature` by Miles Burton

Open `firmware/insole_monitor_project/insole_monitor_project.ino`, select your ESP32 board and flash.

> **Building on ESP32 core 3.3.12:** the sketch does not compile as committed. Two changes are needed:
>
> 1. Use `OneWire` **2.3.8**. Version 2.3.7 fails with `'GPIO' was not declared in this scope`.
> 2. Replace `BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT);` with `BLESecurity::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT);` and add `#include <BLESecurity.h>`.
>
> With both changes it compiles for `esp32:esp32:esp32` using `DallasTemperature` 3.9.0. The core version the sketch was originally built with isn't recorded.

### 2. Verify BLE

Open Serial Monitor at 115200 baud and check for the boot messages listed under [Serial Output](#serial-output). The LED should blink once every 6 seconds while advertising.

### 3. Serve the Web App

```bash
# Option A — Python
cd app/
python3 -m http.server 8080
# Open http://localhost:8080/ in Chrome

# Option B — Node
npx serve app/
```

### 4. Connect

1. Open the app in Chrome (Android) or Bluefy (iOS)
2. Tap **Connect**
3. Select **Insole Monitor v1.0** from the device list
4. Enter passkey `123456` if prompted
5. Allow notification permission when prompted
6. Readings begin arriving, updating once per second

---

## Calibration Guide

The default FSR tables are generic. For accurate gram readings:

1. Place a known weight (e.g. 500 g) on an FSR
2. Record that sensor's value from the `raw` array in Serial Monitor. These values are already inverted, so don't invert them again
3. Convert to resistance: `R = 100000 × (4095 - raw) / raw`
4. Add the `{resistance, grams}` pair to `CAL_10KG` or `CAL_20KG`, keeping resistance in descending order
5. Repeat at 5–8 load points across the sensor's full range
6. Re-flash firmware

Both tables are shared: `CAL_10KG` is used by all four 10 kg sensors. Calibrating each sensor separately would need a table per sensor.

---

## Clinical Thresholds

| Metric | Normal | Elevated | High / At Risk |
|--------|--------|----------|---------------|
| Heel load | < 2000 g | 2000–3500 g | ≥ 3500 g |
| Metatarsal load | < 1800 g | 1800–3200 g | ≥ 3200 g |
| Arch and large toe load | < 1200 g | 1200–2200 g | ≥ 2200 g |
| Plantar temperature | 33–36.8 °C | 36.8–37.5 °C | ≥ 37.5 °C |
| Time at danger level | < 30 s | — | ≥ 30 s → alert, repeating every 60 s |

> ⚠️ These thresholds are indicative starting points, measured in grams of force rather than pressure. Clinical deployment should involve threshold validation with a qualified podiatrist or diabetologist.

---

## Known Limitations

**Sensing and firmware**

- **ADC range:** with a 100 kΩ fixed resistor, every warn and danger load in the app calculates to about 37–112 mV at the ADC. That is below the roughly 150 mV where the ESP32 ADC becomes usable at 11 dB, so high loads may read as 0 V, which the firmware reports as the table maximum (9 kg or 18 kg). This is calculated and has not been measured on hardware.
- **Sample rate:** 1 Hz is too slow to capture individual steps.
- **Battery:** the straight-line estimate doesn't follow a LiPo discharge curve (3.7 V reads 58%), and the raw ADC value isn't corrected for ESP32 ADC non-linearity.
- **Security:** the BLE passkey is hardcoded and the same on every device.
- **Unused code:** `voltageToSOC()`, `BATT_CURVE`, `notifyString()` and `BLE_MTU_REQUEST` are not used.

**Web app**

- **Notifications:** the app uses `new Notification()`, which Chrome on Android does not support, so background notifications are not expected to appear there. The in-app toast and beep are unaffected.
- **Hosting paths:** no single hosting location currently satisfies both the manifest and the service worker. At the domain root, an installed app opens `/solesense/app`, which doesn't exist. Under `/solesense/app/`, the service worker fails to register and offline loading is lost.
- **Offline fallback:** `sw.js` falls back to `/solesense.html`, which no longer exists.
- **Unused code:** the Google Fonts handling in `sw.js` has no effect, because the app loads no web fonts.

**Monitoring logic**

- **Walking:** the 30-second timer restarts when a zone drops below danger, so alerts fire during sustained standing but not normally during walking.
- **Sensor faults:** the banner doesn't report a failed temperature sensor. It continues to use the last value, or shows "Normal" if no temperature has arrived. It also shows "Normal" when the insole isn't being worn.
- **Temperature:** the app uses one sensor and absolute thresholds. It can't compare matching sites on both feet.

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

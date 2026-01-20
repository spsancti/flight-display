# ESP32 ADS-B Flight Display (Round AMOLED)

This project uses an **ESP32-S3** class board to query the [adsb.lol](https://api.adsb.lol) API for nearby flights and render them on a **466×466 round AMOLED** display using **Arduino GFX** and the **gaggimate AmoledDisplay** drivers. The UI is redesigned for a circular layout with radial metrics and on-screen status indicators.

---

## Hardware Used

| Component | Description | Notes |
|-----------|-------------|-------|
| **ESP32-S3 Dev Board** | Main microcontroller | Tested with LilyGo T-Display S3 AMOLED pinout |
| **Round AMOLED Display** | 466×466 px AMOLED (CO5300 driver) | Uses QSPI + I2C touch controller |
| **USB Cable** | For programming and power | 5V supply via USB |

Notes
- The display driver files are vendored from `gaggimate` (`src/display/drivers/AmoledDisplay`).
- Requires **PSRAM enabled (OPI)**; see PlatformIO config.
- Pin mappings are defined in `src/display/drivers/AmoledDisplay/pin_config.h`.

---

## On-screen Indicators

The relay/LED outputs have been removed. Status is shown directly on the display:

- **Wi-Fi status dot** (upper-left)
  - Green: connected
  - Yellow: connecting
  - Red: offline
- **PVT/COM/MIL badge** (top arc)
- **Radial metrics** around the ring: Distance, Seats, Altitude

---

## Libraries

- **Arduino GFX** (CO5300 AMOLED)
- **ArduinoJson** for JSON parsing
- **WiFi**, **WiFiClientSecure**, **HTTPClient**, **WebServer** (ESP32 core)

The AmoledDisplay driver files are taken from the gaggimate repository:
- https://github.com/jniebuhr/gaggimate/tree/master/src/display/drivers/AmoledDisplay

---

## PlatformIO Build

This project is configured for PlatformIO, aligned with gaggimate’s display environment.

### Build

```bash
pio run -e display
```

### Upload

```bash
pio run -e display -t upload
```

### Monitor

```bash
pio device monitor -e display
```

---

## Arduino IDE Build

1. Copy `include/example-config.h` to `include/config.h` and set Wi-Fi credentials.
2. Ensure your board is an **ESP32-S3 with PSRAM** enabled.
3. Install libraries:
   - ArduinoJson
   - Arduino GFX
   - Adafruit GFX (for FreeSans fonts)
4. Compile and upload.

---

## Assumptions (and how to verify)

- **ASSUMPTION:** Display is a 466×466 round AMOLED using the CO5300 driver and matches the LilyGo T-Display S3 AMOLED pinout.  
  **RISK:** Panel won’t initialize or renders incorrectly if pinout differs.  
  **HOW TO VERIFY:** Confirm your board pinout vs `pin_config.h`, power the panel, and watch for the boot splash.

- **ASSUMPTION:** PSRAM is enabled in OPI mode.  
  **RISK:** Build will fail or driver will halt if PSRAM is disabled.  
  **HOW TO VERIFY:** Ensure `board_build.psram = enabled` and `board_build.arduino.memory_type = qio_opi` in `platformio.ini`.

---

## UI Layout (466×466 Round AMOLED)

- **Safe circular frame:** The UI computes a safe radius inside the 466×466 circle and draws a subtle accent ring along that edge.
- **Center title block:** The aircraft friendly name is centered with an optional second line and callsign subtitle.
- **Radial metrics:** Distance (bottom-left), seats (top), and altitude (bottom-right) are rendered around the perimeter for fast glanceability.
- **Status indicators:** Small dot at the top-left indicates Wi‑Fi (green/amber/red).
- **Op-class badge:** A pill label (PVT/COM/MIL) sits near the top to replace relay LEDs.

## API Details

This project uses the [adsb.lol](https://api.adsb.lol) API to retrieve live aircraft data.

### Closest Flight

`GET /v2/closest/{lat}/{lon}/{radius}`

- **lat**: Latitude in decimal degrees  
- **lon**: Longitude in decimal degrees  
- **radius**: Search radius in kilometers (**integer**)  

Example:
```
https://api.adsb.lol/v2/closest/47.6000/-122.3300/32
```

### Optional MIL classification

To detect military aircraft the device can query `/v2/mil` once per new hex and cache the result for several hours.
- Toggle at compile time with `#define FEATURE_MIL_LOOKUP 0/1` (default: 1).
- If disabled, MIL classification is inferred only from type/seat heuristics.

### Local Test Endpoint

The device exposes a simple test endpoint (when enabled) to push JSON and render without calling the API:

- `PUT http://<device-ip>/test/closest`
  - Body: either a minimal test schema `{ "t": "B738", "ident": "TEST123", "alt": 32000, "dist": 12.3 }` or the full `/v2/closest` schema.
- Toggle at compile time with `#define FEATURE_TEST_ENDPOINT 0/1` (default: 1).

Notes
- JSON parsing is filtered and streamed to minimize RAM (~8 KB doc).
- All network I/O has bounded timeouts; Wi-Fi reconnects automatically.

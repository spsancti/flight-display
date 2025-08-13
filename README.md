# ESP32 ADS-B Flight Display

This project uses an **ESP32** to query the [adsb.lol](https://api.adsb.lol) API for nearby flights and display the information on an OLED screen. The display shows flight details such as:

- **Flight number / Callsign**
- **Origin and destination airports**  
- **Aircraft type and seat capacity**  
- **Altitude and distance from your receiver**  
- **Operator classification:** PVT (private), COM (commercial), MIL (military)

---

## Hardware Used

| Component | Description | Notes |
|-----------|-------------|-------|
| **ESP32 Dev Board** | Main microcontroller | Any ESP32 dev module with Wi-Fi support |
| **0.96" OLED Display** | 128x64 pixels, SSD1306 driver | I²C interface (SDA, SCL) |
| **USB Cable** | For programming and power | 5V supply via USB |

---

## API Details

This project uses the [adsb.lol](https://api.adsb.lol) API to retrieve live aircraft data.  
Endpoints used:

### Nearby Flights

GET /v2/point/{lat}/{lon}/{radius}

- **lat**: Latitude in decimal degrees  
- **lon**: Longitude in decimal degrees  
- **radius**: Search radius in kilometers (**integer**)  

Example:
https://api.adsb.lol/v2/point/47.6000/-122.3300/32

Returns a list of aircraft objects with fields like:
```json
{
  "hex": "A388AF",
  "flight": "DAL327",
  "r": "N327DU",
  "t": "BCS3",
  "category": "A3",
  "alt_baro": 35000,
  "gs": 445,
  "lat": 47.621,
  "lon": -122.350
}

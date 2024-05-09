# ESP32 E-Paper PNG Display

![screenshot](https://github.com/inversion/esp32-e-paper-display/assets/1159424/79876f2e-f6b5-4ba1-9271-e7880755c7b4)
![e-paper](https://github.com/inversion/esp32-e-paper-display/assets/1159424/0f005030-0e8c-4f8d-bb89-03bdf600b52e)

The example shows a weather screen, but it could be used for any 400x300 web viewport rendered to a PNG.

# Components

| Folder | Description |
| - | - |
| `web` | NodeJS web server hosting the tiny 'weather screen' page shown above. |
| `screenshotter` | Sidecar NodeJS script that periodically uses puppeteer to take a screenshot of the webpage. This PNG is served from `/screenshot` on the `web` server. |
| `esp32` | ESP32 code to fetch the PNG over HTTPS, render it to the e-paper screen and sleep at intervals. |

# Hardware

- [TTGO ESP32 Microcontroller](https://www.banggood.com/TTGO-T-Display-ESP32-CP2104-CH340K-CH9102F-WiFi-bluetooth-Module-1_14-Inch-LCD-Development-Board-LILYGO-for-Arduino-products-that-work-with-official-Arduino-boards-p-1522925.html)
- Waveshare 4.2 inch e-Paper module: [Waveshare Docs](https://www.waveshare.com/wiki/4.2inch_e-Paper_Module), [Amazon link](https://www.amazon.com/4-2inch-Module-Communicating-Resolution-Controller/dp/B074NR1SW2)

# How to Use

1. The ESP32 code would need modification for differing hardware. Even if you use the same hardware, you *must check the EPD_ constants* which are the pins used for the e-paper module connections.
2. Copy and edit the following configuration:

| Template | Copy To | Description |
| - | - | - |
| `web/.env.local.template` | `web/.env.local` | [OpenWeatherMap](https://openweathermap.org/) API key and city. |
| `screenshotter/.env.local.template` | `screenshotter/.env.local` | Timezone setting for the screenshot 'browser'. |
| `esp32/src/credentials.h.template` | `esp32/src/credentials.h` | WiFi, timezone, screenshot endpoint URL. |

3. Use VSCode with the PlatformIO extension to upload the ESP32 code.
4. Build & run the `web` and `screenshotter` Docker containers: `docker compose up -d`
5. Forward requests from a webserver to the `web` container:
```
    # Example Apache config in a VirtualHost block
    <Location /esp32-e-paper-display>
            ProxyPass "http://localhost:3010"
            ProxyPassReverse "http://localhost:3010"
    </Location>
```
NB: If you change the base path, update the next.config.mjs to reflect this.

# References

- https://github.com/ZinggJM/GxEPD2
- https://github.com/G6EJD/ESP32-e-Paper-Weather-Display/
- https://github.com/kikuchan/pngle/

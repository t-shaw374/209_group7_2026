# Team 07 BLE Power Monitor

The ATmega computes everything. The ESP32 is a plain Bluetooth-to-UART pipe. The web page draws whatever it receives.

```
ATmega (computes Vrms, Irms, P, PF, picks the graph axis)
   │  UART 9600 baud, ASCII lines
   ▼
ESP32 (forwards bytes unchanged, no maths)      esp32_ble_bridge/
   │  BLE, Nordic UART Service
   ▼
Web page (draws the received values)           app/index.html
```

## Files

| File | Runs on | What it does |
|---|---|---|
| `mega/ble_report.c/.h` | ATmega | Formats the computed values into the line protocol, using integers only |
| `mega/ble_cmd.c/.h` | ATmega | Receives command lines from the app (RX interrupt), e.g. `RESETMM` |
| `mega/example_main.c` | ATmega | Shows how to call the modules and track min/max. Uses placeholder values until the real maths is written |
| `esp32_ble_bridge/esp32_ble_bridge.ino` | ESP32 | Transparent BLE ↔ UART bridge |
| `app/index.html` | Chrome / Edge | Live power graph, readings, raw log, CSV save, demo mode |

## Line protocol (ATmega → app)

```
A,0.00,8.00,W                  graph axis min, max, unit (chosen by the ATmega)
D,42,12.98,0.263,3.41,0.99     t, Vrms [V], Irms [A], P [W], PF
M,2.45,7.46                    min and max real power [W] (tracked on the ATmega)
```

App → ATmega commands (one per line):

```
RESETMM                        clear the min/max (sent by the app's "Reset min/max" button)
```

- Send `A,...` at start-up **and** every ~10 readings, so an app that connects late still gets the axis.
- The app shows the strings exactly as received. The only arithmetic it does is placing received values on screen pixels.
- Min/max ignore readings below `NO_LOAD_mW` (50 mW), so the minimum doesn't just read 0.00 W with nothing plugged in. They reset on `RESETMM` or an ATmega restart.
- `t` is the ATmega's own time stamp (for example seconds since power-up). If `t` goes backwards, the app assumes a restart and clears the graph.

## Wiring (ESP32 to P1)

| P1 pin | Signal | ESP32 |
|---|---|---|
| 1 | +5 V | **Do not connect.** See the power note below |
| 2 | GND | GND |
| 3 | ATmega RX (PD0) | GPIO 17 (`MEGA_TX_PIN`) |
| 4 | ATmega TX through R8/R10 divider (≈3.2 V) | GPIO 16 (`MEGA_RX_PIN`) |
| 5, 6 | not connected | – |

**Power:** run the ESP32 from its own USB cable. An ESP32 using BLE draws roughly 80–130 mA with higher peaks. The L78L05 is rated for 100 mA and is already close to its thermal limit with the op-amps and display. Share GND only.

The graph only needs pin 4 (ATmega → ESP32). Pin 3 is for sending commands later. If the Xplained Mini's USB serial link is also driving PD0, leave pin 3 unconnected until you need it.

Pin numbers apply to a classic ESP32 DevKit. For a C3 or S3, change `MEGA_RX_PIN` / `MEGA_TX_PIN` to any free GPIOs.

## Running the app

Web Bluetooth only works from a secure origin:

- **Laptop:** in the `app` folder run `python -m http.server 8000`, then open `http://localhost:8000` in Chrome or Edge.
- **Phone:** put `index.html` on GitHub Pages (HTTPS) and open it in Chrome on Android, or in Bluefy on iPhone.
- **Demo** feeds fake ATmega lines through the same parser, so you can check the page without hardware.

## ESP32 build

Arduino IDE → Boards Manager → "esp32" by Espressif → select your board → upload. It uses the BLE library bundled with the board package. Compiles cleanly for ESP32, ESP32-C3 and ESP32-S3 on core 2.0.9.

## Phase 2: flashing the ATmega over SPI (ISP), not built yet

To keep the ESP32 free of calculations, the **app** would parse the `.hex` file and build the 4-byte ISP instructions. The ESP32 would only shift those bytes out over SPI and toggle RESET, using a second BLE characteristic so flashing never mixes with the data stream.

| ATmega (Xplained Mini header) | ESP32 | Note |
|---|---|---|
| RST (U2 pin 6, not connected on the PCB) | any GPIO | Bodge wire. Held low during programming |
| PB3 / MOSI | VSPI MOSI (GPIO 23) | 3.3 V into a 5 V input is OK (V_IH min = 3.0 V) |
| PB5 / SCK | VSPI SCK (GPIO 18) | SCK ≤ F_CPU/4 = 500 kHz at 2 MHz. Use about 100 kHz |
| PB4 / MISO | VSPI MISO (GPIO 19) | **Needs a divider** (e.g. 1.2k/2.2k like R8/R10). The ESP32 is not 5 V tolerant |
| GND | GND | |

PB3–PB5 aren't used on the PCB, so they're free.

Before relying on it, check:

- **debugWIRE:** if Microchip Studio debugging left the DWEN fuse set, ISP is disabled. Use "Disable debugWIRE and Close" first.
- **Clock:** on the Xplained Mini the ATmega328PB is clocked from the on-board debugger chip by default. Make sure it still runs, and can be programmed, with that board's USB unplugged.
- **Bus contention:** the on-board debugger also connects to RESET and the SPI pins, so don't have its USB connected while the ESP32 is flashing.

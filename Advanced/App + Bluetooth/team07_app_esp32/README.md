# Team 07 power monitor: app and ESP32 files

Everything that runs on the **phone/laptop** and the **ESP32**. The ATmega code is not
in here (that is the team's `SMT_code` in the repo).

```
app/index.html                      the web app (open in Chrome or Edge)
esp32/esp32_ble_bridge/             the ESP32 firmware: Bluetooth <-> UART bridge + SPI programmer
esp32/diagnostics/                  test sketches, only for fault-finding
  esp32_uart_echo/                    UART round-trip test (with the ATmega echo test program)
  esp32_uart_sniffer/                 prints every byte arriving on GPIO16, at 9600 and 76800 baud
  esp32_isp_probe/                    checks the SPI programming wires (RESET/MOSI/MISO/SCK)
replay_test_data/                   files for the app's "Replay file" button
```

## ESP32 bridge

Arduino IDE, board package "esp32" by Espressif. Board **ESP32 Dev Module**, PSRAM
**Disabled**, 115200 for the Serial Monitor. Open `esp32_ble_bridge.ino` and upload.
It is one file (the programmer code is built in). If upload stops at `Connecting...`,
hold the **BOOT** button until `Writing at 0x...` appears.

Settings at the top of the sketch:
- `MEGA_BAUD 9600`: must match the ATmega's UART.
- `LOW_POWER 1`: slow, low-power Bluetooth link. Set 0 while measuring timing in the app.

Wiring to the SMT board, header P5 (the TXS0108E does the 3.3/5 V shifting):

| P5 | Net | ESP32 |
|---|---|---|
| 1, 10 | GND | GND |
| 2 | ESP_UART_RX (from ATmega TX, PD1) | GPIO16 |
| 3 | ESP_UART_TX (to ATmega RX, PD0) | GPIO17 |
| 4 | ESP_RST (ATmega RESET) | GPIO4 |
| 5 | ESP_MOSI (PB3) | GPIO23 |
| 6 | ESP_MISO (PB4) | GPIO19 |
| 7 | ESP_CLK (PB5) | GPIO18 |
| 8 | 3.3VdcESP (shifter's 3.3 V side) | 3V3 (ESP32's own output) |
| 9 | 5Vdc (powers the ESP32) | 5V |

Unplug the P5-9 wire while the ESP32's USB is plugged in. Never connect the ESP32 to
the board's 3.3 V regulator (that is the ADC reference).

## App

Open `app/index.html` in **Chrome or Edge** (laptop or Android). If it says there is no
Web Bluetooth, serve it: `python -m http.server 8000` in the `app` folder, then open
`http://localhost:8000`. Press **Connect** and choose **EM-Team07**.

- **Tiles and power graph** from each `Vrms ... mV, Imax ... mA, P ... mW` line.
- **Waveform** from the `--- mV mA ---` block (40 lines of `mV mA`) before it.
- **Timing**: update interval and send time (last / average / min / max), target < 1 s.
- **Send to ATmega**: text goes to the ATmega's UART (only if its code reads the UART).
- **Flash ATmega**: writes a `.hex` over Bluetooth (ESP32 programs the ATmega over SPI).
  **Back up chip** first: it saves what is on the ATmega as a `.hex` without changing it.
  Flash asks for confirmation because it erases the chip.
- **Replay file / Demo**: try the app without the board (files in `replay_test_data/`).

The app also still understands the `A,` / `D,` / `M,` lines of the alternative
team07-smt firmware.

## UART format the app expects from the ATmega (9600 baud, 8N1, lines end \r\n)

```
--- mV mA ---
<mV> <mA>                               x 40, one period, in time order
Vrms <mV> mV, Imax <mA> mA, P <mW> mW   ends the data set
No signal                               instead, when a capture fails
```

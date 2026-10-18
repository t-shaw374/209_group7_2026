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
| `firmware/` | ATmega | The complete energy-monitor firmware. See "ATmega firmware" below |
| `esp32_ble_bridge/esp32_ble_bridge.ino` | ESP32 | Transparent BLE ↔ UART bridge |
| `app/index.html` | Chrome / Edge | Live power graph, readings, raw log, CSV save, demo mode |

## Line protocol (ATmega → app)

```
A,0.00,8.00,W                  graph axis min, max, unit (chosen by the ATmega)
D,42.0,12.98,0.263,3.41,0.99,0.040   t [s], Vrms [V], Irms [A], P [W], PF, energy [Wh]
M,2.45,7.46                    min and max real power [W] (tracked on the ATmega)
```

App → ATmega commands are listed under "Commands" below. Replies come back as `OK,<cmd>`, `ERR,<cmd>,<reason>` or `S,...` lines in the app's log.

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

## ATmega firmware (`firmware/`)

Built from the lab code in both repos. The table shows where each part came from.

| File | Based on | Job |
|---|---|---|
| `main.c` | – | Report loop: measure, then energy, min/max and frequency, then display and BLE |
| `sampler.c/.h` | Lab 4 `adc.c` (same AVCC reference, 125 kHz ADC clock) and Lab 5 INT0 | Timer1-triggered ADC capture of Vmeas (PC0) and Imeas (PC1). Counts zero-crossings on PD2 |
| `measure.c/.h` | Lab 4 Part 5 power calculation | RMS, real power (with interpolation), apparent power, PF |
| `display.c/.h` | Lab 6 Part 2 `display.c` (tsha374) | 7-segment driver, plus unit letters, a value formatter and a 2.5 ms tick |
| `UART.c/.h` | Lab 4 `UART.c` (TAYE752), unchanged | 9600 baud transmit |
| `ble_report.c/.h`, `ble_cmd.c/.h` | – | Line protocol out to the app, command lines in |
| `commands.c`, `settings.c/.h`, `util.c/.h`, `monitor.h` | – | Command handler, EEPROM settings, parser helpers, shared state |
| `oled.c/.h`, `oled_graph.c/.h`, `font5x7.h` | – | 128 × 32 SSD1306 OLED on software I2C, and the scrolling power graph |
| `doom.c/.h`, `doom_core.c/.h`, `doom_tables.h` | – | DOOM mode (see below). `tools/make_doom_tables.py` regenerates the tables |
| `scratch.c/.h`, `pgm_compat.h` | – | 512-byte RAM buffer shared by the ADC capture and DOOM, and flash-table helpers |
| `common.h` | Lab 4 `common.h` | F_CPU 2 MHz, UBRR 12, calibration constants, sampling setup |

### How it measures

- **Sampling:** Timer1 starts every conversion, one every 200 µs, alternating V, I, V, I. Because the timing is set by hardware, the display and UART interrupts can't make it jitter. Each channel gets exactly 5 samples per 500 Hz cycle. One window is 100 pairs (40 ms, 20 cycles), and each report averages 4 windows.
- **Zero offset:** subtracted using the average of each window, so the ~2.04 V op-amp offset, and any drift in it, cancels out without calibration.
- **Real power:** V and I are sampled 200 µs apart, so the voltage at each current sample is linearly interpolated, $v' = (v_k + v_{k+1})/2$. That shrinks a 500 Hz sine by $\cos(2\pi \cdot 500 \cdot 200\,\mu s) = \cos 36^\circ$, and the code divides that back out exactly.
- **Scaling:** from the measured parts, $V_{meas}/V_{in} = 0.04321$ and $I_{meas}/I_{load} = 1.2986\ \text{V/A}$ (in `common.h`). The `CALV` and `CALI` commands trim these against a multimeter.
- **PC test:** with simulated ADC samples across the whole spec (12.6–15.4 V, 2.5–7.5 VA, PF 0.6–1.0), the algorithm error is under 0.1% for V, I and P. With the frequency off by ±1% it stays under 0.25%. The real accuracy is set by the analogue circuit and calibration.

### Display

The right-hand digit shows the unit: `U` volts, `A` amps, `P` watts, `E` watt-hours, `F` power factor. With 3 significant digits that looks like `13.0U`, `0.26A`, `3.41P`, `0.04E`. AUTO mode cycles V → I → P → E every 2 s. Before the first reading it shows `---`.

### Commands (type into the app's Send box; not case-sensitive)

| Command | What it does | Saved in EEPROM |
|---|---|---|
| `HELP` | Lists the commands | |
| `STATUS` | Firmware version, rate, axis, calibration factors, measured frequency, Offset (PC2) voltage, DC levels of Vmeas/Imeas, VA | |
| `RESET` | Energy total and time back to zero | |
| `RESETMM` | Clears min/max power (also the app's "Reset min/max" button) | |
| `RATE,500` | Report period in ms (250–10000) | yes |
| `AXIS,10` | App graph axis 0–10 W (1–100 W) | yes |
| `DISP,P` | Locks the display on `V`, `I`, `P`, `E` or `PF`. `DISP,AUTO` cycles them again | |
| `ZERO` | **AC source off:** records the noise floor and subtracts it from later readings | yes |
| `CALV,13.00` | **AC on:** type the true Vrms from your multimeter | yes |
| `CALI,0.264` | **Load on:** type the true Irms from your multimeter | yes |
| `CALCLR` | Back to design values (clears the CAL, ZERO, RATE and AXIS settings) | yes |
| `DOOM` | DOOM mode on the OLED. `EXIT` leaves it | |

### Calibration procedure

1. AC off. Wait for one reading, then send `ZERO`.
2. AC on with the load connected. Measure Vrms across the source with a multimeter and send `CALV,<value>`.
3. Measure the load current with the meter on AC amps in series (or Vshunt / 0.505 Ω) and send `CALI,<value>`.
4. Check with `STATUS`. Both factors should be close to 1.000. A factor more than 2× off is refused, because that means something is mis-wired.

Use a true-RMS meter that is rated for 500 Hz. Your meter read low at 500 Hz earlier, so check the voltage against the scope's RMS reading as well.

### Building in Microchip Studio

1. File → New → Project → GCC C Executable Project. Device: **ATmega328PB** (or 328P).
2. Add every `.c` and `.h` file from `firmware/`.
3. The project needs libm (`-lm`). It's on by default; check under Toolchain → Linker → Libraries if `sqrtf` won't link.
4. Build and program. The size is about 10.5 KB flash and 1.1 KB RAM. It builds with no warnings on avr-gcc.

Notes:
- Programming can wipe the EEPROM unless the EESAVE fuse is set. If it does, run the calibration again.
- The frequency reading counts rising edges on Vzc. If the comparator chatters (there's no hysteresis), `STATUS` will show more than 500 Hz. It's for information only and isn't used in the maths.

## OLED (128 × 32, SSD1306 over I2C)

| OLED pin | Connect to | Note |
|---|---|---|
| SDA | PB0 | Bodge wire. Unused on the PCB |
| SCL | PB1 | Bodge wire. Unused on the PCB |
| VCC | 5 V | The module has its own 3.3 V regulator |
| GND | GND | |

- The hardware I2C pins (PC4/PC5) are taken by the 7-segment shift register, so the OLED uses software I2C on PB0/PB1. PB3–PB5 stay free for SPI flashing.
- **The OLED module's own pull-up resistors are needed.** Nearly all modules have them. If yours doesn't, add 4.7 kΩ from SDA and from SCL to VCC.
- The firmware assumes the usual address, **0x3C**. Some modules use 0x3D: build with `-DOLED_ADDR=0x3D` or change it at the top of `oled.c`.
- At start-up the firmware checks whether a display answers. If none does, everything else keeps working and `DOOM` replies `ERR,DOOM,no OLED found`.
- **Normal mode:** the top line shows `3.41W 13.0V 0.264A`. Below it is the power graph, one column per reading (128 readings of history), scaled to the same axis as the app (`AXIS`). All of the graph's maths runs on the ATmega.

## DOOM mode

Send `DOOM` from the app. The app shows a control pad, and the keyboard works too: W/A/S/D or the arrow keys, Q/E to strafe, Space to fire. `EXIT`, or the Exit button, goes back to the energy monitor.

- **The game:** a first-person view drawn with a fixed-point raycaster: 64 rays, 2 px wide each, a 16 × 16 map, walls shaded by which way they face and how far away they are, and black edges at corners. Demons are drawn as scaled sprites and chase you around walls (breadth-first path search). There's a gun, crosshair, health bar along the top, a screen flash when you're hit, and a death screen.
- **Other displays:** the 7-segment display shows `dOOn` (7 segments can't draw an M), then your kill count. Kills and health are also sent to the app as `DOOM,kill,kills=3,hp=70`.
- **Turbo:** while playing, the CPU clock divider changes from 8 to 1 (16 MHz). The UART, Timer0 and the I2C timing are adjusted to match, and everything goes back to 2 MHz on `EXIT`. Measured in the simavr AVR simulator, a frame takes about 10 ms to draw plus 15 ms to send, so about **40 frames/s**. Set `DOOM_TURBO 0` in `doom.c` to stay at 2 MHz (about 8 frames/s).
- Measurement pauses while you play. The time spent playing is not counted as energy.
- **Turning it off:** set `ENABLE_DOOM 0` in `common.h` for the assessed build. That removes about 9 KB of game code, and `DOOM` then replies `ERR,DOOM,disabled`.
- **Input lag:** keys travel app → ESP32 → UART. With the ESP32's low-power Bluetooth settings, expect about 0.1–0.2 s of input lag.
- **Assumption:** turbo assumes the ATmega's clock source is 16 MHz with CKDIV8, which is what gives the labs' 2 MHz. If DOOM mode runs at the wrong speed, set `DOOM_TURBO 0`.

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

## Board: PCB_TEAM07_SMT

This firmware targets the SMT board: a bare ATmega328PB with a 16 MHz crystal, a TXS0108E level shifter to the ESP32, an LM2596 → 7 V → LD1117 5 V / 3.3 V supply, and a 3.3 V ADC reference on AREF. (The earlier through-hole/Xplained Mini version is in older copies of this project.)

### First power-up checklist

1. **Before fitting the ATmega or the ESP32**, power the board and check the rails: 7Vdc (TP5), 5Vdc (TP6), 3.3Vdc (TP7). Write the measured 3.3 V into `ADC_VREF` in `firmware/common.h`.
2. **Set the fuses once** over the ISP header P1 (Atmel-ICE or the lab programmer, Microchip Studio → Device Programming → Fuses): external crystal 8–16 MHz with CKDIV8 ticked → **low fuse 0x7F**, F_CPU = 2 MHz as in the labs. A new chip runs at 1 MHz from its internal oscillator until you do this, and the UART and timing will all be wrong.
3. Program the firmware over P1.

### ESP32 on header P5

| P5 pin | Net | ESP32 DevKit | |
|---|---|---|---|
| 1 | GND | GND | |
| 2 | ESP_UART_RX | GPIO 16 (`MEGA_RX_PIN`) | ← ATmega TX (PD1) through the level shifter |
| 3 | ESP_UART_TX | GPIO 17 (`MEGA_TX_PIN`) | → ATmega RX (PD0) |
| 4 | ESP_RST_GPIO | GPIO 4 (suggested) | ATmega reset, for SPI flashing later |
| 5 | ESP_MOSI | GPIO 23 | SPI flashing (later) |
| 6 | ESP_MISO | GPIO 19 | SPI flashing (later) |
| 7 | ESP_CLK | GPIO 18 | SPI flashing (later) |
| 8 | 3.3VdcESP | 3V3 | the ESP32's own 3.3 V **output** powers the shifter's A side and enables it (OE) |
| 9 | 5Vdc | VIN / 5V | **ESP32 power**, from the board's 5 V LD1117 (0.8 A) |
| 10 | GND | GND | |

- Don't power the ESP32 from the board's 3.3Vdc rail: that rail is the ADC reference (AREF) and the offset source, and the radio's current bursts would show up in every reading.
- No divider is needed: the TXS0108E translates every line between 3.3 V and 5 V.
- On a WROVER module GPIO 16/17 are used by its PSRAM; pick other pins and change the sketch.
- Most DevKits have a diode between USB 5 V and VIN, so it is fine to plug in USB to flash it while on P5. If yours doesn't, unplug the P5 pin 9 wire while flashing.

## Running the app

Web Bluetooth only works from a secure origin:

- **Laptop:** in the `app` folder run `python -m http.server 8000`, then open `http://localhost:8000` in Chrome or Edge.
- **Phone:** put `index.html` on GitHub Pages (HTTPS) and open it in Chrome on Android, or in Bluefy on iPhone.
- **Demo** feeds fake ATmega lines through the same parser, so you can check the page without hardware.

## ESP32 build

Arduino IDE → Boards Manager → "esp32" by Espressif → select your board → upload. It uses the BLE library bundled with the board package. Compiles cleanly for ESP32, ESP32-C3 and ESP32-S3 on core 2.0.9.

## Phase 2: flashing the ATmega over SPI (ISP), not built yet

The SMT board already has the wiring for this: P5 pins 4–7 carry the ATmega's RESET, MOSI, MISO and SCK through the TXS0108E, so no bodge wires or dividers are needed. To keep the ESP32 free of calculations, the **app** would parse the `.hex` file and build the 4-byte ISP instructions; the ESP32 would only shift those bytes out on SPI (VSPI: GPIO 23/19/18) and drive RESET (GPIO 4), on a second BLE characteristic so flashing never mixes with the data stream. SCK must stay below F_CPU/4 = 500 kHz at 2 MHz; about 100 kHz is safe.

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
- **Scaling:** computed in `common.h` from the resistor values in `LTspice_Simulation.asc` (divider 270k/15k, voltage difference amp, 0.5 Ω shunt, current gain 2.25, 1k/10n filters): $V_{vm}/V_{L} = 0.0695$ and $I_{vm}/I_{load} = 1.124\ \text{V/A}$. Change the `R_...` defines if the fitted parts differ; `CALV` and `CALI` trim the rest against a multimeter.
- **Reference:** the ADC uses the external 3.3 V AREF (REFS = 00), as wired on the SMT board.
- **PC test:** with simulated ADC samples across the whole spec (12.6–15.4 V, 2.5–7.5 VA, PF 0.6–1.0), the algorithm error is under 0.1% for V, I and P. With the frequency off by ±1% it stays under 0.25%. The real accuracy is set by the analogue circuit and calibration.

### Display

The right-hand digit shows the unit: `U` volts, `A` amps, `P` watts, `E` watt-hours, `F` power factor. With 3 significant digits that looks like `13.0U`, `0.26A`, `3.41P`, `0.04E`. AUTO mode cycles V → I → P → E every 2 s. Before the first reading it shows `---`.

### Commands (type into the app's Send box; not case-sensitive)

| Command | What it does | Saved in EEPROM |
|---|---|---|
| `HELP` | Lists the commands | |
| `STATUS` | Firmware version, rate, axis, calibration factors, measured frequency, offset voltage (ADC7), current peak (ADC3), DC levels of Vvm/Vim, VA | |
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

1. File → New → Project → GCC C Executable Project. Device: **ATmega328PB**.
2. Add every `.c` and `.h` file from `firmware/`.
3. The project needs libm (`-lm`). It's on by default; check under Toolchain → Linker → Libraries if `sqrtf` won't link.
4. Build and program. The size is about 10.5 KB flash and 1.1 KB RAM. It builds with no warnings on avr-gcc.

Notes:
- Programming can wipe the EEPROM unless the EESAVE fuse is set. If it does, run the calibration again.
- The frequency reading counts rising edges of the zero-crossing comparator on PC0 (VvmCZ, with hysteresis from R16/R17). It's for information only and isn't used in the maths.

## OLED (128 × 32, SSD1306 over I2C)

The OLED plugs into header **P4**: SDA = PC4, SCL = PC5 (the ATmega's hardware I2C), 5 V and GND. The pull-ups R14/R15 are on the board. The bus runs at 100 kHz normally and 400 kHz in DOOM turbo, and every wait has a timeout, so a missing or stuck display can't hang the meter.
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
- Turbo relies on the 16 MHz crystal with the CKDIV8 fuse (low fuse 0x7F). If the fuses aren't set, DOOM mode will run at the wrong speed.

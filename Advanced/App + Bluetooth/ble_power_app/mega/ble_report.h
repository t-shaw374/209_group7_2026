/*
 * ble_report.h  -  sends computed measurements to the BLE app (via the ESP32 bridge)
 *
 * Every value is computed on the ATmega; the ESP32 only forwards the bytes and the
 * app only draws what it receives. Lines are plain ASCII ending in '\n':
 *
 *   A,<min>,<max>,<unit>                 graph axis chosen by the ATmega
 *       e.g.  A,0.00,8.00,W
 *   D,<t>,<Vrms>,<Irms>,<P>,<PF>         one reading
 *       e.g.  D,42,12.98,0.263,3.41,0.99
 *       t    = time stamp kept by the ATmega (e.g. seconds since power-up)
 *       Vrms = volts (2 dp), Irms = amps (3 dp), P = watts (2 dp), PF (2 dp)
 *   M,<Pmin>,<Pmax>                      min / max real power so far, watts (2 dp)
 *       e.g.  M,2.45,7.46
 *       tracked on the ATmega; cleared when the app sends "RESETMM" (see ble_cmd.h)
 *
 * Send the axis line at start-up AND every so often (e.g. every 10 readings),
 * because the app may connect after the ATmega has already started.
 *
 * Uses usart_transmit() from UART.c (TX only is enough).
 */

#ifndef BLE_REPORT_H_
#define BLE_REPORT_H_

#include <stdint.h>

/* Axis for the power graph, in milliwatts (sent as watts with 2 dp). */
void ble_send_axis(uint32_t p_min_mW, uint32_t p_max_mW);

/* One reading. All inputs are integers in milli-units (no floats needed). */
void ble_send_reading(uint16_t t,
                      uint32_t vrms_mV,
                      uint32_t irms_mA,
                      uint32_t p_mW,
                      uint16_t pf_milli);

/* Min / max real power tracked by the ATmega, in milliwatts. */
void ble_send_minmax(uint32_t p_min_mW, uint32_t p_max_mW);

/* Helper, also useful for the 7-segment display: prints milli-units as a decimal
 * with 1-3 decimal places, rounded, e.g. (12984, 2) -> "12.98". */
void ble_send_fixed(uint32_t milli, uint8_t decimals);

#endif /* BLE_REPORT_H_ */

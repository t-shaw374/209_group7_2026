/*
 * ble_report.h  -  sends computed measurements to the BLE app (via the ESP32 bridge)
 *
 * Every value is computed on the ATmega; the ESP32 only forwards the bytes and the
 * app only draws what it receives. Lines are plain ASCII ending in '\n':
 *
 *   A,<min>,<max>,<unit>                 graph axis chosen by the ATmega
 *       e.g.  A,0.00,8.00,W
 *   D,<t>,<Vrms>,<Irms>,<P>,<PF>,<E>     one reading
 *       e.g.  D,42.0,12.98,0.263,3.41,0.99,0.040
 *       t    = seconds since power-up / RESET (1 dp)
 *       Vrms = volts (2 dp), Irms = amps (3 dp), P = watts (2 dp), PF (2 dp)
 *       E    = energy in watt-hours (3 dp)
 *   M,<Pmin>,<Pmax>                      min / max real power so far, watts (2 dp)
 *   OK,<command> / ERR,<command>         reply to a command from the app
 *   S,<key>=<value>,...                  reply to STATUS
 *
 * Uses usart_transmit() from UART.c.
 */

#ifndef BLE_REPORT_H_
#define BLE_REPORT_H_

#include <stdint.h>

void ble_send_axis(uint32_t p_min_mW, uint32_t p_max_mW);
void ble_send_reading(uint32_t t_ms, uint32_t vrms_mV, uint32_t irms_mA,
                      uint32_t p_mW, uint16_t pf_milli, uint32_t e_mWh);
void ble_send_minmax(uint32_t p_min_mW, uint32_t p_max_mW);

/* building blocks for replies */
void ble_send_str(const char *s);
void ble_send_P(const char *flash_str);        /* string in flash: ble_send_P(PSTR("...")) */
void ble_send_uint(uint32_t v);
/* milli-units as a decimal with 1-3 decimal places, rounded: (12984, 2) -> "12.98" */
void ble_send_fixed(uint32_t milli, uint8_t decimals);

#endif /* BLE_REPORT_H_ */

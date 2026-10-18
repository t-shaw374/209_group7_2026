/*
 * ble_cmd.h  -  receive command lines sent from the app (through the ESP32 bridge)
 *
 * Bytes arrive on the UART RX pin (PD0). An RX interrupt collects them, so no
 * characters are lost while main() is busy or in _delay_ms(). Lines end in '\n'.
 *
 *   ble_cmd_init();   after usart_init(), then sei()
 *   char cmd[BLE_CMD_MAX];
 *   if (ble_cmd_get(cmd)) { if (strcmp(cmd, "RESETMM") == 0) { ... } }
 */

#ifndef BLE_CMD_H_
#define BLE_CMD_H_

#include <stdint.h>

#define BLE_CMD_MAX 24   /* longest command, including the terminating '\0' */

void    ble_cmd_init(void);
/* Copies the latest complete line (without '\n') into out; returns 1 if there was one. */
uint8_t ble_cmd_get(char out[BLE_CMD_MAX]);

#endif /* BLE_CMD_H_ */

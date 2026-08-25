/*
 * Lab - UART formatted output submission
 * Author : taye752
 */
#define F_CPU 2000000UL


#include <util/delay.h>
#include "uart_header.h"

#define BAUD 9600
#define UBRR_VALUE ((F_CPU / ((uint32_t)16 * BAUD)) - 1) // 2,000,000 / (16 * 9600) - 1 = 12  -> 9600 baud
#define SEND_DELAY 1000 //ms

#define RMSVoltage 14.5          // XX.X
#define PeakCurrent 125          // XXX
#define Power 1.60               // X.XX



int main(void)
{
    usart_init(UBRR_VALUE);

    while (1)
    {
        // --- RMS Voltage: XX.X ---
        // scale by 10, round, then split
        uint16_t v_scaled = (uint16_t)(RMSVoltage * 10.0 + 0.5);   // 14.5 -> 145
        uint8_t v_tens  = (v_scaled / 100) % 10;                    // 1
        uint8_t v_ones  = (v_scaled / 10)  % 10;                    // 4
        uint8_t v_frac  = v_scaled % 10;                            // 5

        usart_print_string("RMS Voltage is: ");
        usart_transmit(v_tens + '0');
        usart_transmit(v_ones + '0');
        usart_transmit('.');
        usart_transmit(v_frac + '0');
        usart_transmit('\r');
        usart_transmit('\n');

        // --- Peak Current: XXX ---
        uint16_t i_val = (uint16_t)PeakCurrent;                     // 125
        uint8_t i_hund = (i_val / 100) % 10;                        // 1
        uint8_t i_tens = (i_val / 10)  % 10;                        // 2
        uint8_t i_ones = i_val % 10;                                // 5

        usart_print_string("Peak Current is: ");
        usart_transmit(i_hund + '0');
        usart_transmit(i_tens + '0');
        usart_transmit(i_ones + '0');
        usart_transmit('\r');
        usart_transmit('\n');

        // --- Power: X.XX ---
        // scale by 100, round, then split
        uint16_t p_scaled = (uint16_t)(Power * 100.0 + 0.5);       // 1.60 -> 160
        uint8_t p_ones  = (p_scaled / 100) % 10;                    // 1
        uint8_t p_tenth = (p_scaled / 10)  % 10;                    // 6
        uint8_t p_hundr = p_scaled % 10;                            // 0

        usart_print_string("Power is: ");
        usart_transmit(p_ones  + '0');
        usart_transmit('.');
        usart_transmit(p_tenth + '0');
        usart_transmit(p_hundr + '0');
        usart_transmit('\r');
        usart_transmit('\n');

        usart_transmit('\r');   // blank line between blocks (matches brief)
        usart_transmit('\n');

        _delay_ms(SEND_DELAY);
    }
}
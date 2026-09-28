/*
 * sh1106.h
 *
 * Created: 28/09/2026 2:39:38 pm
 *  Author: thoma
 */ 


#ifndef SH1106_H_
#define SH1106_H_

#include <avr/io.h>

#define SH1106_ADDR    0x3C   // 7-bit address; some modules use 0x3D

#define SCREEN_WIDTH   128
#define SCREEN_PAGES   8      // 64 rows / 8 rows-per-page

void sh1106_init(void);
void sh1106_command(uint8_t cmd);

// Point the controller's internal write pointer at the start of a given page.
void sh1106_set_page(uint8_t page);

// Stream exactly `len` bytes of pixel data into the current page,
// starting at the controller's current column (0 after sh1106_set_page).
void sh1106_write_page_data(const uint8_t *data, uint8_t len);




#endif /* SH1106_H_ */
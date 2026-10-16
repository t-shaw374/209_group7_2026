/*
 * _7seg.h
 *
 * Created: 16/10/2026 8:49:38 pm
 *  Author: thoma
 */ 


#ifndef SEGDISPLAY_H_
#define SEGDISPLAY_H_

#include <stdint.h>

void init_display(uint8_t brightness);
void seperate_and_load_characters(uint16_t number, uint8_t decimal_pos);
void send_next_character_to_display(void);



#endif /* 7SEG_H_ */
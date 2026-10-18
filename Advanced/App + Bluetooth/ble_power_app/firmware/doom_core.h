/*
 * doom_core.h  -  tiny first-person "DOOM mode" for the 128 x 32 OLED
 *
 * A Wolfenstein-style raycaster in fixed point: 64 rays per frame (2 px wide),
 * walls shaded by side, demons drawn as scaled sprites, a gun, a crosshair and a
 * health bar. Pure C apart from oled_column(), so it can be rendered on a PC.
 *
 * Keys (one letter per line from the app): w/s move, a/d turn, q/e strafe, f fire.
 */

#ifndef DOOM_CORE_H_
#define DOOM_CORE_H_

#include <stdint.h>

#define DOOM_EV_KILL 0x01
#define DOOM_EV_DEAD 0x02
#define DOOM_EV_HURT 0x04

void     doom_reset(uint16_t seed);     /* new game, title screen first */
void     doom_key(char k);
void     doom_update(uint16_t dt_ms);   /* move demons, timers */
void     doom_render(void);             /* one full frame via oled_column() */

uint8_t  doom_take_events(void);        /* DOOM_EV_* since the last call */
uint16_t doom_kills(void);
int8_t   doom_health(void);

#endif /* DOOM_CORE_H_ */

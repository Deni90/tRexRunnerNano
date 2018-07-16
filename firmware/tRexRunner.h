/*
 * tRexRunner.h
 *
 *  Created on: Jul 15, 2018
 *      Author: Daniel Knezevic
 */

#ifndef TREXRUNNER_H_
#define TREXRUNNER_H_

#include "ssd1306/ssd1306.h"

#define WIDTH                   SSD1306_LCDWIDTH
#define HEIGHT                  SSD1306_LCDHEIGHT

#define RENDER_PERIOD           10 // 100 FPS

#define HORIZON_LINE_WIDTH      WIDTH
#define HORIZON_LINE_HEIGHT     3

#define TREX_STANDING_WIDTH     14
#define TREX_STANDING_HEIGHT    14

#define TREX_DUCKING_WIDTH      22
#define TREX_DUCKING_HEIGHT     9

#define PTERODACTYL_WIDTH       11
#define PTERODACTYL_HEIGHT      10
#define PTERODACTYL_WING_SWAP   20

#define TREX_RUNNING_SPEED      10
#define TREX_MAX_JUMP_HEIGHT    (HEIGHT - 2)

#define GAME_GRAVITY            0.65f
#define GAME_SPEED              0.4f
#define JUMPING_SPEED           0.55f

typedef enum trex_states_e {
    CRASHED = 0, DUCKING, JUMPING, RUNNING, WAITING
} trex_states_t;

typedef struct game_object_s {
    uint8_t x;
    uint8_t y;
    uint8_t width;
    uint8_t height;
    uint8_t *sprite;
} game_object_t;

void FB_Clear();

#endif /* TREXRUNNER_H_ */

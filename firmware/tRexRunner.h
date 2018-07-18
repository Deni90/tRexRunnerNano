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

#define TREX_RUNNING_SPEED      8
#define TREX_MAX_JUMP_HEIGHT    (HEIGHT - 2)

#define GAME_GRAVITY            1.1f
#define GAME_INITIAL_SPEED      1.0f
#define JUMPING_SPEED           1.05f

#define DEBOUNCE_INTERVAL       50

#define BUTTON_IOPORTNAME       D
#define LEFT_BUTTON_BIT         0
#define RIGHT_BUTTON_BIT        1

typedef enum trex_states_e {
    CRASHED = 0, DUCKING, JUMPING, RUNNING, WAITING
} trex_states_t;

typedef struct game_object_s {
    uint8_t x;
    uint8_t y;
    uint8_t width;
    uint8_t height;
    const uint8_t *sprite;
} game_object_t;

void FB_Clear();
void FB_drawImage(int8_t x, int8_t y, const __flash uint8_t* image, uint8_t width, uint8_t height);
void FB_drawGameObject(game_object_t game_object);

#endif /* TREXRUNNER_H_ */

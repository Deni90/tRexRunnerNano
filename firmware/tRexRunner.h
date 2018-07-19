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

#define PTERODACTYL_WIDTH               11
#define PTERODACTYL_HEIGHT              10
#define PTERODACTYL_WING_SWAP           20

#define PTERODACTYL_MIN_FLY_HEIGHT      (HEIGHT - PTERODACTYL_HEIGHT)
#define PTERODACTYL_MAX_FLY_HEIGHT      (HEIGHT - TREX_STANDING_HEIGHT - 3 - PTERODACTYL_HEIGHT)

#define TREX_RUNNING_SPEED      8
#define TREX_MAX_JUMP_HEIGHT    (HEIGHT - 2)

#define CACTUS_MAX_COUNT                2
#define CACTUS_NUMBER_OF_SPECIES        4
#define CACTUS_PADDING_BOTTOM           2
#define CACTUS_RESPAWN_BASE_DELAY       50  // frames
#define CACTUS_RESPAWN_MAX_DELAY        150 // frames


#define GAME_GRAVITY            1.1f
#define GAME_INITIAL_SPEED      0.5f
#define GAME_SPEED_DELTA        0.001f
#define GAME_SPEED_UPDATE_TIME  250 //mS
#define JUMPING_SPEED           1.05f

#define DEBOUNCE_INTERVAL       50

#define BUTTON_IOPORTNAME       D
#define LEFT_BUTTON_BIT         0
#define RIGHT_BUTTON_BIT        1

#define TRUE                    1
#define FALSE                   0

typedef enum trex_states_e {
    WAITING = 0, RUNNING, DUCKING, JUMPING, CRASHED
} trex_states_t;

typedef struct game_object_s {
    int16_t x;
    int16_t y;
    uint8_t width;
    uint8_t height;
    const uint8_t *sprite;
    float delta;
    uint8_t visible;
} game_object_t;

void FB_clear();
void FB_drawImage(int16_t x, int16_t y, const __flash uint8_t* image, uint8_t width, uint8_t height);
void FB_drawGameObject(game_object_t game_object);

void GAME_UpdateHorizon(game_object_t *horizon);

void GAME_CreatePterodactyl(game_object_t *pterodactyl);
void GAME_UpdatePterodactyl(game_object_t *pterodactyl);

void GAME_CreateCactus(game_object_t *cactus);
void GAME_UpdateCactus(game_object_t *cactus);
uint8_t GAME_CountVisibleCactuses(game_object_t cactus[]);

void GAME_UpdateRunningTrex(game_object_t *trex);
void GAME_UpdateDuckingTrex(game_object_t *trex);
void GAME_UpdateJumpingTrex(game_object_t *trex, trex_states_t *trex_state);
void GAME_UpdateChrashedTrex(game_object_t *trex);
void GAME_UpdateTrex(game_object_t *trex, trex_states_t *trex_state);

#endif /* TREXRUNNER_H_ */

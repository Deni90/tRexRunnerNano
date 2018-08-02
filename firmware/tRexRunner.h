/*
 * tRexRunner.h
 *
 *  Created on: Jul 15, 2018
 *      Author: Daniel Knezevic
 */

#ifndef TREXRUNNER_H_
#define TREXRUNNER_H_

#include "ssd1306/ssd1306.h"

#define WIDTH                           SSD1306_LCDWIDTH
#define HEIGHT                          SSD1306_LCDHEIGHT

#define RENDER_PERIOD                   20 // 50 FPS

#define PTERODACTYL_WING_SWAP           20
#define PTERODACTYL_MIN_FLY_HEIGHT      (HEIGHT - PTERODACTYL_HEIGHT)
#define PTERODACTYL_MAX_FLY_HEIGHT      (HEIGHT - TREX_STANDING_HEIGHT - 3 - PTERODACTYL_HEIGHT)

#define TREX_RUNNING_SPEED              4
#define TREX_MAX_JUMP_HEIGHT            (HEIGHT - 2)

#define CACTUS_MAX_COUNT                2
#define CACTUS_NUMBER_OF_SPECIES        4
#define CACTUS_PADDING_BOTTOM           2
#define CACTUS_RESPAWN_BASE_DELAY       30  // frames
#define CACTUS_RESPAWN_MAX_DELAY        150 // frames
#define CACTUS_RESPAWN_MAX_LIMIT        10  // frames
#define CACTUS_RESPAWN_DELAY_DECREMENT  5  // frames

#define SHOW_PTERODACTYL                150

#define GAME_GRAVITY                    1.6f
#define GAME_INITIAL_SPEED              2.5f
#define GAME_SPEED_DELTA                0.03f
#define JUMPING_SPEED                   1.9f
#define GAME_SCORE_INCREMENT            100 // mS

#define DEBOUNCE_INTERVAL               50

#define BUTTON_IOPORTNAME               D
#define LEFT_BUTTON_BIT                 0
#define RIGHT_BUTTON_BIT                1

#define TRUE                            1
#define FALSE                           0

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

typedef struct horizon_s {
    int16_t x;
    int16_t y;
    uint8_t width;
    uint8_t height;
    uint8_t bump1_x;
    uint8_t bump1_width;
    float bump1_delta;
    uint8_t bump2_x;
    uint8_t bump2_width;
    float bump2_delta;
} horizon_t;

void FB_Clear();
uint8_t FB_DrawImage(int16_t x, int16_t y, const __flash uint8_t* image, uint8_t width, uint8_t height);
void FB_DrawUnsignedValue(int16_t x, int16_t y, uint32_t value);
uint8_t FB_DrawGameObject(game_object_t game_object);
void FB_SetPixel(uint8_t x, uint8_t y);
void FB_ClearPixel(uint8_t x, uint8_t y);

void GAME_Init();
void GAME_ShowScore();

void GAME_InitHorizon();
void GAME_UpdateHorizon();

void GAME_InitPrerodactyl(game_object_t *pterodactyl);
void GAME_CreatePterodactyl(game_object_t *pterodactyl);
void GAME_UpdatePterodactyl(game_object_t *pterodactyl);

void GAME_InitCactus(game_object_t *cactus);
void GAME_CreateCactus(game_object_t *cactus);
void GAME_UpdateCactus(game_object_t *cactus);
uint8_t GAME_CountVisibleCactuses(game_object_t cactus[]);

void GAME_InitTrex();
void GAME_UpdateRunningTrex();
void GAME_UpdateDuckingTrex();
void GAME_UpdateJumpingTrex();
void GAME_UpdateChrashedTrex();
void GAME_UpdateTrex();

#endif /* TREXRUNNER_H_ */

/*
 * tRexRunner.h
 *
 *  Created on: Dec 14, 2024
 *      Author: Daniel Knezevic
 */

#ifndef TREXRUNNER_H_
#define TREXRUNNER_H_

#include "ssd1306/ssd1306.h"
#include "sprites.h"

#define WIDTH                           SSD1306_WIDTH
#define HEIGHT                          SSD1306_HEIGHT

#define RENDER_PERIOD                   20 // 50 FPS

#define TREX_RUNNING_SPEED              4
#define TREX_MAX_JUMP_HEIGHT            (HEIGHT - HORIZON_LINE_HEIGHT - 2)

#define CACTUS_MAX_COUNT                3

#define PTERODACTYL                     CACTUS_MAX_COUNT
#define PTERODACTYL_WING_SWAP           16
#define PTERODACTYL_FLYING_HEIGHTS_CNT  3
#define PTERODACTYL_MIN_FLY_HEIGHT      (HEIGHT - PTERODACTYL_HEIGHT)
#define PTERODACTYL_MID_FLY_HEIGHT      (HEIGHT - TREX_DUCKING_HEIGHT - 3 - PTERODACTYL_HEIGHT)
#define PTERODACTYL_MAX_FLY_HEIGHT      (HEIGHT - TREX_STANDING_HEIGHT - 3 - PTERODACTYL_HEIGHT)

#define OBSTACLE_RESPAWN_BASE_DISTANCE  50  // px
#define OBSTACLE_RESPAWN_DISTANCE_INC   5   // px
#define SHOW_PTERODACTYL                120 // px

#define GAME_GRAVITY                    0.8f
#define GAME_INITIAL_SPEED              1.5f
#define GAME_SPEED_DELTA                0.1f
#define JUMPING_SPEED                   2.0f
#define GAME_SCORE_INC_FREQ             50 // mS

#define INVERTED_MODE_THRESHOLD         1000

#define HI_SCORE_Y                      1

#define DEBOUNCE_INTERVAL               50

#define LEFT_BUTTON_GPIO                1 // GPIO1
#define RIGHT_BUTTON_GPIO               0 // GPIO0

typedef enum trex_states_e {
    RUNNING = 0, DUCKING, JUMPING, CRASHED
} trex_states_t;

typedef struct game_object_s {
    float x;
    float y;
    uint8_t width;
    uint8_t height;
    const uint8_t *sprite;
    bool visible;
} game_object_t;

typedef struct horizon_s {
    int16_t x;
    int16_t y;
    uint8_t width;
    uint8_t height;
    float bump1_x;
    uint8_t bump1_width;
    float bump2_x;
    uint8_t bump2_width;
} horizon_t;

void BUTTONS_Init();
void BUTTONS_MonitorButtons();

void FB_Clear();
bool FB_DrawImage(int16_t x, int16_t y, const uint8_t* image, uint8_t width, uint8_t height);
void FB_DrawUnsignedValue(int16_t x, int16_t y, uint32_t value);
bool FB_DrawGameObject(game_object_t game_object);
void FB_SetPixel(uint8_t x, uint8_t y);
void FB_InvertColor();
void FB_DrawRectangle(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t fill);

void GAME_Init();
void GAME_ShowScore();
void GAME_HandleState();
void GAME_AdjustDifficulty();

void GAME_InitHorizon();
void GAME_UpdateHorizon();

void GAME_InitPrerodactyl(game_object_t *pterodactyl);
void GAME_CreatePterodactyl(game_object_t *pterodactyl);
void GAME_UpdatePterodactyl(game_object_t *pterodactyl);

void GAME_InitCactus(game_object_t *cactus);
void GAME_CreateCactus(game_object_t *cactus);
void GAME_UpdateCactus(game_object_t *cactus);
uint8_t GAME_CountVisibleCacti(game_object_t cactus[]);

void GAME_InitTrex();
void GAME_UpdateRunningTrex();
void GAME_UpdateDuckingTrex();
void GAME_UpdateJumpingTrex();
void GAME_UpdateTrex();

#endif /* TREXRUNNER_H_ */

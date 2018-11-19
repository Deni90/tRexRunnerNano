/*
 * tRexRunner.h
 *
 *  Created on: Jul 15, 2018
 *      Author: Daniel Knezevic
 */

#ifndef TREXRUNNER_H_
#define TREXRUNNER_H_

#include "ssd1306/ssd1306.h"
#include "sprites.h"

#define WIDTH                           SSD1306_LCDWIDTH
#define HEIGHT                          SSD1306_LCDHEIGHT

#define RENDER_PERIOD                   20 // 50 FPS

#define TREX_RUNNING_SPEED              2
#define TREX_MAX_JUMP_HEIGHT            (HEIGHT - HORIZON_LINE_HEIGHT - 1)

#define CACTUS_MAX_COUNT                3

#define PTERODACTYL                     CACTUS_MAX_COUNT
#define PTERODACTYL_WING_SWAP           8
#define PTERODACTYL_FLYING_HEIGHTS_CNT  3
#define PTERODACTYL_MIN_FLY_HEIGHT      (HEIGHT - PTERODACTYL_HEIGHT)
#define PTERODACTYL_MID_FLY_HEIGHT      (HEIGHT - TREX_DUCKING_HEIGHT - 3 - PTERODACTYL_HEIGHT)
#define PTERODACTYL_MAX_FLY_HEIGHT      (HEIGHT - TREX_STANDING_HEIGHT - 3 - PTERODACTYL_HEIGHT)

#define OBSTACLE_RESPAWN_BASE_DISTANCE  50  // px
#define OBSTACLE_RESPAWN_DISTANCE_INC   5   // px
#define SHOW_PTERODACTYL                120 // px

#define GAME_GRAVITY                    3.2f
#define GAME_INITIAL_SPEED              3.8f
#define GAME_SPEED_DELTA                0.4f
#define JUMPING_SPEED                   3.5f
#define GAME_SCORE_INCREMENT            50 // mS

#define INVERTED_MODE_THRESHOLD         1000

#define HI_SCORE_Y                      1

#define DEBOUNCE_INTERVAL               50

#define BUTTON_IOPORTNAME               B
#define LEFT_BUTTON_BIT                 1
#define RIGHT_BUTTON_BIT                0

#define USB_PWR_IOPORTNAME              D
#define USB_PWR_BIT                     4

#define CHG_PIN_IOPORTNAME              D
#define CHG_PIN_BIT                     5

#define AUTO_CUTOFF_IOPORTNAME          D
#define AUTO_CUTOFF_BIT                 6

#define TRUE                            1
#define FALSE                           0

typedef enum trex_states_e {
    WAITING = 0, RUNNING, DUCKING, JUMPING, CRASHED
} trex_states_t;

typedef struct game_object_s {
    float x;
    float y;
    uint8_t width;
    uint8_t height;
    const uint8_t *sprite;
    uint8_t visible;
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

void FB_Clear();
uint8_t FB_DrawImage(int16_t x, int16_t y, const __flash uint8_t* image, uint8_t width, uint8_t height);
void FB_DrawUnsignedValue(int16_t x, int16_t y, uint32_t value);
uint8_t FB_DrawGameObject(game_object_t game_object);
void FB_SetPixel(uint8_t x, uint8_t y);
void FB_ClearPixel(uint8_t x, uint8_t y);
void FB_InvertColor();

void GAME_Init();
void GAME_ShowScore();
void GAME_BackupHighScore();
void GAME_RestoreHighScore();
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
uint8_t GAME_CountVisibleCactuses(game_object_t cactus[]);

void GAME_InitTrex();
void GAME_UpdateRunningTrex();
void GAME_UpdateDuckingTrex();
void GAME_UpdateJumpingTrex();
void GAME_UpdateChrashedTrex();
void GAME_UpdateTrex();

#endif /* TREXRUNNER_H_ */

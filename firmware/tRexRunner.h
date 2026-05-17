#ifndef trexrunner_h
#define trexrunner_h

#include <inttypes.h>

#include "ssd1306.h"

#define WIDTH  SSD1306_WIDTH
#define HEIGHT SSD1306_HEIGHT

#define RENDER_PERIOD 10   // 100 FPS

#define TREX_RUNNING_SPEED   8
#define TREX_MAX_JUMP_HEIGHT (HEIGHT - HORIZON_LINE_HEIGHT - 2)

#define CACTUS_MAX_COUNT 3

#define PTERODACTYL                    CACTUS_MAX_COUNT
#define PTERODACTYL_WING_SWAP          25
#define PTERODACTYL_FLYING_HEIGHTS_CNT 3
#define PTERODACTYL_MIN_FLY_HEIGHT     (HEIGHT - PTERODACTYL_HEIGHT)
#define PTERODACTYL_MID_FLY_HEIGHT                                             \
    (HEIGHT - TREX_DUCKING_HEIGHT - 3 - PTERODACTYL_HEIGHT)
#define PTERODACTYL_MAX_FLY_HEIGHT                                             \
    (HEIGHT - TREX_STANDING_HEIGHT - 3 - PTERODACTYL_HEIGHT)

#define OBSTACLE_RESPAWN_BASE_DISTANCE 50    // px
#define OBSTACLE_RESPAWN_DISTANCE_INC  5     // px
#define SHOW_PTERODACTYL               120   // px

#define GAME_GRAVITY         0.8f
#define GAME_INITIAL_SPEED   1.1f
#define GAME_SPEED_DELTA     0.05f
#define JUMPING_SPEED        0.9f
#define GAME_SCORE_INCREMENT 70   // mS

#define INVERTED_MODE_THRESHOLD 1000

#define HI_SCORE_Y 1

#define DEBOUNCE_INTERVAL 50

#define LEFT_BUTTON_GPIO  PC3
#define RIGHT_BUTTON_GPIO PC4

#define TIMEOUT_INTERVAL 1500   // mS
#define STARTUP_INTERVAL 1000   // mS

#define INACTIVITY_PERIOD 30000   // mS

#define MIN_BATTERY_VOLTAGE    3600    // mV
#define BATTERY_MONITOR_PERIOD 30000   // milliseconds

#define LOW_BATTERY_ALERT_DURATION 1500   // mS

#define HIGH_SCORE_RESET_TIME 10000   // mS

typedef enum trex_states_e {
    RUNNING = 0,
    DUCKING,
    JUMPING,
    CRASHED
} trex_states_t;

typedef struct game_object_s {
    float x;
    float y;
    uint8_t width;
    uint8_t height;
    const uint8_t* sprite;
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

void TIMER_Init();

void BUTTONS_Init();
void BUTTONS_MonitorButtons();

void FB_Clear();
uint8_t FB_DrawImage(int16_t x, int16_t y, const uint8_t* image, uint8_t width,
                     uint8_t height);
void FB_DrawUnsignedValue(int16_t x, int16_t y, uint32_t value);
uint8_t FB_DrawGameObject(game_object_t game_object);
void FB_SetPixel(uint8_t x, uint8_t y);
void FB_InvertColor();
void FB_DrawRectangle(uint8_t x, uint8_t y, uint8_t width, uint8_t height,
                      uint8_t fill);

void GAME_Init();
void GAME_ShowScore();
void GAME_HandleState();
void GAME_AdjustDifficulty();

void GAME_InitHorizon();
void GAME_UpdateHorizon();

void GAME_InitPrerodactyl(game_object_t* pterodactyl);
void GAME_CreatePterodactyl(game_object_t* pterodactyl);
void GAME_UpdatePterodactyl(game_object_t* pterodactyl);

void GAME_InitCactus(game_object_t* cactus);
void GAME_CreateCactus(game_object_t* cactus);
void GAME_UpdateCactus(game_object_t* cactus);
uint8_t GAME_CountVisibleCactuses(game_object_t cactus[]);

void GAME_InitTrex();
void GAME_UpdateRunningTrex();
void GAME_UpdateDuckingTrex();
void GAME_UpdateJumpingTrex();
void GAME_UpdateTrex();

#endif   // trexrunner_h

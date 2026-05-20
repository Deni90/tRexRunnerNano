//------------------------------------------------------------------------------
// Includes
//------------------------------------------------------------------------------

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "ch32fun.h"
#include "lib_rand.h"

#include "sprites.h"
#include "ssd1306.h"

//------------------------------------------------------------------------------
// Configuration include
//------------------------------------------------------------------------------

#include "tRexRunner.h"

//------------------------------------------------------------------------------
// Macros
//------------------------------------------------------------------------------

#define WIDTH  SSD1306_WIDTH
#define HEIGHT SSD1306_HEIGHT

#define TREX_MAX_JUMP_HEIGHT (HEIGHT - HORIZON_LINE_HEIGHT - 2)

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

#define INVERTED_MODE_THRESHOLD 1000   // points

#define HI_SCORE_Y 1

#define DEBOUNCE_INTERVAL 50   // mS

#define JUMP_BUTTON_BIT          0
#define JUMP_BUTTON_GPIO         PC3
#define IS_JUMP_BUTTON_PRESSED() (button_state & (1 << JUMP_BUTTON_BIT))

#define DUCK_BUTTON_BIT          1
#define DUCK_BUTTON_GPIO         PC4
#define IS_DUCK_BUTTON_PRESSED() (button_state & (1 << DUCK_BUTTON_BIT))

#define AUTOCUTOFF_GPIO PD3

#define TIMEOUT_INTERVAL  1500    // mS
#define STARTUP_INTERVAL  1000    // mS
#define INACTIVITY_PERIOD 30000   // mS

#define MIN_BATTERY_VOLTAGE        3600    // mV
#define BATTERY_MONITOR_PERIOD     30000   // milliseconds
#define LOW_BATTERY_ALERT_DURATION 1500    // mS

#define HIGH_SCORE_RESET_TIME 10000   // mS

#define FLASH_TARGET_ADDR 0x08003FC0

//------------------------------------------------------------------------------
// Type definitions
//------------------------------------------------------------------------------

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

//------------------------------------------------------------------------------
// Forward declarations
//------------------------------------------------------------------------------

void TIM1_UP_IRQHandler(void) __attribute__((interrupt));
static void TIMER_Init();

static void BUTTONS_Init();
static void BUTTONS_MonitorButtons();

static uint32_t FLASH_Read_u32(uint32_t address);
static void FLASH_Write_u32(uint32_t address, uint32_t val);

static void POWER_MANAGER_init();
static void POWER_MANAGER_turnOff();
static void POWER_MANAGER_MonitorInactivity();
static uint16_t POWER_MANAGER_ReadBatteryVoltage();
static void POWER_MANAGER_MonitorBattery();
static void POWER_MANAGER_ShowBatteryStatus(uint8_t x, uint8_t y,
                                            uint8_t progress);

static void FB_Clear();
static uint8_t FB_DrawImage(int16_t x, int16_t y, const uint8_t* image,
                            uint8_t width, uint8_t height);
static void FB_DrawUnsignedValue(int16_t x, int16_t y, uint32_t value);
static uint8_t FB_DrawGameObject(game_object_t game_object);
static void FB_SetPixel(uint8_t x, uint8_t y);
static void FB_InvertColor();
static void FB_DrawRectangle(uint8_t x, uint8_t y, uint8_t width,
                             uint8_t height, uint8_t fill);

static void GAME_Init();
static void GAME_ShowScore();
static void GAME_HandleState();
static void GAME_AdjustDifficulty();

static void GAME_InitHorizon();
static void GAME_UpdateHorizon();

static void GAME_InitPrerodactyl(game_object_t* pterodactyl);
static void GAME_CreatePterodactyl(game_object_t* pterodactyl);
static void GAME_UpdatePterodactyl(game_object_t* pterodactyl);

static void GAME_InitCactus(game_object_t* cactus);
static void GAME_CreateCactus(game_object_t* cactus);
static void GAME_UpdateCactus(game_object_t* cactus);
static uint8_t GAME_CountVisibleCacti(game_object_t cactus[]);

static void GAME_InitTrex();
static void GAME_UpdateRunningTrex();
static void GAME_UpdateDuckingTrex();
static void GAME_UpdateJumpingTrex();
static void GAME_UpdateTrex();

//------------------------------------------------------------------------------
// Global variables
//------------------------------------------------------------------------------

static uint8_t frame_buffer[WIDTH * HEIGHT / 8];

volatile uint16_t global_clock = 0;
volatile uint8_t lb_debounce_clock = 0;
volatile uint8_t rb_debounce_clock = 0;
volatile uint16_t game_speed_update_clock = 0;
volatile uint16_t inactivity_clock = 0;
volatile uint16_t battery_monitor_clock = 0;

static float game_speed = GAME_INITIAL_SPEED;
static uint32_t high_score = 0;
static uint32_t score = 0;

static uint8_t button_state = 0x00;
static trex_states_t trex_state = RUNNING;

static horizon_t horizon;
static game_object_t trex;
static game_object_t
    obstacles[CACTUS_MAX_COUNT + 1];   // last element is pterodactyl

static uint8_t latest_cactus = 0;   // index of the newest cactus in the array

static uint16_t obstacle_respawn_base_distance = OBSTACLE_RESPAWN_BASE_DISTANCE;
// make sure WIDTH > OBSTACLE_RESPAWN_BASE_DISTANCE
static uint16_t obstacle_respawn_max_distance =
    WIDTH - OBSTACLE_RESPAWN_BASE_DISTANCE;
static uint16_t show_pterodactyl = SHOW_PTERODACTYL;

static uint8_t inverted_mode = false;

static uint16_t battery_voltage = 0;

// lookup table for pterodactyl flying heights
static const uint8_t pterodactyl_flying_heights[] = {
    PTERODACTYL_MIN_FLY_HEIGHT, PTERODACTYL_MID_FLY_HEIGHT,
    PTERODACTYL_MAX_FLY_HEIGHT};

//------------------------------------------------------------------------------
// Inline functions
//------------------------------------------------------------------------------

static inline int my_floor(float x) {
    int i = (int) x;

    // If x is negative and has fractional part,
    // subtract 1 because C truncates toward zero.
    if (x < 0.0f && x != (float) i) {
        i--;
    }

    return i;
}

//------------------------------------------------------------------------------
// Main
//------------------------------------------------------------------------------

int main() {
    SystemInit();
    funGpioInitAll();   // Enable GPIOs

    BUTTONS_Init();
    TIMER_Init();
    POWER_MANAGER_init();

    Delay_Ms(100);   // give OLED some more time

    SSD1306_Init();

    bool button_released = false;   // used to prevent immediate restart of the
                                    // game while holding the jumping button

    // initialize the game
    GAME_Init();

    // wait until the buttons are released to prevent automatic start of the
    // game
    global_clock = 0;
    while (button_state) {
        if ((button_state & (1 << JUMP_BUTTON_BIT)) &&
            (button_state & (1 << DUCK_BUTTON_BIT))) {
            if (global_clock >= HIGH_SCORE_RESET_TIME && high_score != 0) {
                high_score = 0;
                FLASH_Write_u32(FLASH_TARGET_ADDR, high_score);
                GAME_Init();
            }
        }
        BUTTONS_MonitorButtons();
        // POWER_MANAGER_MonitorBattery();
    }

    // wait for button press to start the game
    while (1) {
        if (!IS_JUMP_BUTTON_PRESSED()) {
            button_released = true;
        }
        if (button_state && button_released) {
            button_released = false;
            break;
        }
        BUTTONS_MonitorButtons();
    }

    // Seed the Random Number Generator.
    seed(global_clock);   // initialize PRNG

    while (1) {
        BUTTONS_MonitorButtons();
        // GAME OVER
        if (trex_state == CRASHED) {
            if (!IS_JUMP_BUTTON_PRESSED())
                button_released = true;

            // wait for jump button to restart the game
            if (IS_JUMP_BUTTON_PRESSED() && button_released) {
                button_released = false;
                GAME_Init();
            } else {
                if (inverted_mode)
                    FB_InvertColor();   // restore to original buffer
                FB_DrawImage(WIDTH / 2 - GAME_OVER_SPLASH_WIDTH / 2, 10,
                             game_over_splash, GAME_OVER_SPLASH_WIDTH,
                             GAME_OVER_SPLASH_HEIGHT);
                if (inverted_mode)
                    FB_InvertColor();   // invert back
                if (score > high_score) {
                    SSD1306_Clear();
                    high_score = score;
                    FLASH_Write_u32(FLASH_TARGET_ADDR, high_score);
                }
                SSD1306_Display(frame_buffer);
                continue;
            }
        }

        GAME_HandleState();

        if (global_clock >= RENDER_PERIOD) {
            global_clock = 0;

            FB_Clear();

            GAME_ShowScore();

            // create new obstacles
            if (GAME_CountVisibleCacti(obstacles) < CACTUS_MAX_COUNT) {
                uint8_t previous_cactus;
                if (latest_cactus > 0) {
                    previous_cactus = latest_cactus - 1;
                } else {
                    previous_cactus = CACTUS_MAX_COUNT - 1;
                }
                if (obstacles[latest_cactus].visible == false &&
                    !obstacles[PTERODACTYL].visible) {
                    if ((int) obstacles[previous_cactus].x <=
                            (WIDTH - obstacles[previous_cactus].width) ||
                        !obstacles[previous_cactus].visible) {
                        GAME_CreateCactus(&obstacles[latest_cactus]);
                        uint16_t random_distance =
                            obstacle_respawn_base_distance +
                            rand() % obstacle_respawn_max_distance;
                        obstacles[latest_cactus].x += random_distance;
                        // respawn pterodatyl?
                        if (random_distance >= show_pterodactyl) {
                            // replace cactus with pterodactyl
                            obstacles[latest_cactus].visible = false;
                            GAME_CreatePterodactyl(
                                &obstacles[CACTUS_MAX_COUNT]);
                            obstacles[PTERODACTYL].x += random_distance;
                        }
                        latest_cactus++;
                    }
                }
                if (latest_cactus == CACTUS_MAX_COUNT) {
                    latest_cactus = 0;
                }
            }

            // update cacti
            for (uint8_t i = 0; i < CACTUS_MAX_COUNT; i++) {
                GAME_UpdateCactus(&obstacles[i]);
            }
            // update pterodactyl
            GAME_UpdatePterodactyl(&obstacles[PTERODACTYL]);

            // update trex
            GAME_UpdateTrex();
            GAME_UpdateHorizon();
            if (inverted_mode)
                FB_InvertColor();
            // RENDER
            SSD1306_Display(frame_buffer);
        }

        // speed up the game periodically
        if (game_speed_update_clock >= GAME_SCORE_INCREMENT) {
            game_speed_update_clock = 0;
            score++;
            GAME_AdjustDifficulty();

            if ((score % INVERTED_MODE_THRESHOLD) == 0) {
                inverted_mode = !inverted_mode;
            }
        }
    }

    return 0;
}

//------------------------------------------------------------------------------
// Function definitions
//------------------------------------------------------------------------------

static void TIMER_Init() {
    // Enable TIM1 clock
    RCC->APB2PCENR |= RCC_APB2Periph_TIM1;
    // Reset timer
    TIM1->CTLR1 = 0;
    // 48 MHz / 48 = 1 MHz timer clock (1 µs per tick)
    TIM1->PSC = 48 - 1;
    // 1000 ticks = 1 ms
    TIM1->ATRLR = 1000 - 1;
    // Reset counter
    TIM1->CNT = 0;
    // Enable update interrupt
    TIM1->INTFR = 0;
    TIM1->DMAINTENR |= TIM_UIE;
    // Enable NVIC interrupt
    NVIC_EnableIRQ(TIM1_UP_IRQn);
    // Start timer
    TIM1->CTLR1 |= TIM_CEN;
}

void TIM1_UP_IRQHandler(void) {
    if (TIM1->INTFR & TIM_UIF) {
        TIM1->INTFR = ~TIM_UIF;   // clear interrupt flag

        global_clock++;
        lb_debounce_clock++;
        rb_debounce_clock++;
        game_speed_update_clock++;
        inactivity_clock++;
        battery_monitor_clock++;
    }
}

static uint32_t FLASH_Read_u32(uint32_t address) {
    return *(volatile uint32_t*) address;   // Direct memory mapping
}

static void FLASH_Write_u32(uint32_t address, uint32_t val) {
    // Code borrowed from ch32fun/examples/flashtest/flashtest.c

    // Unkock flash - be aware you need extra stuff for the bootloader.
    FLASH->KEYR = FLASH_KEY1;
    FLASH->KEYR = FLASH_KEY2;

    // For option bytes.
    // FLASH->OBKEYR = FLASH_KEY1;
    // FLASH->OBKEYR = FLASH_KEY2;

    // For unlocking programming, in general.
    FLASH->MODEKEYR = FLASH_KEY1;
    FLASH->MODEKEYR = FLASH_KEY2;

    printf("FLASH->CTLR = %08lx\n", FLASH->CTLR);
    if (FLASH->CTLR & 0x8080) {
        while (1)
            ;
    }

    uint32_t* ptr = (uint32_t*) address;

    // Erase Page
    FLASH->CTLR = CR_PAGE_ER;
    FLASH->ADDR = (intptr_t) ptr;
    FLASH->CTLR = CR_STRT_Set | CR_PAGE_ER;
    while (FLASH->STATR & FLASH_STATR_BSY)
        ;   // Takes about 3ms.

    // Clear buffer and prep for flashing.
    FLASH->CTLR = CR_PAGE_PG;   // synonym of FTPG.
    FLASH->CTLR = CR_BUF_RST | CR_PAGE_PG;
    FLASH->ADDR = (intptr_t)
        ptr;   // This can actually happen about anywhere toward the end here.

    // Note: It takes about 6 clock cycles for this to finish.
    while (FLASH->STATR & FLASH_STATR_BSY)
        ;   // No real need for this.

    *ptr = val;                                       // Write to the memory
    FLASH->CTLR = CR_PAGE_PG | FLASH_CTLR_BUF_LOAD;   // Load the buffer.
    while (FLASH->STATR & FLASH_STATR_BSY)
        ;   // Only needed if running from RAM.

    // Actually write the flash out. (Takes about 3ms)
    FLASH->CTLR = CR_PAGE_PG | CR_STRT_Set;

    while (FLASH->STATR & FLASH_STATR_BSY)
        ;

    // Lock Flash
    FLASH->CTLR |= FLASH_CTLR_LOCK;
}

static void BUTTONS_Init() {
    funPinMode(JUMP_BUTTON_GPIO, GPIO_CFGLR_IN_PUPD);
    funDigitalWrite(JUMP_BUTTON_GPIO, FUN_HIGH);
    funPinMode(DUCK_BUTTON_GPIO, GPIO_CFGLR_IN_PUPD);
    funDigitalWrite(DUCK_BUTTON_GPIO, FUN_HIGH);
}

static void BUTTONS_MonitorButtons() {
    if (lb_debounce_clock >= DEBOUNCE_INTERVAL) {
        lb_debounce_clock = 0;
        if (!funDigitalRead(JUMP_BUTTON_GPIO))
            button_state |= (1 << JUMP_BUTTON_BIT);
        else
            button_state &= ~(1 << JUMP_BUTTON_BIT);
    } else if ((button_state & (1 << JUMP_BUTTON_BIT)) ==
               !funDigitalRead(JUMP_BUTTON_GPIO)) {
        lb_debounce_clock = 0;
    }

    if (rb_debounce_clock >= DEBOUNCE_INTERVAL) {
        rb_debounce_clock = 0;
        if (!funDigitalRead(DUCK_BUTTON_GPIO))
            button_state |= (1 << DUCK_BUTTON_BIT);
        else
            button_state &= ~(1 << DUCK_BUTTON_BIT);
    } else if ((button_state & (1 << DUCK_BUTTON_BIT)) ==
               !funDigitalRead(DUCK_BUTTON_GPIO)) {
        rb_debounce_clock = 0;
    }
}

static void POWER_MANAGER_init() {
    // TODO initialize GPIO pins
    funPinMode(AUTOCUTOFF_GPIO, GPIO_CFGLR_IN_PUPD);
    funDigitalWrite(AUTOCUTOFF_GPIO, FUN_HIGH);

    // Initializes ADC for battery voltage monitoring
    // code borrowed from ch32fun/examples/adc_polled/adc_polled.c:adc_init()
    // ADCCLK = 24 MHz => RCC_ADCPRE = 0: divide by 2
    RCC->CFGR0 &= ~(0x1F << 11);

    // Enable GPIOD and ADC
    RCC->APB2PCENR |= RCC_APB2Periph_GPIOD | RCC_APB2Periph_ADC1;

    // PD4 is analog input chl 7
    GPIOD->CFGLR &= ~(0xf << (4 * 4));   // CNF = 00: Analog, MODE = 00: Input

    // Reset the ADC to init all regs
    RCC->APB2PRSTR |= RCC_APB2Periph_ADC1;
    RCC->APB2PRSTR &= ~RCC_APB2Periph_ADC1;

    // Set up single conversion on chl 7
    ADC1->RSQR1 = 0;
    ADC1->RSQR2 = 0;
    ADC1->RSQR3 = 7;   // 0-9 for 8 ext inputs and two internals

    // set sampling time for chl 7
    ADC1->SAMPTR2 &= ~(ADC_SMP0 << (3 * 7));
    ADC1->SAMPTR2 |= 7 << (3 * 7);   // 0:7 => 3/9/15/30/43/57/73/241 cycles

    // turn on ADC and set rule group to sw trig
    ADC1->CTLR2 |= ADC_ADON | ADC_EXTSEL;

    // Reset calibration
    ADC1->CTLR2 |= ADC_RSTCAL;
    while (ADC1->CTLR2 & ADC_RSTCAL)
        ;

    // Calibrate
    ADC1->CTLR2 |= ADC_CAL;
    while (ADC1->CTLR2 & ADC_CAL)
        ;

    // should be ready for SW conversion now
}

static void POWER_MANAGER_turnOff() {
    funDigitalWrite(AUTOCUTOFF_GPIO, FUN_LOW);
}

static uint16_t POWER_MANAGER_ReadBatteryVoltage() {
    // Code borrowed and adapted from
    // ch32fun/examples/adc_polled/adc_polled.c:adc_get() start sw conversion

    // (auto clears)
    ADC1->CTLR2 |= ADC_SWSTART;

    // wait for conversion complete
    while (!(ADC1->STATR & ADC_EOC))
        ;

    // get result
    // Voltage divider is returning half of the real voltage from the lipo
    // ADC has 10bit resolution and the voltage reference is Vdd = 3.3V
    float voltage = 3300.0 / 512 * ADC1->RDATAR;
    return (uint16_t) voltage;
}

static void POWER_MANAGER_MonitorBattery() {
    if (battery_monitor_clock >= BATTERY_MONITOR_PERIOD) {
        battery_monitor_clock = 0;   // reset timer
        battery_voltage =
            POWER_MANAGER_ReadBatteryVoltage();   // read battery status
        if (battery_voltage <= MIN_BATTERY_VOLTAGE) {
            POWER_MANAGER_ShowBatteryStatus((WIDTH - BATTERY_ICON_WITH) / 2,
                                            (HEIGHT - BATTERY_ICON_HEIGHT) / 2,
                                            0);
            global_clock = 0;
            while (global_clock < LOW_BATTERY_ALERT_DURATION)
                ;
            POWER_MANAGER_turnOff();
            while (1)
                ;   // wait until the device is powered off
        }
    }
}

static void POWER_MANAGER_ShowBatteryStatus(uint8_t x, uint8_t y,
                                            uint8_t progress) {
    FB_Clear();
    FB_DrawRectangle(x + 2, y + 0, 30, 16, false);
    FB_DrawRectangle(x + 0, y + 4, 2, 8, true);

    FB_DrawRectangle(x + 6, y + 4, progress * 22 / UINT8_MAX, 8, true);
    SSD1306_Display(frame_buffer);
}

static void FB_Clear() {
    memset(frame_buffer, 0, sizeof(uint8_t) * (WIDTH * HEIGHT / 8));
}

static uint8_t FB_DrawImage(int16_t x, int16_t y, const uint8_t* image,
                            uint8_t width, uint8_t height) {
    uint8_t collision = false;
    for (int16_t h = y; h < (y + height); h++) {
        for (int16_t w = x; w < (x + width); w++) {
            if ((w >= WIDTH) || (h >= HEIGHT))
                continue;
            if (h < 0 || w < 0)
                continue;
            uint16_t buffer_index = WIDTH * (h / 8) + w;
            uint8_t image_w = w - x;
            uint8_t image_h = h - y;
            uint16_t image_index = width * (image_h / 8) + image_w;
            if ((image[image_index] >> (image_h % 8) & 0x01) == 0x01) {
                if (frame_buffer[buffer_index] >> (h % 8) & 0x01)
                    collision = true;
                frame_buffer[buffer_index] |= (1 << (h % 8));
            }
        }
    }
    return collision;
}

static void FB_DrawUnsignedValue(int16_t x, int16_t y, uint32_t value) {
    int16_t xx = x;
    for (uint32_t dividend = 10000; dividend > 0; dividend /= 10) {
        FB_DrawImage(xx, y, &digits[(value / dividend % 10) * DIGIT_WIDTH],
                     DIGIT_WIDTH, DIGIT_HEIGHT);
        xx += DIGIT_WIDTH;
    }
}

static uint8_t FB_DrawGameObject(game_object_t game_object) {
    if (!game_object.visible)
        return false;
    return FB_DrawImage(my_floor(game_object.x), my_floor(game_object.y),
                        game_object.sprite, game_object.width,
                        game_object.height);
}

static void FB_SetPixel(uint8_t x, uint8_t y) {
    if (x >= WIDTH || y >= HEIGHT)
        return;

    uint32_t index = WIDTH * (y / 8) + x;
    frame_buffer[index] |= (1 << (y & 7));
}

static void FB_InvertColor() {
    for (uint16_t i = 0; i < (WIDTH * HEIGHT / 8); i++) {
        frame_buffer[i] = ~frame_buffer[i];
    }
}

static void FB_DrawRectangle(uint8_t x, uint8_t y, uint8_t width,
                             uint8_t height, uint8_t fill) {
    if ((x >= WIDTH) || (y >= HEIGHT))
        return;

    uint8_t a, b;

    if ((y + height) > HEIGHT) {
        a = HEIGHT;
    } else {
        a = y + height;
    }

    if ((x + width) > WIDTH) {
        b = WIDTH;
    } else {
        b = x + width;
    }

    for (uint8_t i = y; i < a; i++) {
        for (uint8_t j = x; j < b; j++) {
            if (fill) {
                FB_SetPixel(j, i);
            } else {
                if (i == y || i == (a - 1) || j == x || j == (b - 1)) {
                    FB_SetPixel(j, i);
                }
            }
        }
    }
}

static void GAME_Init() {
    score = 0;
    high_score = FLASH_Read_u32(FLASH_TARGET_ADDR);
    game_speed = GAME_INITIAL_SPEED;
    trex_state = RUNNING;
    latest_cactus = 0;   // index of the newest cactus in the array
    obstacle_respawn_base_distance = OBSTACLE_RESPAWN_BASE_DISTANCE;
    obstacle_respawn_max_distance = WIDTH - OBSTACLE_RESPAWN_BASE_DISTANCE;
    show_pterodactyl = SHOW_PTERODACTYL;
    inverted_mode = false;

    // Seed the Random Number Generator.
    seed(global_clock);

    GAME_InitHorizon();
    GAME_InitTrex();
    for (uint8_t i = 0; i < CACTUS_MAX_COUNT; i++)
        GAME_InitCactus(&obstacles[i]);
    GAME_InitPrerodactyl(&obstacles[PTERODACTYL]);

    FB_Clear();
    GAME_UpdateHorizon();
    GAME_ShowScore();
    GAME_UpdateTrex();

    // RENDER
    SSD1306_Display(frame_buffer);
}

static void GAME_ShowScore() {
    FB_DrawImage(WIDTH - DIGIT_WIDTH * 13, HI_SCORE_Y, hi_score_str,
                 HI_SCORE_STR_WIDTH, HI_SCORE_STR_HEIGHT);
    FB_DrawUnsignedValue(WIDTH - DIGIT_WIDTH * 11, HI_SCORE_Y, high_score);
    FB_DrawUnsignedValue(WIDTH - DIGIT_WIDTH * 5 - 1, HI_SCORE_Y, score);
}

static void GAME_HandleState() {
    // update trex state based on button states
    if (IS_JUMP_BUTTON_PRESSED()) {
        trex_state = JUMPING;
    }
    if (IS_DUCK_BUTTON_PRESSED() && (trex_state != JUMPING)) {
        if (trex_state == RUNNING) {
            // preload a ducking sprite
            trex.sprite = trex_ducking1;
        }
        trex_state = DUCKING;
    } else if (trex_state != JUMPING) {
        if (trex_state == DUCKING) {
            // preload a running sprite
            trex.sprite = trex_running1;
        }
        trex_state = RUNNING;
    }
}

static void GAME_AdjustDifficulty() {
    if ((score % 100) == 0) {
        game_speed += GAME_SPEED_DELTA;
        // increase the distance between obstacles a little bit
        obstacle_respawn_base_distance += OBSTACLE_RESPAWN_DISTANCE_INC;
        obstacle_respawn_max_distance += OBSTACLE_RESPAWN_DISTANCE_INC;
        show_pterodactyl += OBSTACLE_RESPAWN_DISTANCE_INC * 2;
    }
}

static void GAME_InitHorizon() {
    horizon.x = 0;
    horizon.y = HEIGHT - HORIZON_LINE_HEIGHT;
    horizon.width = HORIZON_LINE_WIDTH;
    horizon.height = HORIZON_LINE_HEIGHT;
    horizon.bump1_x = HORIZON_LINE_BUMP1_X;
    horizon.bump1_width = HORIZON_LINE_BUMP1_WIDTH;
    horizon.bump2_x = HORIZON_LINE_BUMP2_X;
    horizon.bump2_width = HORIZON_LINE_BUMP2_WIDTH;
}

static void GAME_UpdateHorizon() {
    for (uint8_t i = horizon.x; i < horizon.width; i++) {
        // create some space between trex and horizon
        if (trex_state == RUNNING &&
            (i >= trex.x + TREX_STANDING_CLEARENCE_MIN) &&
            (i < trex.x + TREX_STANDING_CLEARENCE_MAX))
            continue;
        if (trex_state == RUNNING &&
            (i >= trex.x + TREX_STANDING_CLEARENCE_MIN) &&
            (i < trex.x + TREX_STANDING_CLEARENCE_MAX))
            continue;
        if (trex_state == JUMPING &&
            (i >= trex.x + TREX_STANDING_CLEARENCE_MIN) &&
            (i < trex.x + TREX_STANDING_CLEARENCE_MAX) &&
            (trex.y > trex.height + 1))
            continue;
        if (trex_state == DUCKING &&
            (i >= trex.x + TREX_DUCKING_CLEARENCE_MIN) &&
            (i < trex.x + TREX_DUCKING_CLEARENCE_MAX))
            continue;

        int8_t bump1_xx = my_floor(horizon.bump1_x);
        int8_t bump2_xx = my_floor(horizon.bump2_x);

        if (i >= bump1_xx && i < bump1_xx + horizon.bump1_width)
            // draw first bump
            FB_SetPixel(i, horizon.y);
        else if (i >= bump2_xx && i < bump2_xx + horizon.bump2_width)
            // draw second bump
            FB_SetPixel(i, horizon.y);
        else
            // draw horizon line
            FB_SetPixel(i, horizon.y + 1);
    }

    // move bumps
    if (horizon.bump1_x - game_speed > 0) {
        horizon.bump1_x -= game_speed;
    } else {
        horizon.bump1_x = horizon.width;
    }

    if (horizon.bump2_x - game_speed > 0) {
        horizon.bump2_x -= game_speed;
    } else {
        horizon.bump2_x = horizon.width;
    }
}

static void GAME_InitPrerodactyl(game_object_t* pterodactyl) {
    pterodactyl->sprite = pterodactyl1;
    pterodactyl->x = 0 - PTERODACTYL_WIDTH;
    pterodactyl->y = 0 - PTERODACTYL_HEIGHT;
    pterodactyl->width = PTERODACTYL_WIDTH;
    pterodactyl->height = PTERODACTYL_HEIGHT;
    pterodactyl->visible = true;
}

static void GAME_CreatePterodactyl(game_object_t* pterodactyl) {
    pterodactyl->x = WIDTH;
    uint8_t temp = rand() % PTERODACTYL_FLYING_HEIGHTS_CNT;
    pterodactyl->y = pterodactyl_flying_heights[temp];
    pterodactyl->width = PTERODACTYL_WIDTH;
    pterodactyl->height = PTERODACTYL_HEIGHT;
    pterodactyl->sprite = pterodactyl1;
    pterodactyl->visible = true;
}

static void GAME_UpdatePterodactyl(game_object_t* pterodactyl) {
    static unsigned int flapping_counter = 0;

    if (!pterodactyl->visible)
        return;

    if (++flapping_counter >= PTERODACTYL_WING_SWAP) {
        flapping_counter = 0;

        if (pterodactyl->sprite == pterodactyl1) {
            pterodactyl->sprite = pterodactyl2;
        } else {
            pterodactyl->sprite = pterodactyl1;
        }
    }
    FB_DrawGameObject(*pterodactyl);

    // move to the left in small steps
    if (pterodactyl->x + pterodactyl->width > 0) {
        pterodactyl->x -= game_speed;
    } else {
        pterodactyl->visible = false;
    }
}

static void GAME_InitCactus(game_object_t* cactus) { cactus->visible = false; }

static void GAME_CreateCactus(game_object_t* cactus) {
    uint8_t cactus_type = rand() % CACTUS_NUMBER_OF_SPECIES;

    cactus->x = WIDTH;
    cactus->y = HEIGHT - CACTUS_PADDING_BOTTOM;
    cactus->visible = true;

    switch (cactus_type) {
    case 0:
        cactus->sprite = cactus1;
        cactus->width = CACTUS1_WIDTH;
        cactus->height = CACTUS1_HEIGHT;
        break;
    case 1:
        cactus->sprite = cactus2;
        cactus->width = CACTUS2_WIDTH;
        cactus->height = CACTUS2_HEIGHT;
        break;
    case 2:
        cactus->sprite = cactus3;
        cactus->width = CACTUS3_WIDTH;
        cactus->height = CACTUS3_HEIGHT;
        break;
    case 3:
    default:
        cactus->sprite = cactus4;
        cactus->width = CACTUS4_WIDTH;
        cactus->height = CACTUS4_HEIGHT;
        break;
    }

    cactus->y -= cactus->height;
}

static void GAME_UpdateCactus(game_object_t* cactus) {
    if (!cactus->visible)
        return;

    FB_DrawGameObject(*cactus);

    // move to the left in small steps
    if (cactus->x + cactus->width > 0) {
        cactus->x -= game_speed;
    } else {
        cactus->visible = false;
    }
}

static uint8_t GAME_CountVisibleCacti(game_object_t cactus[]) {
    uint8_t cactuses_on_screen = 0;
    for (uint8_t i = 0; i < CACTUS_MAX_COUNT; i++) {
        if (cactus[i].visible)
            cactuses_on_screen++;
    }
    return cactuses_on_screen;
}

static void GAME_InitTrex() {
    trex.x = TREX_PADDING_RIGHT;
    trex.y = HEIGHT - TREX_STANDING_HEIGHT - 1;
    trex.width = TREX_STANDING_WIDTH;
    trex.height = TREX_STANDING_HEIGHT;
    trex.sprite = trex_running1;
    trex.visible = true;
}

static void GAME_UpdateRunningTrex() {
    static uint16_t running_counter = 0;

    trex.y = HEIGHT - TREX_STANDING_HEIGHT - 1;
    trex.width = TREX_STANDING_WIDTH;
    trex.height = TREX_STANDING_HEIGHT;

    if (++running_counter >= TREX_RUNNING_SPEED) {
        running_counter = 0;
        if (trex.sprite == trex_running1) {
            trex.sprite = trex_running2;
        } else {
            trex.sprite = trex_running1;
        }
    }
}

static void GAME_UpdateDuckingTrex() {
    static uint16_t running_counter = 0;

    trex.y = HEIGHT - TREX_DUCKING_HEIGHT - 1;
    trex.width = TREX_DUCKING_WIDTH;
    trex.height = TREX_DUCKING_HEIGHT;

    if (++running_counter >= TREX_RUNNING_SPEED) {
        running_counter = 0;
        if (trex.sprite == trex_ducking1) {
            trex.sprite = trex_ducking2;
        } else {
            trex.sprite = trex_ducking1;
        }
    }
}

static void GAME_UpdateJumpingTrex() {
    static uint8_t jump_max_y_reached = 0;

    trex.width = TREX_STANDING_WIDTH;
    trex.height = TREX_STANDING_HEIGHT;
    trex.sprite = trex_standing_init;

    // jump up
    if (!jump_max_y_reached && trex.y >= (HEIGHT - TREX_MAX_JUMP_HEIGHT)) {
        trex.y -= JUMPING_SPEED;
    } else
        jump_max_y_reached = 1;

    // let gravity do the landing
    if (jump_max_y_reached && trex.y <= (HEIGHT - TREX_STANDING_HEIGHT - 2)) {
        trex.y += GAME_GRAVITY;
    }

    // next state running
    if (jump_max_y_reached && trex.y > (HEIGHT - TREX_STANDING_HEIGHT - 2)) {
        trex.y = HEIGHT - TREX_STANDING_HEIGHT - 1;
        if (button_state & (1 << DUCK_BUTTON_BIT)) {
            trex_state = DUCKING;
            // preload ducking sprite
            trex.sprite = trex_ducking1;
        } else {
            trex_state = RUNNING;
            // preload running sprite
            trex.sprite = trex_running1;
        }
        jump_max_y_reached = 0;
    }
}

static void GAME_UpdateTrex() {
    switch (trex_state) {
    case RUNNING:
        GAME_UpdateRunningTrex();
        break;
    case DUCKING:
        GAME_UpdateDuckingTrex();
        break;
    case JUMPING:
        GAME_UpdateJumpingTrex();
        break;
    case CRASHED:
        break;
    }

    if (FB_DrawGameObject(trex)) {
        trex_state = CRASHED;
    }
}

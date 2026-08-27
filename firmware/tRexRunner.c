//------------------------------------------------------------------------------
// Includes
//------------------------------------------------------------------------------

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
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
#define LEVEL_UP_POINTS         100

#define HI_STR_X   (WIDTH - (DIGIT_WIDTH * 13))
#define HI_STR_Y   1
#define HI_SCORE_X (WIDTH - (DIGIT_WIDTH * 11))
#define HI_SCORE_Y HI_STR_Y
#define SCORE_X    (WIDTH - (DIGIT_WIDTH * 5) - 1)
#define SCORE_Y    HI_STR_Y

#define GAME_OVER_X ((WIDTH / 2) - (GAME_OVER_SPLASH_WIDTH / 2))
#define GAME_OVER_Y 10

#define DEBOUNCE_INTERVAL_MS  50
#define OLED_STARTUP_DELAY_MS 100

#define JUMP_BUTTON_BIT          0
#define JUMP_BUTTON_GPIO         PC3
#define IS_JUMP_BUTTON_PRESSED() (button_state & (1 << JUMP_BUTTON_BIT))

#define DUCK_BUTTON_BIT          1
#define DUCK_BUTTON_GPIO         PC4
#define IS_DUCK_BUTTON_PRESSED() (button_state & (1 << DUCK_BUTTON_BIT))

#define AUTOCUTOFF_GPIO      PD3
#define CHARGE_COMPLETE_GPIO PD5
#define USB_PWR_GPIO         PD2

#define BOOT_WINDOW_MS      1500
#define STARTUP_INTERVAL_MS 1000
#define FACTORY_TRIGGER_MS  5000

#define INACTIVITY_PERIOD_MS 30000

#define MIN_BATTERY_VOLTAGE           3600   // mV
#define BATTERY_MONITOR_PERIOD_MS     30000
#define LOW_BATTERY_ALERT_DURATION_MS 2500

#define HI_SCORE_FLASH_ADDR 0x08003FC0

#define PAGE_HEIGHT 8

#define CEIL(val) ((int) (val) + ((val) > (int) (val)))
#define MAX_DIGIT_DIVISOR                                                      \
    10000                 // Supports printing up to 5-digit numbers (0-99999)
#define DECIMAL_BASE 10   // Base-10 numerical division step

#define CHARGE_CHECK_INTERVAL_MS 10

//------------------------------------------------------------------------------
// Type definitions
//------------------------------------------------------------------------------

typedef enum system_state_e {
    SYS_STARTUP,
    SYS_BATTERY_CHARGING,
    SYS_FACTORY_RESET,
    SYS_WAIT_GAME_START,
    SYS_RUNNING_GAME,
    SYS_GAME_OVER,
    SYS_SHUTDOWN
} system_state_t;

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
static void BUTTONS_MonitorButtons(uint32_t now_ms);

static uint32_t FLASH_Read_u32(uint32_t address);
static void FLASH_Write_u32(uint32_t address, uint32_t val);

static void POWER_MANAGER_init();
static void POWER_MANAGER_turnOff();
static uint16_t POWER_MANAGER_ReadBatteryVoltage();

static void FB_Clear();
static uint8_t FB_DrawImage(int16_t pos_x, int16_t pos_y, const uint8_t* image,
                            uint8_t width, uint8_t height);
static void FB_DrawUnsignedValue(int16_t pos_x, int16_t pos_y, uint32_t value);
static uint8_t FB_DrawGameObject(game_object_t game_object);
static void FB_SetPixel(uint8_t pos_x, uint8_t pos_y);
static void FB_InvertColor();
static void FB_DrawRectangle(uint8_t pos_x, uint8_t pos_y, uint8_t width,
                             uint8_t height, uint8_t fill);
static void FB_DrawProgressBar(uint32_t current_ms, uint32_t target_ms);
static void FB_ShowBatteryStatus(uint8_t pos_x, uint8_t pos_y,
                                 uint8_t progress);

static void SYS_HardwareSetup();
static system_state_t SYS_ProcessStartup(uint32_t now_ms);
static system_state_t SYS_ProcessBatteryCharging(uint32_t now_ms);
static system_state_t SYS_ProcessFactoryReset(uint32_t now_ms);
static system_state_t SYS_ProcessGame(uint32_t now_ms);
static system_state_t SYS_ProcessGameOver();
static system_state_t SYS_MonitorInactivity(uint32_t now_ms);
static system_state_t SYS_MonitorBattery(uint32_t now_ms);

static void GAME_Init();
static void GAME_ShowScore();
static void GAME_HandleTrexState();
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

volatile uint32_t system_millis = 0;
static uint8_t frame_buffer[WIDTH * HEIGHT / PAGE_HEIGHT];
static system_state_t current_state = SYS_STARTUP;
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
// lookup table for pterodactyl flying heights
static const uint8_t pterodactyl_flying_heights[] = {
    PTERODACTYL_MIN_FLY_HEIGHT, PTERODACTYL_MID_FLY_HEIGHT,
    PTERODACTYL_MAX_FLY_HEIGHT};

//------------------------------------------------------------------------------
// Main
//------------------------------------------------------------------------------

int main() {
    SYS_HardwareSetup();

    uint32_t last_render_time = 0;
    bool is_render_time = false;
    bool was_any_button_pressed = true;

    while (1) {
        uint32_t current_time = system_millis;

        if ((current_time - last_render_time) >= RENDER_PERIOD) {
            last_render_time = current_time;
            is_render_time = true;
        }

        switch (current_state) {
        case SYS_STARTUP:
            current_state = SYS_ProcessStartup(current_time);
            break;
        case SYS_BATTERY_CHARGING:
            current_state = SYS_ProcessBatteryCharging(current_time);
            break;
        case SYS_FACTORY_RESET:
            current_state = SYS_ProcessFactoryReset(current_time);
            break;
        case SYS_WAIT_GAME_START:
            // Handle button presses only on the rising edge.
            // This prevents accidental game starts if a button remains
            // pressed after startup or after a game over.
            bool is_any_button_pressed =
                (IS_JUMP_BUTTON_PRESSED() || IS_DUCK_BUTTON_PRESSED()) != 0;
            bool is_button_just_pressed =
                (is_any_button_pressed && !was_any_button_pressed) != 0;
            was_any_button_pressed = is_any_button_pressed;
            if (is_button_just_pressed) {
                GAME_Init();
                current_state = SYS_RUNNING_GAME;
            }
            break;
        case SYS_RUNNING_GAME:
            if (is_render_time) {
                current_state = SYS_ProcessGame(current_time);
            }
            break;
        case SYS_GAME_OVER:
            current_state = SYS_ProcessGameOver();
            break;
        case SYS_SHUTDOWN:
            FB_Clear();
            POWER_MANAGER_turnOff();
            break;
        }
        // Render the prepared scene
        if (is_render_time) {
            is_render_time = false;
            SSD1306_Display(frame_buffer);
        }
        // Periodical checks
        if (current_state != SYS_BATTERY_CHARGING) {
            BUTTONS_MonitorButtons(current_time);
            current_state = SYS_MonitorInactivity(current_time);
            current_state = SYS_MonitorBattery(current_time);
        }
    }

    return 0;
}

//------------------------------------------------------------------------------
// Function definitions
//------------------------------------------------------------------------------

static void SYS_HardwareSetup() {
    SystemInit();
    funGpioInitAll();   // Enable GPIOs
    BUTTONS_Init();
    TIMER_Init();
    POWER_MANAGER_init();
    // give OLED some more time
    Delay_Ms(OLED_STARTUP_DELAY_MS);
    SSD1306_Init();
}

static system_state_t SYS_ProcessStartup(uint32_t now_ms) {
    static uint32_t boot_window_ms = 0;
    static uint32_t hold_ms = 0;
    system_state_t next_state = SYS_STARTUP;
    // Initialize boot window timestamp on the very first frame
    if (boot_window_ms == 0) {
        boot_window_ms = now_ms;
    }
    uint32_t progress = 0;
    if (!funDigitalRead(USB_PWR_GPIO)) {
        next_state = SYS_BATTERY_CHARGING;
    } else if (IS_JUMP_BUTTON_PRESSED() && IS_DUCK_BUTTON_PRESSED()) {
        // Start holding if we weren't already holding
        if (hold_ms == 0) {
            hold_ms = now_ms;
        }
        // Reset boot window start time while buttons are actively held
        boot_window_ms = now_ms;
        progress = now_ms - hold_ms;
        FB_DrawProgressBar(progress, STARTUP_INTERVAL_MS);
        // Check if hold duration met the requirement
        if (progress >= STARTUP_INTERVAL_MS) {
            GAME_Init();
            next_state = SYS_FACTORY_RESET;
        }
    } else {
        FB_DrawProgressBar(progress, STARTUP_INTERVAL_MS);
        // Buttons were released after being held
        // Or the boot window has expired
        if ((hold_ms > 0) || ((now_ms - boot_window_ms) >= BOOT_WINDOW_MS)) {
            next_state = SYS_SHUTDOWN;
        }
    }
    // Cleanup: reset static variables to default values
    if (next_state != SYS_STARTUP) {
        hold_ms = 0;
        boot_window_ms = 0;
    }
    return next_state;
}

static system_state_t SYS_ProcessBatteryCharging(uint32_t now_ms) {
    static uint32_t last_check_time_ms = 0;
    static uint32_t last_icon_update_time_ms = 0;
    static uint16_t battery_icon_x_pos = (WIDTH - BATTERY_ICON_WITH) / 2;
    static uint8_t progress = 0;
    // Initialize static variables for the first time
    if (last_check_time_ms == 0) {
        last_check_time_ms = now_ms;
    }
    if (last_icon_update_time_ms == 0) {
        last_icon_update_time_ms = now_ms;
    }
    // Check is the USB still connected. If not, turn off the device
    if (funDigitalRead(USB_PWR_GPIO)) {
        return SYS_SHUTDOWN;
    }
    // Peridically check the battery charge status
    if ((now_ms - last_check_time_ms) > CHARGE_CHECK_INTERVAL_MS) {
        last_check_time_ms = now_ms;
        if (!funDigitalRead(CHARGE_COMPLETE_GPIO)) {
            // Charging
            ++progress;
        } else {
            // Charging complete
            progress = UINT8_MAX;
        }
        FB_ShowBatteryStatus(battery_icon_x_pos,
                             (HEIGHT - BATTERY_ICON_HEIGHT) / 2, progress);
    }
    // Periodically update the battery icon position to prevent OLED burn in
    if ((now_ms - last_icon_update_time_ms) >=
        UINT8_MAX * CHARGE_CHECK_INTERVAL_MS) {
        last_icon_update_time_ms = now_ms;
        battery_icon_x_pos += BATTERY_ICON_X_POS_INCREMENT;
        if (battery_icon_x_pos >= WIDTH - BATTERY_ICON_WITH) {
            battery_icon_x_pos = 0;
        }
    }
    // Keep this state indefinitely
    return SYS_BATTERY_CHARGING;
}

static system_state_t SYS_ProcessFactoryReset(uint32_t now_ms) {
    static uint32_t factory_reset_time_ms = 0;
    system_state_t next_state = SYS_FACTORY_RESET;
    if (IS_JUMP_BUTTON_PRESSED() && IS_DUCK_BUTTON_PRESSED()) {
        // Initialize factory reset timestamp on the very first frame
        if (factory_reset_time_ms == 0) {
            factory_reset_time_ms = now_ms;
        }
        // Check if factory reset trigger time has elapsed
        if ((now_ms - factory_reset_time_ms) >= FACTORY_TRIGGER_MS) {
            high_score = 0;
            FLASH_Write_u32(HI_SCORE_FLASH_ADDR, high_score);
            GAME_Init();
            next_state = SYS_WAIT_GAME_START;
        }
    } else {
        // Buttons were released after being held, switch to next state
        if (factory_reset_time_ms > 0) {
            next_state = SYS_WAIT_GAME_START;
        }
    }
    // Cleanup: reset static variables to default values
    if (next_state != SYS_FACTORY_RESET) {
        factory_reset_time_ms = 0;
    }
    return next_state;
}

static system_state_t SYS_ProcessGame(uint32_t now_ms) {
    if (trex_state == CRASHED) {
        return SYS_GAME_OVER;
    }
    FB_Clear();
    GAME_HandleTrexState();
    GAME_ShowScore();
    // Create new obstacles
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
                    (rand() % obstacle_respawn_max_distance);
                obstacles[latest_cactus].x += (float) random_distance;
                // Respawn pterodatyl?
                if (random_distance >= show_pterodactyl) {
                    // Replace cactus with pterodactyl
                    obstacles[latest_cactus].visible = false;
                    GAME_CreatePterodactyl(&obstacles[CACTUS_MAX_COUNT]);
                    obstacles[PTERODACTYL].x += (float) random_distance;
                }
                latest_cactus++;
            }
        }
        if (latest_cactus == CACTUS_MAX_COUNT) {
            latest_cactus = 0;
        }
    }
    // Update cacti
    for (uint8_t i = 0; i < CACTUS_MAX_COUNT; i++) {
        GAME_UpdateCactus(&obstacles[i]);
    }
    // Update pterodactyl
    GAME_UpdatePterodactyl(&obstacles[PTERODACTYL]);
    // Update trex
    GAME_UpdateTrex();
    GAME_UpdateHorizon();
    if (inverted_mode) {
        FB_InvertColor();
    }
    // Speed up the game periodically
    static uint32_t game_speed_update_clock = 0;
    if ((now_ms - game_speed_update_clock) >= GAME_SCORE_INCREMENT) {
        game_speed_update_clock = now_ms;
        score++;
        GAME_AdjustDifficulty();
    }
    // Update score
    if ((score % INVERTED_MODE_THRESHOLD) == 0) {
        inverted_mode = !inverted_mode;
    }
    return SYS_RUNNING_GAME;
}

static system_state_t SYS_ProcessGameOver() {
    // If in inverterd mode, switch back to original (dark) layout
    if (inverted_mode) {
        FB_InvertColor();
    }
    // Add the "game over" image to the frame buffer
    FB_DrawImage(GAME_OVER_X, GAME_OVER_Y, game_over_splash,
                 GAME_OVER_SPLASH_WIDTH, GAME_OVER_SPLASH_HEIGHT);
    // Restore the inverted (light) mode if needed
    if (inverted_mode) {
        FB_InvertColor();   // invert back
    }
    // Process high score
    if (score > high_score) {
        high_score = score;
        FLASH_Write_u32(HI_SCORE_FLASH_ADDR, high_score);
    }
    return SYS_WAIT_GAME_START;
}

static system_state_t SYS_MonitorInactivity(uint32_t now_ms) {
    static uint32_t last_button_press_time = 0;
    if (button_state) {
        last_button_press_time = now_ms;
    }
    if ((now_ms - last_button_press_time) >= INACTIVITY_PERIOD_MS) {
        return SYS_SHUTDOWN;
    }
    return current_state;
}

static system_state_t SYS_MonitorBattery(uint32_t now_ms) {
    static uint32_t battery_check_time = 0;
    if ((now_ms - battery_check_time) >= BATTERY_MONITOR_PERIOD_MS) {
        battery_check_time = now_ms;
        uint16_t battery_voltage =
            POWER_MANAGER_ReadBatteryVoltage();   // read battery status
        if (battery_voltage <= MIN_BATTERY_VOLTAGE) {
            FB_ShowBatteryStatus((WIDTH - BATTERY_ICON_WITH) / 2,
                                 (HEIGHT - BATTERY_ICON_HEIGHT) / 2, 0);
            SSD1306_Display(frame_buffer);
            Delay_Ms(LOW_BATTERY_ALERT_DURATION_MS);
            return SYS_SHUTDOWN;
        }
    }
    return current_state;
}

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
        system_millis++;
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
        while (1) {
        }
    }

    uint32_t* ptr = (uint32_t*) address;

    // Erase Page
    FLASH->CTLR = CR_PAGE_ER;
    FLASH->ADDR = (intptr_t) ptr;
    FLASH->CTLR = CR_STRT_Set | CR_PAGE_ER;
    while (FLASH->STATR & FLASH_STATR_BSY) {   // Takes about 3ms.
    }

    // Clear buffer and prep for flashing.
    FLASH->CTLR = CR_PAGE_PG;   // synonym of FTPG.
    FLASH->CTLR = CR_BUF_RST | CR_PAGE_PG;
    FLASH->ADDR = (intptr_t)
        ptr;   // This can actually happen about anywhere toward the end here.

    // Note: It takes about 6 clock cycles for this to finish.
    while (FLASH->STATR & FLASH_STATR_BSY) {   // No real need for this.
    }

    *ptr = val;                                       // Write to the memory
    FLASH->CTLR = CR_PAGE_PG | FLASH_CTLR_BUF_LOAD;   // Load the buffer.
    while (FLASH->STATR &
           FLASH_STATR_BSY) {   // Only needed if running from RAM.
    }

    // Actually write the flash out. (Takes about 3ms)
    FLASH->CTLR = CR_PAGE_PG | CR_STRT_Set;

    while (FLASH->STATR & FLASH_STATR_BSY) {
    }

    // Lock Flash
    FLASH->CTLR |= FLASH_CTLR_LOCK;
}

static void BUTTONS_Init() {
    funPinMode(JUMP_BUTTON_GPIO, GPIO_CFGLR_IN_PUPD);
    funDigitalWrite(JUMP_BUTTON_GPIO, FUN_HIGH);
    funPinMode(DUCK_BUTTON_GPIO, GPIO_CFGLR_IN_PUPD);
    funDigitalWrite(DUCK_BUTTON_GPIO, FUN_HIGH);
}

static void BUTTONS_MonitorButtons(uint32_t now_ms) {
    static uint32_t jump_btn_last_stable_change_ms = 0;
    static uint8_t jump_btn_raw_state = 1;   // Start high (not pressed)
    static uint32_t duck_btn_last_stable_change_ms = 0;
    static uint8_t duck_btn_raw_state = 1;   // Start high (not pressed)
    // JUMP button
    uint8_t current_jump_raw = funDigitalRead(JUMP_BUTTON_GPIO);
    if (current_jump_raw != jump_btn_raw_state) {
        jump_btn_last_stable_change_ms = now_ms;
        jump_btn_raw_state = current_jump_raw;
    } else if ((now_ms - jump_btn_last_stable_change_ms) >=
               DEBOUNCE_INTERVAL_MS) {
        // State has been stable for the debounce interval, update button state
        if (!current_jump_raw) {
            button_state |= (1 << JUMP_BUTTON_BIT);
        } else {
            button_state &= ~(1 << JUMP_BUTTON_BIT);
        }
    }
    // DUCK button
    uint8_t current_duck_raw = funDigitalRead(DUCK_BUTTON_GPIO);
    if (current_duck_raw != duck_btn_raw_state) {
        duck_btn_last_stable_change_ms = now_ms;
        duck_btn_raw_state = current_duck_raw;
    } else if ((now_ms - duck_btn_last_stable_change_ms) >=
               DEBOUNCE_INTERVAL_MS) {
        // State has been stable for the debounce interval, update button state
        if (!current_duck_raw) {
            button_state |= (1 << DUCK_BUTTON_BIT);
        } else {
            button_state &= ~(1 << DUCK_BUTTON_BIT);
        }
    }
}

static void POWER_MANAGER_init() {
    // Initialize GPIO pins
    funPinMode(AUTOCUTOFF_GPIO, GPIO_CFGLR_OUT_10Mhz_PP);
    funDigitalWrite(AUTOCUTOFF_GPIO, FUN_HIGH);
    funPinMode(CHARGE_COMPLETE_GPIO, GPIO_CFGLR_IN_PUPD);
    funDigitalWrite(CHARGE_COMPLETE_GPIO, FUN_HIGH);
    funPinMode(USB_PWR_GPIO, GPIO_CFGLR_IN_PUPD);
    funDigitalWrite(USB_PWR_GPIO, FUN_HIGH);

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
    while (ADC1->CTLR2 & ADC_RSTCAL) {
    }

    // Calibrate
    ADC1->CTLR2 |= ADC_CAL;
    while (ADC1->CTLR2 & ADC_CAL) {
    }

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
    while (!(ADC1->STATR & ADC_EOC)) {
    }
    // get result
    // Voltage divider is returning half of the real voltage from the lipo
    // ADC has 10bit resolution and the voltage reference is Vdd = 3.3V
    float voltage = 3300.0 / 512 * ADC1->RDATAR;
    return (uint16_t) voltage;
}

static void FB_Clear() {
    memset(frame_buffer, 0, sizeof(uint8_t) * (WIDTH * HEIGHT / 8));
}

static uint8_t FB_DrawImage(int16_t pos_x, int16_t pos_y, const uint8_t* image,
                            uint8_t width, uint8_t height) {
    uint8_t collision = false;
    for (int16_t iter_y = pos_y; iter_y < (pos_y + height); iter_y++) {
        for (int16_t iter_x = pos_x; iter_x < (pos_x + width); iter_x++) {
            if ((iter_x >= WIDTH) || (iter_y >= HEIGHT)) {
                continue;
            }
            if (iter_y < 0 || iter_x < 0) {
                continue;
            }
            uint16_t buffer_index = (WIDTH * (iter_y / PAGE_HEIGHT)) + iter_x;
            uint8_t image_w = iter_x - pos_x;
            uint8_t image_h = iter_y - pos_y;
            uint16_t image_index = (width * (image_h / PAGE_HEIGHT)) + image_w;
            if ((image[image_index] >> (image_h % PAGE_HEIGHT) & 0x01) ==
                0x01) {
                if (frame_buffer[buffer_index] >> (iter_y % PAGE_HEIGHT) &
                    0x01) {
                    collision = true;
                }
                frame_buffer[buffer_index] |= (1 << (iter_y % PAGE_HEIGHT));
            }
        }
    }
    return collision;
}

static void FB_DrawUnsignedValue(int16_t pos_x, int16_t pos_y, uint32_t value) {
    int16_t temp_x_pos = pos_x;
    for (uint32_t dividend = MAX_DIGIT_DIVISOR; dividend > 0;
         dividend /= DECIMAL_BASE) {
        FB_DrawImage(
            temp_x_pos, pos_y,
            &digits[(size_t) ((value / dividend % DECIMAL_BASE) * DIGIT_WIDTH)],
            DIGIT_WIDTH, DIGIT_HEIGHT);
        temp_x_pos += DIGIT_WIDTH;
    }
}

static uint8_t FB_DrawGameObject(game_object_t game_object) {
    if (!game_object.visible) {
        return false;
    }
    return FB_DrawImage(CEIL(game_object.x), CEIL(game_object.y),
                        game_object.sprite, game_object.width,
                        game_object.height);
}

static void FB_SetPixel(uint8_t pos_x, uint8_t pos_y) {
    if (pos_x >= WIDTH || pos_y >= HEIGHT) {
        return;
    }

    uint32_t index = (WIDTH * (pos_y / PAGE_HEIGHT)) + pos_x;
    frame_buffer[index] |= (1 << (pos_y & (PAGE_HEIGHT - 1)));
}

static void FB_InvertColor() {
    for (size_t i = 0; i < (WIDTH * HEIGHT / PAGE_HEIGHT); i++) {
        frame_buffer[i] = ~frame_buffer[i];
    }
}

static void FB_DrawRectangle(uint8_t pos_x, uint8_t pos_y, uint8_t width,
                             uint8_t height, uint8_t fill) {
    if ((pos_x >= WIDTH) || (pos_y >= HEIGHT)) {
        return;
    }

    uint8_t max_x;
    uint8_t max_y;

    if ((pos_y + height) > HEIGHT) {
        max_y = HEIGHT;
    } else {
        max_y = pos_y + height;
    }

    if ((pos_x + width) > WIDTH) {
        max_x = WIDTH;
    } else {
        max_x = pos_x + width;
    }

    for (uint8_t i = pos_y; i < max_y; i++) {
        for (uint8_t j = pos_x; j < max_x; j++) {
            if (fill) {
                FB_SetPixel(j, i);
            } else {
                if (i == pos_y || i == (max_y - 1) || j == pos_x ||
                    j == (max_x - 1)) {
                    FB_SetPixel(j, i);
                }
            }
        }
    }
}

static void FB_DrawProgressBar(uint32_t current_ms, uint32_t target_ms) {
    FB_Clear();
    FB_DrawRectangle(PROGRESS_BAR_X, PROGRESS_BAR_Y, PROGRESS_BAR_WIDTH,
                     PROGRESS_BAR_HEIGHT, false);

    if (target_ms == 0) {
        return;
    }

    // Upcast temporarily to uint64_t to prevent overflow during cross
    // multiplication
    uint32_t filled_width =
        (uint32_t) (((uint64_t) current_ms * PROGRESS_BAR_WIDTH) / target_ms);

    if (filled_width > PROGRESS_BAR_WIDTH) {
        filled_width = PROGRESS_BAR_WIDTH;
    }

    if (filled_width > 0) {
        FB_DrawRectangle(PROGRESS_BAR_X, PROGRESS_BAR_Y, filled_width,
                         PROGRESS_BAR_HEIGHT, true);
    }
}

static void FB_ShowBatteryStatus(uint8_t pos_x, uint8_t pos_y,
                                 uint8_t progress) {
    FB_Clear();
    FB_DrawRectangle(pos_x + 2, pos_y + 0, BATTERY_ICON_WITH - 2,
                     BATTERY_ICON_HEIGHT, false);
    FB_DrawRectangle(pos_x + 0, pos_y + 4, 2, BATTERY_ICON_PROFRESS_BAR_HEIGHT,
                     true);
    FB_DrawRectangle(pos_x + 6, pos_y + 4,
                     progress * BATTERY_ICON_PROFRESS_BAR_WIDTH / UINT8_MAX,
                     BATTERY_ICON_PROFRESS_BAR_HEIGHT, true);
}

static void GAME_Init() {
    score = 0;
    high_score = FLASH_Read_u32(HI_SCORE_FLASH_ADDR);
    game_speed = GAME_INITIAL_SPEED;
    trex_state = RUNNING;
    latest_cactus = 0;   // index of the newest cactus in the array
    obstacle_respawn_base_distance = OBSTACLE_RESPAWN_BASE_DISTANCE;
    obstacle_respawn_max_distance = WIDTH - OBSTACLE_RESPAWN_BASE_DISTANCE;
    show_pterodactyl = SHOW_PTERODACTYL;
    inverted_mode = false;
    // Seed the Random Number Generator.
    // FIXME: consider other seed sources
    seed(system_millis);
    GAME_InitHorizon();
    GAME_InitTrex();
    for (uint8_t i = 0; i < CACTUS_MAX_COUNT; i++) {
        GAME_InitCactus(&obstacles[i]);
    }
    GAME_InitPrerodactyl(&obstacles[PTERODACTYL]);
    FB_Clear();
    GAME_UpdateHorizon();
    GAME_ShowScore();
    GAME_UpdateTrex();
}

static void GAME_ShowScore() {
    FB_DrawImage(HI_STR_X, HI_STR_Y, hi_score_str, HI_SCORE_STR_WIDTH,
                 HI_SCORE_STR_HEIGHT);
    FB_DrawUnsignedValue(HI_SCORE_X, HI_SCORE_Y, high_score);
    FB_DrawUnsignedValue(SCORE_X, SCORE_Y, score);
}

static void GAME_HandleTrexState() {
    // Update trex state based on button states
    if (IS_JUMP_BUTTON_PRESSED()) {
        trex_state = JUMPING;
    }
    if (IS_DUCK_BUTTON_PRESSED() && (trex_state != JUMPING)) {
        if (trex_state == RUNNING) {
            // Preload a ducking sprite
            trex.sprite = trex_ducking1;
        }
        trex_state = DUCKING;
    } else if (trex_state != JUMPING) {
        if (trex_state == DUCKING) {
            // Preload a running sprite
            trex.sprite = trex_running1;
        }
        trex_state = RUNNING;
    }
}

static void GAME_AdjustDifficulty() {
    if ((score % LEVEL_UP_POINTS) == 0) {
        game_speed += GAME_SPEED_DELTA;
        // Increase the distance between obstacles a little bit
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
        // Create some space between trex and horizon
        if (trex_state == RUNNING &&
            ((float) i >= trex.x + TREX_STANDING_CLEARENCE_MIN) &&
            ((float) i < trex.x + TREX_STANDING_CLEARENCE_MAX)) {
            continue;
        }
        if (trex_state == RUNNING &&
            ((float) i >= trex.x + TREX_STANDING_CLEARENCE_MIN) &&
            ((float) i < trex.x + TREX_STANDING_CLEARENCE_MAX)) {
            continue;
        }
        if (trex_state == JUMPING &&
            ((float) i >= trex.x + TREX_STANDING_CLEARENCE_MIN) &&
            ((float) i < trex.x + TREX_STANDING_CLEARENCE_MAX) &&
            (trex.y > (float) trex.height + 1)) {
            continue;
        }
        if (trex_state == DUCKING &&
            ((float) i >= trex.x + TREX_DUCKING_CLEARENCE_MIN) &&
            ((float) i < trex.x + TREX_DUCKING_CLEARENCE_MAX)) {
            continue;
        }
        int8_t bump1_xx = CEIL(horizon.bump1_x);
        int8_t bump2_xx = CEIL(horizon.bump2_x);
        if ((i >= bump1_xx && i < bump1_xx + horizon.bump1_width) ||
            (i >= bump2_xx &&
             i < bump2_xx + horizon.bump2_width)) {   // Draw bumps
            FB_SetPixel(i, horizon.y);
        } else {   // Draw horizon line
            FB_SetPixel(i, horizon.y + 1);
        }
    }
    // Move bumps
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
    if (!pterodactyl->visible) {
        return;
    }
    if (++flapping_counter >= PTERODACTYL_WING_SWAP) {
        flapping_counter = 0;

        if (pterodactyl->sprite == pterodactyl1) {
            pterodactyl->sprite = pterodactyl2;
        } else {
            pterodactyl->sprite = pterodactyl1;
        }
    }
    FB_DrawGameObject(*pterodactyl);
    // Move to the left in small steps
    if (pterodactyl->x + (float) pterodactyl->width > 0) {
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

    cactus->y -= (float) cactus->height;
}

static void GAME_UpdateCactus(game_object_t* cactus) {
    if (!cactus->visible) {
        return;
    }
    FB_DrawGameObject(*cactus);
    // Move to the left in small steps
    if (cactus->x + (float) cactus->width > 0) {
        cactus->x -= game_speed;
    } else {
        cactus->visible = false;
    }
}

static uint8_t GAME_CountVisibleCacti(game_object_t cactus[]) {
    uint8_t cactuses_on_screen = 0;
    for (uint8_t i = 0; i < CACTUS_MAX_COUNT; i++) {
        if (cactus[i].visible) {
            cactuses_on_screen++;
        }
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
    // Jump up
    if (!jump_max_y_reached && trex.y >= (HEIGHT - TREX_MAX_JUMP_HEIGHT)) {
        trex.y -= JUMPING_SPEED;
    } else {
        jump_max_y_reached = 1;
    }
    // Let gravity do the landing
    if (jump_max_y_reached && trex.y <= (HEIGHT - TREX_STANDING_HEIGHT - 2)) {
        trex.y += GAME_GRAVITY;
    }
    // Next state running
    if (jump_max_y_reached && trex.y > (HEIGHT - TREX_STANDING_HEIGHT - 2)) {
        trex.y = HEIGHT - TREX_STANDING_HEIGHT - 1;
        if (button_state & (1 << DUCK_BUTTON_BIT)) {
            trex_state = DUCKING;
            // Preload ducking sprite
            trex.sprite = trex_ducking1;
        } else {
            trex_state = RUNNING;
            // Preload running sprite
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

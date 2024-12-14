/*
 * tRexRunner.c
 *
 *  Created on: Dec 14, 2024
 *      Author: Daniel Knezevic
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/i2c.h"

#include "tRexRunner.h"
#include "ssd1306.h"

static uint8_t frame_buffer[WIDTH * HEIGHT / 8];

volatile uint16_t global_clock = 0;
volatile uint8_t lb_debounce_clock = 0;
volatile uint8_t rb_debounce_clock = 0;
volatile uint16_t game_speed_update_clock = 0;
volatile uint16_t inactivity_clock = 0;

static float game_speed = GAME_INITIAL_SPEED;
static uint32_t high_score = 0;
static uint32_t score = 0;

static uint_fast32_t button_state = 0;
#define is_left_button_pressed()      (button_state & (1 << LEFT_BUTTON_GPIO))
#define is_right_button_pressed()     (button_state & (1 << RIGHT_BUTTON_GPIO))

static trex_states_t trex_state = RUNNING;

static horizon_t horizon;
static game_object_t trex;
static game_object_t obstacles[CACTUS_MAX_COUNT + 1]; // last element is pterodactyl

static uint8_t latest_cactus = 0; // index of the newest cactus in the array

static uint16_t obstacle_respawn_base_distance = OBSTACLE_RESPAWN_BASE_DISTANCE;
// make sure WIDTH > OBSTACLE_RESPAWN_BASE_DISTANCE
static uint16_t obstacle_respawn_max_distance = WIDTH - OBSTACLE_RESPAWN_BASE_DISTANCE;
static uint16_t show_pterodactyl = SHOW_PTERODACTYL;

static bool inverted_mode = false;

// lookup table for pterodactyl flying heights
static const uint8_t pterodactyl_flying_heights[] = {
        PTERODACTYL_MIN_FLY_HEIGHT,
        PTERODACTYL_MID_FLY_HEIGHT,
        PTERODACTYL_MAX_FLY_HEIGHT
};

bool repeating_timer_callback(struct repeating_timer *t) {
    global_clock++;
    lb_debounce_clock++;
    rb_debounce_clock++;
    game_speed_update_clock++;
    inactivity_clock++;
    return true;
}

void BUTTONS_Init()
{
    gpio_init(LEFT_BUTTON_GPIO);
    gpio_set_dir(LEFT_BUTTON_GPIO, GPIO_IN);  // configure LEFT_BUTTON_IO as input
    gpio_pull_up(LEFT_BUTTON_GPIO);  // pull up LEFT_BUTTON_IO

    gpio_init(RIGHT_BUTTON_GPIO);
    gpio_set_dir(RIGHT_BUTTON_GPIO, GPIO_IN);  // configure LEFT_BUTTON_IO as input
    gpio_pull_up(RIGHT_BUTTON_GPIO);  // pull up LEFT_BUTTON_IO
}

void BUTTONS_MonitorButtons()
{
    if (lb_debounce_clock >= DEBOUNCE_INTERVAL)
    {
        lb_debounce_clock = 0;
        if (!gpio_get(LEFT_BUTTON_GPIO))
            button_state |= (1 << LEFT_BUTTON_GPIO);
        else
            button_state &= ~(1 << LEFT_BUTTON_GPIO);
    } else if ((button_state & (1 << LEFT_BUTTON_GPIO)) == !gpio_get(LEFT_BUTTON_GPIO))
    {
        lb_debounce_clock = 0;
    }

    if (rb_debounce_clock >= DEBOUNCE_INTERVAL)
    {
        rb_debounce_clock = 0;
        if (!gpio_get(RIGHT_BUTTON_GPIO))
            button_state |= (1 << RIGHT_BUTTON_GPIO);
        else
            button_state &= ~(1 << RIGHT_BUTTON_GPIO);
    } else if ((button_state & (1 << RIGHT_BUTTON_GPIO)) == !gpio_get(RIGHT_BUTTON_GPIO))
    {
        rb_debounce_clock = 0;
    }
}

void FB_Clear()
{
    memset(frame_buffer, 0, sizeof(uint8_t) * (WIDTH * HEIGHT / 8));
}

bool FB_DrawImage(int16_t x, int16_t y, const uint8_t* image, uint8_t width, uint8_t height)
{
    uint8_t collision = false;
    for(int16_t h = y; h < (y + height); h++)
    {
        for(int16_t w = x; w < (x + width); w++)
        {
            if((w >= WIDTH) || (h >= HEIGHT)) continue;
            if( h < 0 || w < 0 ) continue;
            uint16_t buffer_index = WIDTH * (h / 8) + w;
            uint8_t image_w = w - x;
            uint8_t image_h = h - y;
            uint16_t image_index = width * (image_h / 8) + image_w;
            if((image[image_index] >> (image_h % 8) & 0x01) == 0x01)
            {
                if(frame_buffer[buffer_index] >> (h % 8) & 0x01)
                    collision = true;
                frame_buffer[buffer_index]|=(1 << (h % 8));
            }
        }
    }
    return collision;
}

void FB_DrawUnsignedValue(int16_t x, int16_t y, uint32_t value)
{
    int16_t xx = x;
    for (uint32_t dividend = 10000; dividend > 0; dividend /= 10)
    {
        FB_DrawImage(xx, y, &digits[(value / dividend % 10) * DIGIT_WIDTH],
                DIGIT_WIDTH, DIGIT_HEIGHT);
        xx += DIGIT_WIDTH;
    }
}

bool FB_DrawGameObject(game_object_t game_object)
{
    if(!game_object.visible)
        return false;
    return FB_DrawImage(ceil(game_object.x), ceil(game_object.y),
            game_object.sprite, game_object.width, game_object.height);
}

void FB_SetPixel(uint8_t x, uint8_t y)
{
    if((x >= WIDTH) || (y >= HEIGHT)) return;
    uint16_t buffer_index = WIDTH * (y / 8) + x;
    frame_buffer[buffer_index] |= (1 << (y % 8));
}

void FB_InvertColor()
{
    for(uint16_t i = 0; i < (WIDTH * HEIGHT / 8); i++)
    {
        frame_buffer[i] = ~frame_buffer[i];
    }
}

void FB_DrawRectangle(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t fill)
{
    if((x >= WIDTH) || (y >= HEIGHT)) return;

    uint8_t a, b;

    if((y + height) > HEIGHT)
    {
        a = HEIGHT;
    }
    else
    {
        a = y + height;
    }

    if((x + width) > WIDTH)
     {
         b = WIDTH;
     }
     else
     {
         b = x + width;
     }

    for(uint8_t i = y; i < a; i++)
    {
        for(uint8_t j = x; j < b; j++)
        {
            if(fill)
            {
                FB_SetPixel(j, i);
            } else
            {
                if(i == y || i == (a - 1) || j == x || j == (b - 1))
                {
                    FB_SetPixel(j, i);
                }
            }
        }
    }
}

void GAME_InitHorizon()
{
    horizon.x = 0;
    horizon.y = HEIGHT - HORIZON_LINE_HEIGHT;
    horizon.width = HORIZON_LINE_WIDTH;
    horizon.height = HORIZON_LINE_HEIGHT;
    horizon.bump1_x = HORIZON_LINE_BUMP1_X;
    horizon.bump1_width = HORIZON_LINE_BUMP1_WIDTH;
    horizon.bump2_x = HORIZON_LINE_BUMP2_X;
    horizon.bump2_width = HORIZON_LINE_BUMP2_WIDTH;
}

void GAME_UpdateHorizon()
{
    for(uint8_t i = horizon.x; i < horizon.width; i++)
    {
        // create some space between trex and horizon
        if(trex_state == RUNNING &&
                (i >= trex.x + TREX_STANDING_CLEARENCE_MIN) &&
                (i < trex.x + TREX_STANDING_CLEARENCE_MAX))
            continue;
        if(trex_state == RUNNING &&
                (i >= trex.x + TREX_STANDING_CLEARENCE_MIN) &&
                (i < trex.x + TREX_STANDING_CLEARENCE_MAX))
            continue;
        if(trex_state == JUMPING &&
                (i >= trex.x + TREX_STANDING_CLEARENCE_MIN) &&
                (i < trex.x + TREX_STANDING_CLEARENCE_MAX) &&
                (trex.y > trex.height + 1))
            continue;
        if(trex_state == DUCKING &&
                (i >= trex.x + TREX_DUCKING_CLEARENCE_MIN) &&
                (i < trex.x + TREX_DUCKING_CLEARENCE_MAX))
            continue;

        int8_t bump1_xx = ceil(horizon.bump1_x);
        int8_t bump2_xx = ceil(horizon.bump2_x);

        if(i >= bump1_xx && i < bump1_xx + horizon.bump1_width)
            // draw first bump
            FB_SetPixel(i, horizon.y);
        else if(i >= bump2_xx && i < bump2_xx + horizon.bump2_width)
             // draw second bump
            FB_SetPixel(i, horizon.y);
        else
            // draw horizon line
            FB_SetPixel(i, horizon.y + 1);
    }

    // move bumps
    if (horizon.bump1_x - game_speed > 0)
    {
        horizon.bump1_x -= game_speed;
    }
    else
    {
        horizon.bump1_x = horizon.width;
    }

    if (horizon.bump2_x - game_speed > 0)
    {
        horizon.bump2_x -= game_speed;
    }
    else
    {
        horizon.bump2_x = horizon.width;
    }
}

void GAME_InitPrerodactyl(game_object_t *pterodactyl)
{
    pterodactyl->sprite = pterodactyl1;
    pterodactyl->x = 0 - PTERODACTYL_WIDTH;
    pterodactyl->y = 0 - PTERODACTYL_HEIGHT;
    pterodactyl->width = PTERODACTYL_WIDTH;
    pterodactyl->height = PTERODACTYL_HEIGHT;
    pterodactyl->visible = true;
}

void GAME_CreatePterodactyl(game_object_t *pterodactyl)
{
    pterodactyl->x = WIDTH;
    uint8_t temp = rand() % PTERODACTYL_FLYING_HEIGHTS_CNT;
    pterodactyl->y = pterodactyl_flying_heights[temp];
    pterodactyl->width = PTERODACTYL_WIDTH;
    pterodactyl->height = PTERODACTYL_HEIGHT;
    pterodactyl->sprite = pterodactyl1;
    pterodactyl->visible = true;
}

void GAME_UpdatePterodactyl(game_object_t *pterodactyl)
{
    static unsigned int flapping_counter = 0;

    if(!pterodactyl->visible)
        return;

    if (++flapping_counter >= PTERODACTYL_WING_SWAP)
    {
        flapping_counter = 0;

        if (pterodactyl->sprite == pterodactyl1)
        {
            pterodactyl->sprite = pterodactyl2;
        } else
        {
            pterodactyl->sprite = pterodactyl1;
        }
    }
    FB_DrawGameObject(*pterodactyl);

    // move to the left in small steps
    if (pterodactyl->x + pterodactyl->width > 0)
    {
        pterodactyl->x -= game_speed;
    } else
    {
        pterodactyl->visible = false;
    }
}

void GAME_InitCactus(game_object_t *cactus)
{
    cactus->visible = false;
}

void GAME_CreateCactus(game_object_t *cactus)
{
    uint8_t cactus_type = rand() % CACTUS_NUMBER_OF_SPECIES;

    cactus->x = WIDTH;
    cactus->y = HEIGHT - CACTUS_PADDING_BOTTOM;
    cactus->visible = true;

    switch(cactus_type){
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

void GAME_UpdateCactus(game_object_t *cactus)
{
    if(!cactus->visible)
        return;

    FB_DrawGameObject(*cactus);

    // move to the left in small steps
    if (cactus->x + cactus->width > 0)
    {
        cactus->x -= game_speed;
    } else
    {
        cactus->visible = false;
    }
}

uint8_t GAME_CountVisibleCacti(game_object_t cactus[])
{
    uint8_t cacti_on_screen = 0;
    for(uint8_t i = 0; i < CACTUS_MAX_COUNT; i++)
    {
        if(cactus[i].visible)
            cacti_on_screen++;
    }
    return cacti_on_screen;
}

void GAME_InitTrex()
{
    trex.x = TREX_PADDING_RIGHT;
    trex.y = HEIGHT - TREX_STANDING_HEIGHT - 1;
    trex.width = TREX_STANDING_WIDTH;
    trex.height = TREX_STANDING_HEIGHT;
    trex.sprite = trex_running1;
    trex.visible = true;
}

void GAME_UpdateRunningTrex()
{
    static uint16_t running_counter = 0;

    trex.y = HEIGHT - TREX_STANDING_HEIGHT - 1;
    trex.width = TREX_STANDING_WIDTH;
    trex.height = TREX_STANDING_HEIGHT;

    if (++running_counter >= TREX_RUNNING_SPEED)
    {
        running_counter = 0;
        if (trex.sprite == trex_running1)
        {
            trex.sprite = trex_running2;
        } else
        {
            trex.sprite = trex_running1;
        }
    }
}

void GAME_UpdateDuckingTrex()
{
    static uint16_t running_counter = 0;

    trex.y = HEIGHT - TREX_DUCKING_HEIGHT - 1;
    trex.width = TREX_DUCKING_WIDTH;
    trex.height = TREX_DUCKING_HEIGHT;

    if (++running_counter >= TREX_RUNNING_SPEED)
    {
        running_counter = 0;
        if (trex.sprite == trex_ducking1)
        {
            trex.sprite = trex_ducking2;
        } else
        {
            trex.sprite = trex_ducking1;
        }
    }
}

void GAME_UpdateJumpingTrex()
{
    static uint8_t jump_max_y_reached = 0;

    trex.width = TREX_STANDING_WIDTH;
    trex.height = TREX_STANDING_HEIGHT;
    trex.sprite = trex_standing_init;

    /* jump up */
    if (!jump_max_y_reached
            && trex.y >= (HEIGHT - TREX_MAX_JUMP_HEIGHT))
    {
        trex.y -= JUMPING_SPEED;
    } else
        jump_max_y_reached = 1;

    /* let gravity do the landing */
    if (jump_max_y_reached
            && trex.y <= (HEIGHT - TREX_STANDING_HEIGHT - 2))
    {
        trex.y += GAME_GRAVITY;
    }

    //next state running
    if (jump_max_y_reached
            && trex.y > (HEIGHT - TREX_STANDING_HEIGHT - 2))
    {
        trex.y = HEIGHT - TREX_STANDING_HEIGHT - 1;
        if (is_right_button_pressed())
        {
            trex_state = DUCKING;
            // preload ducking sprite
            trex.sprite = trex_ducking1;
        } else
        {
            trex_state = RUNNING;
            // preload running sprite
            trex.sprite = trex_running1;
        }
        jump_max_y_reached = 0;
    }
}

void GAME_UpdateTrex()
{
    switch(trex_state)
    {
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

    if(FB_DrawGameObject(trex))
    {
        trex_state = CRASHED;
    }
}

void GAME_ShowScore()
{
    FB_DrawImage(WIDTH - DIGIT_WIDTH * 13, HI_SCORE_Y, hi_score_str, HI_SCORE_STR_WIDTH, HI_SCORE_STR_HEIGHT);
    FB_DrawUnsignedValue(WIDTH - DIGIT_WIDTH * 11, HI_SCORE_Y, high_score);
    FB_DrawUnsignedValue(WIDTH - DIGIT_WIDTH * 5 - 1, HI_SCORE_Y, score);
}

void GAME_Init()
{
    score = 0;
    game_speed = GAME_INITIAL_SPEED;
    trex_state = RUNNING;
    latest_cactus = 0; // index of the newest cactus in the array
    obstacle_respawn_base_distance = OBSTACLE_RESPAWN_BASE_DISTANCE;
    obstacle_respawn_max_distance = WIDTH - OBSTACLE_RESPAWN_BASE_DISTANCE;
    show_pterodactyl = SHOW_PTERODACTYL;
    inverted_mode = false;

    srand(global_clock);    // initialize PRNG

    GAME_InitHorizon();
    GAME_InitTrex();
    for(uint8_t i = 0; i < CACTUS_MAX_COUNT; i++)
        GAME_InitCactus(&obstacles[i]);
    GAME_InitPrerodactyl(&obstacles[PTERODACTYL]);

    FB_Clear();
    GAME_ShowScore();
    GAME_UpdateTrex();
    GAME_UpdateHorizon();

    /* RENDER */
    SSD1306_Display(frame_buffer);
}

void GAME_HandleState()
{
    // update trex state based on button states
    if (is_left_button_pressed())
    {
        trex_state = JUMPING;
    }
    if (is_right_button_pressed() && (trex_state != JUMPING))
    {
        if(trex_state == RUNNING)
        {
            // preload a ducking sprite
            trex.sprite = trex_ducking1;
        }
        trex_state = DUCKING;
    } else if (trex_state != JUMPING)
    {
        if(trex_state == DUCKING)
        {
            // preload a running sprite
            trex.sprite = trex_running1;
        }
        trex_state = RUNNING;
    }
}

void GAME_AdjustDifficulty()
{
    if((score % 100) == 0)
    {
        game_speed += GAME_SPEED_DELTA;
        // increase the distance between obstacles a little bit
        obstacle_respawn_base_distance += OBSTACLE_RESPAWN_DISTANCE_INC;
        obstacle_respawn_max_distance += OBSTACLE_RESPAWN_DISTANCE_INC;
        show_pterodactyl += OBSTACLE_RESPAWN_DISTANCE_INC * 2;
    }
}

int main() {
    stdio_init_all();
    struct repeating_timer timer;
    add_repeating_timer_ms(-1, repeating_timer_callback, NULL, &timer);
    BUTTONS_Init();
    SSD1306_Init();
    GAME_Init();

    bool button_released = false;    // used to prevent immediate restart of the game while holding the jumping button

    // wait for button press to start the game
    while(1)
    {
        if(!is_left_button_pressed())
        {
            button_released = true;
        }
        if(button_state && button_released)
        {
            button_released = false;
            break;
        }
        BUTTONS_MonitorButtons();
    }

    srand(global_clock);    // initialize PRNG

    while(1)
    {
        BUTTONS_MonitorButtons();
        /* GAME OVER */
        if(trex_state == CRASHED)
        {
            if(!is_left_button_pressed())
                button_released = true;

            // wait for jump button to restart the game
            if(is_left_button_pressed() && button_released)
            {
                button_released = false;
                GAME_Init();
            } else
            {
                if(inverted_mode) FB_InvertColor(); // restore to original buffer
                FB_DrawImage(WIDTH /2 - GAME_OVER_SPLASH_WIDTH / 2,
                        10, game_over_splash, GAME_OVER_SPLASH_WIDTH,
                        GAME_OVER_SPLASH_HEIGHT);
                if(inverted_mode) FB_InvertColor(); // invert back
                if(score > high_score)
                {
                    SSD1306_Clear();
                    high_score = score;
                }
                SSD1306_Display(frame_buffer);
                continue;
            }
        }

        GAME_HandleState();

        if (global_clock >= RENDER_PERIOD)
        {
            global_clock = 0;

            FB_Clear();

            GAME_ShowScore();

            // create new obstacles
            if(GAME_CountVisibleCacti(obstacles) < CACTUS_MAX_COUNT)
            {
                uint8_t previous_cactus;
                if(latest_cactus > 0)
                {
                    previous_cactus = latest_cactus - 1;
                } else
                {
                    previous_cactus = CACTUS_MAX_COUNT - 1;
                }
                if(obstacles[latest_cactus].visible == false &&
                        !obstacles[PTERODACTYL].visible)
                {
                    if((int)obstacles[previous_cactus].x <= (WIDTH -
                            obstacles[previous_cactus].width) ||
                            !obstacles[previous_cactus].visible)
                    {
                        GAME_CreateCactus(&obstacles[latest_cactus]);
                        uint16_t random_distance =
                                obstacle_respawn_base_distance +
                                rand() % obstacle_respawn_max_distance;
                        obstacles[latest_cactus].x += random_distance;
                        // respawn pterodatyl?
                        if(random_distance >= show_pterodactyl)
                        {
                            // replace cactus with pterodactyl
                            obstacles[latest_cactus].visible = false;
                            GAME_CreatePterodactyl(&obstacles[CACTUS_MAX_COUNT]);
                            obstacles[PTERODACTYL].x += random_distance;
                        }
                        latest_cactus++;
                    }
                }
                if(latest_cactus == CACTUS_MAX_COUNT)
                {
                    latest_cactus = 0;
                }
            }

            // update cacti
            for(uint8_t i = 0; i < CACTUS_MAX_COUNT; i++)
            {
                GAME_UpdateCactus(&obstacles[i]);
            }
            // update pterodactyl
            GAME_UpdatePterodactyl(&obstacles[PTERODACTYL]);

            // update trex
            GAME_UpdateTrex();
            GAME_UpdateHorizon();
            if(inverted_mode) FB_InvertColor();
            /* RENDER */
            SSD1306_Display(frame_buffer);
        }

        // speed up the game periodically
        if(game_speed_update_clock >= GAME_SCORE_INC_FREQ)
        {
            game_speed_update_clock = 0;
            score++;
            GAME_AdjustDifficulty();

            if((score % INVERTED_MODE_THRESHOLD) == 0)
            {
                inverted_mode = !inverted_mode;
            }
        }
    }

    return 0;
}
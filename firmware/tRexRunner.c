/*
 * twi.cpp
 *
 * Created: 21.02.2017 20:17:36
 * Author : Daniel Knezevic
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <math.h>
#include <stdlib.h>

#include "tRexRunner.h"
#include "sprites.h"

#define CONCAT(a, b)            a ## b
#define CONCAT_EXP(a, b)        CONCAT(a, b)

#define BUTTON_OUTPORT          CONCAT_EXP(PORT, BUTTON_IOPORTNAME)
#define BUTTON_INPORT           CONCAT_EXP(PIN, BUTTON_IOPORTNAME)
#define BUTTON_DDRPORT          CONCAT_EXP(DDR, BUTTON_IOPORTNAME)

#define left_button_state()     (!(BUTTON_INPORT & (1 << LEFT_BUTTON_BIT)))
#define right_button_state()    (!(BUTTON_INPORT & (1 << RIGHT_BUTTON_BIT)))

static uint8_t frame_buffer[WIDTH * HEIGHT / 8];

volatile uint16_t global_clock = 0;
volatile uint8_t lb_debounce_clock = 0;
volatile uint8_t rb_debounce_clock = 0;
volatile uint16_t game_speed_update_clock = 0;
volatile uint16_t delay_clock = 0;

volatile uint32_t seed = 0;

static float game_speed = GAME_INITIAL_SPEED;
static uint32_t high_score = 0;
static uint32_t score = 0;

static uint8_t button_state = 0x00;
static trex_states_t trex_state = RUNNING;

static horizon_t horizon;
static game_object_t trex;
static game_object_t obstacles[CACTUS_MAX_COUNT + 1]; // last element is pterodactyl

static uint8_t latest_cactus = 0; // index of the newest cactus in the array
static uint16_t cactus_respawn_max_delay = CACTUS_RESPAWN_MAX_DELAY;
static uint16_t cactus_respawn_delay = 0;

static uint16_t pterodactyl_respawn_delay = 65535;
static uint8_t respawn_pterodactyl = FALSE;

/*
 * Timer1 "Compare Match" ISR
 */
ISR(TIMER1_COMPA_vect, ISR_NOBLOCK)
{
    global_clock++;
    lb_debounce_clock++;
    rb_debounce_clock++;
    game_speed_update_clock++;
    if(delay_clock)
        delay_clock--;
    seed++;
}

/*
 * Initialize Timer1
 */
void TIMER_init(void)
{
    TCCR1B |= (1 << WGM12); // CTC mode of Timer1
    OCR1A = 249;           // OCR1A Timer1's TOP value for 1mS @8Mhz
    TCCR1B |= (1 << CS11) | (1 << CS10); // start Timer1, clkI/O/64 (From prescaler) 125kHz
    TIMSK1 |= (1 << OCIE1A); // Timer/Counter1 Output Compare Match A Interrupt Enable
}

void TIMER_delay(uint16_t delay)
{
    delay_clock = delay;
    while(delay_clock);
}

void BUTTONS_init()
{
    BUTTON_OUTPORT |= (1 << LEFT_BUTTON_BIT); // pull up LEFT_BUTTON_IO
    BUTTON_DDRPORT &= ~(1 << LEFT_BUTTON_BIT); // configure LEFT_BUTTON_IO as input

    BUTTON_OUTPORT |= (1 << RIGHT_BUTTON_BIT); // pull up the RIGHT_BUTTON_IO
    BUTTON_DDRPORT &= ~(1 << RIGHT_BUTTON_BIT); // configure RIGHT_BUTTON_IO as input
}

void BUTTONS_monitorButtons()
{
    cli();
    if (lb_debounce_clock >= DEBOUNCE_INTERVAL)
    {
        lb_debounce_clock = 0;
        if (left_button_state())
            button_state |= (1 << LEFT_BUTTON_BIT);
        else
            button_state &= ~(1 << LEFT_BUTTON_BIT);
    } else if ((button_state & (1 << LEFT_BUTTON_BIT)) == left_button_state())
    {
        lb_debounce_clock = 0;
    }

    if (rb_debounce_clock >= DEBOUNCE_INTERVAL)
    {
        rb_debounce_clock = 0;
        if (right_button_state())
            button_state |= (1 << RIGHT_BUTTON_BIT);
        else
            button_state &= ~(1 << RIGHT_BUTTON_BIT);
    } else if ((button_state & (1 << RIGHT_BUTTON_BIT)) == right_button_state())
    {
        rb_debounce_clock = 0;
    }
    sei();
}

void FB_Clear()
{
    memset(frame_buffer, 0, sizeof(uint8_t) * (WIDTH * HEIGHT / 8));
}

uint8_t FB_DrawImage(int16_t x, int16_t y, const __flash uint8_t* image, uint8_t width, uint8_t height)
{
    uint8_t collision = FALSE;
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
                    collision = TRUE;
                frame_buffer[buffer_index]|=(1 << (h % 8));
            }
        }
    }
    return collision;
}

void FB_DrawUnsignedValue(int16_t x, int16_t y, uint32_t value)
{
    int16_t xx = x;
    for (uint32_t dividend = 10000; dividend > 0; dividend /= 10) {
        FB_DrawImage(xx, y, &digits[(value / dividend % 10) * DIGIT_WIDTH],
                DIGIT_WIDTH, DIGIT_HEIGHT);
        xx += DIGIT_WIDTH;
    }
}

uint8_t FB_DrawGameObject(game_object_t game_object)
{
    if(!game_object.visible)
        return FALSE;
    return FB_DrawImage(floor(game_object.x), floor(game_object.y),
            game_object.sprite, game_object.width, game_object.height);
}

void FB_SetPixel(uint8_t x, uint8_t y)
{
    if((x >= WIDTH) || (y >= HEIGHT)) return;
    uint16_t buffer_index = WIDTH * (y / 8) + x;
    frame_buffer[buffer_index] |= (1 << (y % 8));

}

void FB_ClearPixel(uint8_t x, uint8_t y)
{
    if((x >= WIDTH) || (y >= HEIGHT)) return;
    uint16_t buffer_index = WIDTH * (y / 8) + x;
    frame_buffer[buffer_index] &= ~(1 << (y % 8));

}

void GAME_InitHorizon()
{
    horizon.x = 0;
    horizon.y = HEIGHT - HORIZON_LINE_HEIGHT - 1;
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
                (trex.y > trex.height - 1))
            continue;
        if(trex_state == DUCKING &&
                (i >= trex.x + TREX_DUCKING_CLEARENCE_MIN) &&
                (i < trex.x + TREX_DUCKING_CLEARENCE_MAX))
            continue;

        if(i >= horizon.bump1_x && i < horizon.bump1_x + horizon.bump1_width)
            // draw first bump
            FB_SetPixel(i, horizon.y);
        else if(i >= horizon.bump2_x && i < horizon.bump2_x + horizon.bump2_width)
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
    pterodactyl->visible = TRUE;
}

void GAME_CreatePterodactyl(game_object_t *pterodactyl)
{
    uint8_t range = PTERODACTYL_MIN_FLY_HEIGHT - PTERODACTYL_MAX_FLY_HEIGHT;
    pterodactyl->x = WIDTH;
    pterodactyl->y = PTERODACTYL_MAX_FLY_HEIGHT + (rand() % range);
    pterodactyl->width = PTERODACTYL_WIDTH;
    pterodactyl->height = PTERODACTYL_HEIGHT;
    pterodactyl->sprite = pterodactyl1;
    pterodactyl->visible = TRUE;
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
        pterodactyl->visible = FALSE;
    }
}

void GAME_InitCactus(game_object_t *cactus)
{
    cactus->visible = FALSE;
}

void GAME_CreateCactus(game_object_t *cactus)
{
    uint8_t cactus_type = rand() % CACTUS_NUMBER_OF_SPECIES;

    cactus->x = WIDTH;
    cactus->y = HEIGHT - CACTUS_PADDING_BOTTOM;
    cactus->visible = TRUE;

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
        cactus->visible = FALSE;
    }
}

uint8_t GAME_CountVisibleCactuses(game_object_t cactus[])
{
    uint8_t cactuses_on_screen = 0;
    for(uint8_t i = 0; i < CACTUS_MAX_COUNT; i++)
    {
        if(cactus[i].visible)
            cactuses_on_screen++;
    }
    return cactuses_on_screen;
}

void GAME_InitTrex()
{
    trex.x = TREX_PADDING_RIGHT;
    trex.y = HEIGHT - TREX_STANDING_HEIGHT - 1;
    trex.width = TREX_STANDING_WIDTH;
    trex.height = TREX_STANDING_HEIGHT;
    trex.sprite = trex_running1;
    trex.visible = TRUE;
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
        if (button_state & (1 << RIGHT_BUTTON_BIT))
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

void GAME_UpdateChrashedTrex()
{
    trex.width = TREX_STANDING_WIDTH;
    trex.height = TREX_STANDING_HEIGHT;
    trex.sprite = trex_dead;
}

void GAME_UpdateTrex()
{
    switch(trex_state)
    {
    case WAITING:
        // TODO implement me
        break;
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
        GAME_UpdateChrashedTrex();
        break;
    }


    if(FB_DrawGameObject(trex))
    {
        trex_state = CRASHED;
    }
}

void GAME_ShowScore()
{
    FB_DrawImage(WIDTH - DIGIT_WIDTH * 13, 0, hi_score_str, HI_SCORE_STR_WIDTH, HI_SCORE_STR_HEIGHT);
    FB_DrawUnsignedValue(WIDTH - DIGIT_WIDTH * 11, 0, high_score);
    FB_DrawUnsignedValue(WIDTH - DIGIT_WIDTH * 5 - 1, 0, score);
}

void GAME_Init()
{
    high_score = 0; //TODO read from eeprom
    score = 0;
    game_speed = GAME_INITIAL_SPEED;
    trex_state = RUNNING;
    latest_cactus = 0; // index of the newest cactus in the array
    cactus_respawn_max_delay = CACTUS_RESPAWN_MAX_DELAY;
    cactus_respawn_delay = 0;
    pterodactyl_respawn_delay = 65535;
    respawn_pterodactyl = FALSE;

    srand(seed);    // initialize PRNG

    GAME_InitHorizon();
    GAME_InitTrex();
    for(uint8_t i = 0; i < CACTUS_MAX_COUNT; i++)
        GAME_InitCactus(&obstacles[i]);
    GAME_InitPrerodactyl(&obstacles[CACTUS_MAX_COUNT]);

    FB_Clear();
    GAME_UpdateHorizon();
    GAME_ShowScore();
    GAME_UpdateTrex();

    /* RENDER */
    SSD1306_display(frame_buffer);
}

// TODO battery monitor
// TODO inactivity monitor
// TODO game over string
int main(void)
{
    BUTTONS_init();
    TIMER_init();
    sei();

    TIMER_delay(1700);

    SSD1306_init();
    SSD1306_clear();

    GAME_Init();

    uint8_t button_released = FALSE;    // used to prevent immediate restart of the game while holding the jumping button

    //wait for button press to start the game
    while(!button_state)
    {
        BUTTONS_monitorButtons(&button_state);
    }
    srand(seed);    // initialize PRNG

    while (1)
    {
        BUTTONS_monitorButtons();

        /* GAME OVER */
        if(trex_state == CRASHED)
        {
            if(!(button_state & (1 << LEFT_BUTTON_BIT)))
                button_released = TRUE;

            // wait for jump button to restart the game
            if((button_state & (1 << LEFT_BUTTON_BIT)) && button_released)
            {
                button_released = FALSE;
                GAME_Init();
            }
            continue;
        }

        if (global_clock >= RENDER_PERIOD)
        {
            global_clock = 0;

            // update trex state based on button states
            if (button_state & (1 << LEFT_BUTTON_BIT))
            {
                trex_state = JUMPING;
            }
            if ((button_state & (1 << RIGHT_BUTTON_BIT)) && (trex_state != JUMPING))
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

            FB_Clear();

            GAME_ShowScore(high_score, score);

            GAME_UpdateHorizon();

            // create new obstacles
            if(GAME_CountVisibleCactuses(obstacles) <= CACTUS_MAX_COUNT)
            {
                // time for creating new cactus?
                if(obstacles[latest_cactus].visible == FALSE && cactus_respawn_delay == 0)
                {
                    GAME_CreateCactus(&obstacles[latest_cactus]);
                    // update delar for creating new one
                    cactus_respawn_delay = CACTUS_RESPAWN_BASE_DELAY + rand() % cactus_respawn_max_delay;
                    latest_cactus++;
                    // respawn pterodactyl?
                    if(cactus_respawn_delay >= SHOW_PTERODACTYL && !obstacles[CACTUS_MAX_COUNT].visible)
                    {
                        pterodactyl_respawn_delay = cactus_respawn_delay / 2;
                        respawn_pterodactyl = TRUE;
                    }
                }
                if(latest_cactus == CACTUS_MAX_COUNT)
                {
                    latest_cactus = 0;
                }
            }

            if(pterodactyl_respawn_delay == 0 && respawn_pterodactyl)
            {
                respawn_pterodactyl = FALSE;
                GAME_CreatePterodactyl(&obstacles[CACTUS_MAX_COUNT]);
            }

            if(cactus_respawn_delay)
            {
                cactus_respawn_delay--;
            }

            if(pterodactyl_respawn_delay)
            {
                pterodactyl_respawn_delay--;
            }

            // update cactuses
            for(uint8_t i = 0; i < CACTUS_MAX_COUNT; i++)
            {
                GAME_UpdateCactus(&obstacles[i]);
            }
            // update pterodactyl
            GAME_UpdatePterodactyl(&obstacles[CACTUS_MAX_COUNT]);

            // update trex
            GAME_UpdateTrex();

            /* RENDER */
            SSD1306_display(frame_buffer);
        }

        // speed up the game periodically
        if(game_speed_update_clock >= GAME_SCORE_INCREMENT)
        {
            game_speed_update_clock = 0;
            score++;
            if((score % 100) == 0)
            {
                game_speed += GAME_SPEED_DELTA;
                if(cactus_respawn_max_delay >= CACTUS_RESPAWN_MAX_LIMIT)
                {
                    cactus_respawn_max_delay -= CACTUS_RESPAWN_DELAY_DECREMENT;
                }
            }
        }
    }
}

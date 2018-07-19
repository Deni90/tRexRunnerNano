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
volatile uint16_t test_clock = 0;

volatile uint32_t seed = 0;

uint8_t button_state = 0x00;

float game_speed = GAME_INITIAL_SPEED;

//128x3
static const __flash uint8_t horizon_line[] =
{
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x04, 0x04, 0x04, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x04, 0x04, 0x04, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02
};

// 14x16px
static const __flash uint8_t trex_standing_init[] =
{
    0xfc, 0xe0, 0xc0, 0xe0, 0xe0, 0xf0, 0xfe, 0xff, 0xfd, 0x5f, 0xd7, 0x17, 0x07, 0x06,
    0x01, 0x03, 0x07, 0x3f, 0x2f, 0x07, 0x3f, 0x23, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00
};

// 14x16px
static const __flash uint8_t trex_running1[] =
{
    0xfc, 0xe0, 0xc0, 0xe0, 0xe0, 0xf0, 0xfe, 0xff, 0xfd, 0x5f, 0xd7, 0x17, 0x07, 0x06,
    0x01, 0x03, 0x07, 0x1f, 0x17, 0x07, 0x3f, 0x23, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00
};

// 14x16px
static const __flash uint8_t trex_running2[] =
{
    0xfc, 0xe0, 0xc0, 0xe0, 0xe0, 0xf0, 0xfe, 0xff, 0xfd, 0x5f, 0xd7, 0x17, 0x07, 0x06,
    0x01, 0x03, 0x07, 0x3f, 0x2f, 0x07, 0x0f, 0x0b, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00
};

// 14x16px
static const __flash uint8_t trex_dead[] =
{
    0xfc, 0xe0, 0xc0, 0xe0, 0xe0, 0xf0, 0xfe, 0xf5, 0xfd, 0x5f, 0xfb, 0x17, 0x07, 0x06,
    0x01, 0x03, 0x07, 0x3f, 0x2f, 0x07, 0x3f, 0x23, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00
};

// 22x10px
static const __flash uint8_t trex_ducking1[] =
{
    0x03, 0x06, 0x0e, 0x1c, 0xfc, 0x7c, 0x3e, 0x1e, 0x7e, 0x5e, 0x1e, 0x7e, 0x5c, 0x0c, 0x1e, 0x1d, 0x1f, 0x1f, 0x17, 0x17, 0x07, 0x06,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
// FIXME 22x10px
static const __flash uint8_t trex_ducking2[] =
{
    0x03, 0x06, 0x0e, 0x1c, 0x7c, 0x5c, 0x3e, 0xfe, 0x7e, 0x7e, 0x1e, 0x7e, 0x5c, 0x0c, 0x1e, 0x1d, 0x1f, 0x1f, 0x17, 0x17, 0x07, 0x06,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

//11x10
static const __flash uint8_t pterodactyl1[] =
{
    0x10, 0x18, 0x1c, 0x30, 0x7f, 0xfe, 0xfc, 0xf8, 0xa0, 0xa0, 0x20,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

//11x10
static const __flash uint8_t pterodactyl2[] =
{
    0x10, 0x18, 0x1c, 0x3c, 0xf0, 0xf0, 0xf0, 0xf0, 0xa0, 0xa0, 0x20,
    0x00, 0x00, 0x00, 0x00, 0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00
};

//5x8
static const __flash uint8_t cactus1[] =
{
    0x1c, 0x10, 0xfe, 0x20, 0x3c
};

//5x8
static const __flash uint8_t cactus2[] =
{
    0x0c, 0x08, 0xff, 0x10, 0x1c
};

//6x9
static const __flash uint8_t cactus3[] =
{
    0x0e, 0x08, 0xfe, 0xff, 0x20, 0x38,
    0x00, 0x00, 0x01, 0x01, 0x00, 0x00
};

//8x13
static const __flash uint8_t cactus4[] =
{
    0xf0, 0xf8, 0x80, 0xfe, 0xff, 0x80, 0xf0, 0xf8,
    0x00, 0x01, 0x01, 0x1f, 0x1f, 0x01, 0x01, 0x00
};

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

void BUTTONS_init()
{
    BUTTON_OUTPORT |= (1 << LEFT_BUTTON_BIT); // pull up LEFT_BUTTON_IO
    BUTTON_DDRPORT &= ~(1 << LEFT_BUTTON_BIT); // configure LEFT_BUTTON_IO as input

    BUTTON_OUTPORT |= (1 << RIGHT_BUTTON_BIT); // pull up the RIGHT_BUTTON_IO
    BUTTON_DDRPORT &= ~(1 << RIGHT_BUTTON_BIT); // configure RIGHT_BUTTON_IO as input
}

void BUTTONS_monitorButtons(uint8_t *buttons_state)
{
    cli();
    if (lb_debounce_clock >= DEBOUNCE_INTERVAL)
    {
        lb_debounce_clock = 0;
        if (left_button_state())
            *buttons_state |= (1 << LEFT_BUTTON_BIT);
        else
            *buttons_state &= ~(1 << LEFT_BUTTON_BIT);
    } else if ((*buttons_state & (1 << LEFT_BUTTON_BIT)) == left_button_state())
    {
        lb_debounce_clock = 0;
    }

    if (rb_debounce_clock >= DEBOUNCE_INTERVAL)
    {
        rb_debounce_clock = 0;
        if (right_button_state())
            *buttons_state |= (1 << RIGHT_BUTTON_BIT);
        else
            *buttons_state &= ~(1 << RIGHT_BUTTON_BIT);
    } else if ((*buttons_state & (1 << RIGHT_BUTTON_BIT)) == right_button_state())
    {
        rb_debounce_clock = 0;
    }
    sei();

}

/*
 * Timer1 "Compare Match" ISR
 */
ISR(TIMER1_COMPA_vect, ISR_NOBLOCK)
{
    global_clock++;
    lb_debounce_clock++;
    rb_debounce_clock++;
    game_speed_update_clock++;
    test_clock++;
    seed++;
}

void FB_clear()
{
    memset(frame_buffer, 0, sizeof(uint8_t) * (WIDTH * HEIGHT / 8));
}

void FB_drawImage(int16_t x, int16_t y, const __flash uint8_t* image, uint8_t width, uint8_t height)
{
    for(int h = y; h < (y + height); h++)
    {
        for(int w = x; w < (x + width); w++)
        {
            if((w >= SSD1306_LCDWIDTH) || (h >= SSD1306_LCDHEIGHT)) continue;
            if( h < 0 || w < 0 ) continue;
            uint16_t buffer_index = SSD1306_LCDWIDTH * (h / 8) + w;
            uint8_t image_w = w - x;
            uint8_t image_h = h - y;
            uint16_t image_index = width * (image_h / 8) + image_w;
            if((image[image_index] >> (image_h % 8) & 0x01) == 0x01)
            {
                frame_buffer[buffer_index]|=(1 << (h % 8));
            }
            /* //only apply "or" to buffer
             else
             {
             frame_buffer[buffer_index]&=~(1 << (h % 8));
             }*/
        }
    }
}

void FB_drawGameObject(game_object_t game_object)
{
    FB_drawImage(game_object.x, game_object.y, game_object.sprite,
            game_object.width, game_object.height);
}

void GAME_UpdateHorizon(game_object_t *horizon)
{
    FB_drawGameObject(*horizon);
    if (horizon->delta < 0)
    {
        game_object_t new_horizon = *horizon;
        new_horizon.x = WIDTH + horizon->x;
        FB_drawGameObject(new_horizon);
    }
    if (horizon->delta + 128 > 0)
    {
        horizon->delta -= game_speed;
    } else
    {
        horizon->delta = 0;
    }
    horizon->x = floor(horizon->delta);
}

void GAME_CreatePterodactyl(game_object_t *pterodactyl)
{
    uint8_t range = PTERODACTYL_MIN_FLY_HEIGHT - PTERODACTYL_MAX_FLY_HEIGHT;
    pterodactyl->x = WIDTH;
    pterodactyl->y = PTERODACTYL_MAX_FLY_HEIGHT + (rand() % range);
    pterodactyl->width = PTERODACTYL_WIDTH;
    pterodactyl->height = PTERODACTYL_HEIGHT;
    pterodactyl->sprite = pterodactyl1;
    pterodactyl->delta = pterodactyl->x;
    pterodactyl->visible = TRUE;
}

void GAME_UpdatePterodactyl(game_object_t *pterodactyl)
{
    static unsigned int flapping_counter = 0;

    // move to the left in small steps
    if (pterodactyl->delta + pterodactyl->width > 0)
    {
        pterodactyl->delta -= game_speed;
    } else
    {
        pterodactyl->visible = FALSE;
    }

    pterodactyl->x = floor(pterodactyl->delta);


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
    FB_drawGameObject(*pterodactyl);
}

void GAME_CreateCactus(game_object_t *cactus)
{
    uint8_t cactus_type = rand() % CACTUS_NUMBER_OF_SPECIES;

    cactus->x = WIDTH;
    cactus->y = HEIGHT - CACTUS_PADDING_BOTTOM;
    cactus->delta = WIDTH;
    cactus->visible = TRUE;

    switch(cactus_type){
    case 0:
        cactus->sprite = cactus1;
        cactus->width = 5;
        cactus->height = 8;
        break;
    case 1:
        cactus->sprite = cactus2;
        cactus->width = 5;
        cactus->height = 8;
        break;
    case 2:
        cactus->sprite = cactus3;
        cactus->width = 6;
        cactus->height = 9;
        break;
    case 3:
        cactus->sprite = cactus4;
        cactus->width = 8;
        cactus->height = 13;
        break;
    default:
        cactus->sprite = cactus3;
        cactus->width = 6;
        cactus->height = 9;
    }

    cactus->y -= cactus->height;
}

void GAME_UpdateCactus(game_object_t *cactus)
{
        // move to the left in small steps
    if (cactus->delta + cactus->width > 0)
    {
        cactus->delta -= game_speed;
    } else
    {
        cactus->visible = FALSE;
    }

    cactus->x = floor(cactus->delta);
    FB_drawGameObject(*cactus);
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

void GAME_UpdateRunningTrex(game_object_t *trex)
{
    static uint16_t running_counter = 0;

    trex->y = HEIGHT - TREX_STANDING_HEIGHT - 1;
    trex->width = TREX_STANDING_WIDTH;
    trex->height = TREX_STANDING_HEIGHT;

    if (++running_counter >= TREX_RUNNING_SPEED)
    {
        running_counter = 0;
        if (trex->sprite == trex_running1)
        {
            trex->sprite = trex_running2;
        } else
        {
            trex->sprite = trex_running1;
        }
    }
}

void GAME_UpdateDuckingTrex(game_object_t *trex)
{
    static uint16_t running_counter = 0;

    trex->y = HEIGHT - TREX_DUCKING_HEIGHT - 1;
    trex->width = TREX_DUCKING_WIDTH;
    trex->height = TREX_DUCKING_HEIGHT;

    if (++running_counter >= TREX_RUNNING_SPEED)
    {
        running_counter = 0;
        if (trex->sprite == trex_ducking1)
        {
            trex->sprite = trex_ducking2;
        } else
        {
            trex->sprite = trex_ducking1;
        }
    }
}

void GAME_UpdateJumpingTrex(game_object_t *trex, trex_states_t *trex_state)
{
    static uint8_t jump_max_y_reached = 0;
    static float trex_y_delta = HEIGHT - TREX_STANDING_HEIGHT - 1;

    trex->width = TREX_STANDING_WIDTH;
    trex->height = TREX_STANDING_HEIGHT;
    trex->sprite = trex_standing_init;

    /* jump up */
    if (!jump_max_y_reached
            && trex->y >= (HEIGHT - TREX_MAX_JUMP_HEIGHT))
    {
        trex_y_delta -= JUMPING_SPEED;
        trex->y = floor(trex_y_delta);
    } else
        jump_max_y_reached = 1;

    /* let gravity do the landing */
    if (jump_max_y_reached
            && trex->y <= (HEIGHT - TREX_STANDING_HEIGHT - 2))
    {
        trex_y_delta += GAME_GRAVITY;
        trex->y = floor(trex_y_delta);
    }

    //next state running
    if (jump_max_y_reached
            && trex->y > (HEIGHT - TREX_STANDING_HEIGHT - 2))
    {
        trex_y_delta = HEIGHT - TREX_STANDING_HEIGHT - 1;
        if (button_state & (1 << PD1))
        {
            *trex_state = DUCKING;
        } else
        {
            *trex_state = RUNNING;
        }
        jump_max_y_reached = 0;
    }
}

void GAME_UpdateChrashedTrex(game_object_t *trex)
{

}

void GAME_UpdateTrex(game_object_t *trex, trex_states_t *trex_state)
{
    switch(*trex_state)
    {
    case WAITING:
        // TODO implement me
        break;
    case RUNNING:
        GAME_UpdateRunningTrex(trex);
        break;
    case DUCKING:
        GAME_UpdateDuckingTrex(trex);
        break;
    case JUMPING:
        GAME_UpdateJumpingTrex(trex, trex_state);
        break;
    case CRASHED:
        GAME_UpdateChrashedTrex(trex);
        break;
    }

    FB_drawGameObject(*trex);
}

// TODO turning on
// TODO battery monitor
// TODO inactivity monitor

// TODO start game animation
// TODO score + highscore
// TODO collision detection
// TODO game over
// TODO random pterodactyl generation
// TODO fix trex ducking glitch
// TODO add clearence between trex and horizon

#include <util/delay.h>

int main(void)
{
    BUTTONS_init();
    TIMER_init();
    sei();
    SSD1306_init();
    SSD1306_clear();

    game_object_t horizon = {
        WIDTH,
        HEIGHT - HORIZON_LINE_HEIGHT - 1,
        HORIZON_LINE_WIDTH,
        HORIZON_LINE_HEIGHT,
        horizon_line,
        0,
        TRUE
    };

    game_object_t trex = {
        8,
        HEIGHT - TREX_STANDING_HEIGHT - 1,
        TREX_STANDING_WIDTH,
        TREX_STANDING_HEIGHT,
        trex_running1,
        HEIGHT - TREX_STANDING_HEIGHT - 1,
        TRUE

    };

    trex_states_t trex_state = RUNNING;

    game_object_t cactus[CACTUS_MAX_COUNT];
    uint8_t latest_cactus = 0; // index of the newest cactus in the array
    uint16_t cactus_respawn_delay = 0;

//    while(!button_state)
//    {
//        BUTTONS_monitorButtons(&button_state);
//    }
    srand(seed);    // initialize PRNG

    while (1)
    {
        BUTTONS_monitorButtons(&button_state);


        if (global_clock >= RENDER_PERIOD)
        {
            global_clock = 0;

            /* UPDATE GAME */
            if (button_state & (1 << PD0))
            {
                trex_state = JUMPING;
            }
            if ((button_state & (1 << PD1)) && (trex_state != JUMPING))
            {
                trex_state = DUCKING;
            } else if (trex_state != JUMPING)
            {
                trex_state = RUNNING;
            }

            FB_clear();

            GAME_UpdateHorizon(&horizon);

            // create new cactuses
            if(GAME_CountVisibleCactuses(cactus) <= CACTUS_MAX_COUNT)
            {
                // time for creating new cactus?
                if(cactus[latest_cactus].visible == FALSE && cactus_respawn_delay == 0)
                {
                    GAME_CreateCactus(&cactus[latest_cactus]);
                    cactus_respawn_delay = CACTUS_RESPAWN_BASE_DELAY + rand() % CACTUS_RESPAWN_MAX_DELAY;
                    latest_cactus++;
                }

                if(latest_cactus == CACTUS_MAX_COUNT)
                {
                    latest_cactus = 0;
                }
            }

            if(cactus_respawn_delay)
            {
                cactus_respawn_delay--;
            }

            // update cactuses
            for(uint8_t i = 0; i < CACTUS_MAX_COUNT; i++)
            {
                GAME_UpdateCactus(&cactus[i]);
            }

            GAME_UpdateTrex(&trex, &trex_state);

            /* RENDER */
            SSD1306_display(frame_buffer);
        }

        // speed up the game periodically
        if(game_speed_update_clock >= GAME_SPEED_UPDATE_TIME)
        {
            game_speed_update_clock = 0;
            game_speed += GAME_SPEED_DELTA;
        }
    }
}

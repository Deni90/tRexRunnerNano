/*
 * twi.cpp
 *
 * Created: 21.02.2017 20:17:36
 * Author : Daniel Knezevic
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <math.h>

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
volatile uint16_t test_clock = 0;
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
    test_clock++;
}

void FB_Clear()
{
    memset(frame_buffer, 0, sizeof(uint8_t) * (WIDTH * HEIGHT / 8));
}

void FB_drawImage(int8_t x, int8_t y, const __flash uint8_t* image, uint8_t width, uint8_t height)
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

// TODO turning on
// TODO battery monitor
// TODO inactivity monitor

// TODO start game animation
// TODO score + highscore
// TODO collision detection
// TODO game over
// TODO random cactus and pterodactyl generation
// TODO fix trex ducking glitch
// TODO add clearence between trex and horizon

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
        horizon_line
    };
    float horizon_delta = 0;

    game_object_t trex = {
        8,
        HEIGHT - TREX_STANDING_HEIGHT - 1,
        TREX_STANDING_WIDTH,
        TREX_STANDING_HEIGHT,
        trex_running1
    };
    trex_states_t trex_state = RUNNING;
    uint8_t trex_running_leg = 0;
    unsigned int running_counter = 0;
    float trex_y_delta = HEIGHT - TREX_STANDING_HEIGHT - 1;
    uint8_t jump_max_y_reached = 0;

    game_object_t pterodactyl = {
        WIDTH, //TODO change me
        5,
        PTERODACTYL_WIDTH,
        PTERODACTYL_HEIGHT,
        pterodactyl1
    };
    uint8_t pterodactyl_wing = 0;
    float pterodactyl_x_delta = WIDTH;
    unsigned int flapping_counter = 0;

    uint8_t button_state = 0x00;

    while (1)
    {
        BUTTONS_monitorButtons(&button_state);

        /* UPDATE GAME */
        if (global_clock >= RENDER_PERIOD)
        {
            global_clock = 0;

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

            FB_Clear();

            /* HORIZONT LINE BEGIN*/
            FB_drawImage(horizon.x, horizon.y, horizon.sprite, horizon.width,
                    horizon.height);
            if (horizon_delta < 0)
            {
                FB_drawImage(WIDTH + horizon.x, horizon.y, horizon.sprite,
                        horizon.width, horizon.height);
            }
            if (horizon_delta + 128 > 0)
            {
                horizon_delta -= GAME_SPEED;
            } else
            {
                horizon_delta = 0;
            }
            horizon.x = floor(horizon_delta);
            /* HORIZONT LINE END*/

            FB_drawImage(30, 22, cactus1, 5, 8);
            FB_drawImage(36, 22, cactus2, 5, 8);
            FB_drawImage(70, 21, cactus3, 6, 9);
            FB_drawImage(100, 17, cactus4, 8, 13);

            /* PTERODACTYL BEGIN */
            if (pterodactyl_x_delta + PTERODACTYL_WIDTH > 0)
                pterodactyl_x_delta -= GAME_SPEED;
            else
                pterodactyl_x_delta = WIDTH; //TODO change to random walue
            pterodactyl.x = floor(pterodactyl_x_delta);

            if (++flapping_counter >= PTERODACTYL_WING_SWAP)
            {

                flapping_counter = 0;

                if (pterodactyl_wing)
                {
                    pterodactyl.sprite = pterodactyl1;
                    pterodactyl_wing = 0;
                } else
                {
                    pterodactyl.sprite = pterodactyl2;
                    pterodactyl_wing = 1;
                }
            }
            FB_drawGameObject(pterodactyl);
            /* PTERODACTYL END */

            if (trex_state == RUNNING)
            {
                trex.y = HEIGHT - TREX_STANDING_HEIGHT - 1;
                trex.width = TREX_STANDING_WIDTH;
                trex.height = TREX_STANDING_HEIGHT;
                if (++running_counter >= TREX_RUNNING_SPEED)
                {
                    running_counter = 0;
                    if (trex_running_leg)
                    {
                        trex_running_leg = 0;
                        trex.sprite = trex_running2;
                    } else
                    {
                        trex_running_leg = 1;
                        trex.sprite = trex_running1;
                    }
                }
            } else if (trex_state == DUCKING)
            {
                trex.y = HEIGHT - TREX_DUCKING_HEIGHT - 1;
                trex.width = TREX_DUCKING_WIDTH;
                trex.height = TREX_DUCKING_HEIGHT;
                if (++running_counter >= TREX_RUNNING_SPEED)
                {
                    running_counter = 0;
                    if (trex_running_leg)
                    {
                        trex_running_leg = 0;
                        trex.sprite = trex_ducking1;
                    } else
                    {
                        trex_running_leg = 1;
                        trex.sprite = trex_ducking2;
                    }
                }
            } else if (trex_state == JUMPING)
            {
                trex.width = TREX_STANDING_WIDTH;
                trex.height = TREX_STANDING_HEIGHT;
                trex.sprite = trex_standing_init;

                /* jump up */
                if (!jump_max_y_reached
                        && trex.y >= (HEIGHT - TREX_MAX_JUMP_HEIGHT))
                {
                    trex_y_delta -= JUMPING_SPEED;
                    trex.y = floor(trex_y_delta);
                } else
                    jump_max_y_reached = 1;

                /* let gravity do the landing */
                if (jump_max_y_reached
                        && trex.y <= (HEIGHT - TREX_STANDING_HEIGHT - 2))
                {
                    trex_y_delta += GAME_GRAVITY;
                    trex.y = floor(trex_y_delta);
                }

                //next state running
                if (jump_max_y_reached
                        && trex.y > (HEIGHT - TREX_STANDING_HEIGHT - 2))
                {
                    if (button_state & (1 << PD1))
                    {
                        trex_state = DUCKING;
                    } else
                    {
                        trex_state = RUNNING;
                    }
                    jump_max_y_reached = 0;
                }
            }
            FB_drawGameObject(trex);
            /* RENDER */
            SSD1306_display(frame_buffer);
        }
    }
}

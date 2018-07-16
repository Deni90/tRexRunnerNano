/*
 * twi.cpp
 *
 * Created: 21.02.2017 20:17:36
 * Author : Daniel Knezevic
 */

#include <avr/io.h>
#include <avr/interrupt.h>

#include "tRexRunner.h"

static uint8_t frame_buffer[WIDTH * HEIGHT / 8];
volatile unsigned int global_clock = 0;
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

/*
 * Timer1 "Compare Match" ISR
 */
ISR(TIMER1_COMPA_vect, ISR_NOBLOCK)
{
    global_clock++;
}

void FB_Clear()
{
    memset(frame_buffer, 0, sizeof(uint8_t) * (WIDTH * HEIGHT / 8));
}

int main(void)
{
    TIMER_init();
    sei();
    SSD1306_init();
    SSD1306_clear();

    uint8_t i = 0;

    while (1)
    {
        if(global_clock >= RENDER_PERIOD)
        {
            global_clock = 0;
            FB_Clear();
            memset(frame_buffer, ++i, sizeof(uint8_t) * (WIDTH * HEIGHT / 8));
            SSD1306_display(frame_buffer);
        }
    }
}


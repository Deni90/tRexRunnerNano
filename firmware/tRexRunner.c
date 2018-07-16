/*
 * twi.cpp
 *
 * Created: 21.02.2017 20:17:36
 * Author : Daniel Knezevic
 */

#include <avr/io.h>

#include "tRexRunner.h"

static uint8_t frame_buffer[WIDTH * HEIGHT / 8];

int main(void)
{
    SSD1306_init();
    SSD1306_clear();

    memset(frame_buffer, 0x0f, sizeof(uint8_t) * (WIDTH * HEIGHT / 8));
    SSD1306_display(frame_buffer);
    while (1)
    {
    }
}


/*
 * ssd1306.c
 *
 *  Created on: Jul 13, 2018
 *      Author: Daniel Knezevic
 */

#include "ssd1306.h"
#include "../twi/twi.h"

/*
 * control
 * 0x80 : Single Command byte
 * 0x00 : Command Stream
 * 0xC0 : Single Data byte
 * 0x40 : Data Stream
 *
 */
static void SSD1306_command(uint8_t c)
{
    TWI_Start();
    TWI_Write(_i2c_address);
    TWI_Write(0x00); // Control This is Command
    TWI_Write(c);
    TWI_Stop();
}

void SSD1306_init()
{
    TWI_Init();

    // Init sequence for 128x64 OLED module
    SSD1306_command(SSD1306_DISPLAYOFF);                    // 0xAE
    SSD1306_command(SSD1306_SETDISPLAYCLOCKDIV);            // 0xD5
    SSD1306_command(0x80);                           // the suggested ratio 0x80

    SSD1306_command(SSD1306_SETMULTIPLEX);                  // 0xA8
    SSD1306_command(SSD1306_LCDHEIGHT - 1);

    SSD1306_command(SSD1306_SETDISPLAYOFFSET);              // 0xD3
    SSD1306_command(0x0);                                   // no offset
    SSD1306_command(SSD1306_SETSTARTLINE);			        // line #0
    SSD1306_command(SSD1306_CHARGEPUMP);                    // 0x8D
    SSD1306_command(0x14);			// using internal VCC

    SSD1306_command(SSD1306_MEMORYMODE);                    // 0x20/////
    SSD1306_command(0x00);                         // 0x00 horizontal addressing

    SSD1306_command(SSD1306_SEGREMAP | 0x1);
    SSD1306_command(SSD1306_COMSCANDEC);

    SSD1306_command(SSD1306_SETCOMPINS);                    // 0xDA
    SSD1306_command(0x02);
    SSD1306_command(SSD1306_SETCONTRAST);                   // 0x81
    SSD1306_command(0x8F);

    SSD1306_command(SSD1306_SETPRECHARGE);                  // 0xd9
    SSD1306_command(0xF1);

    SSD1306_command(SSD1306_SETVCOMDETECT);                 // 0xDB
    SSD1306_command(0x40);

    SSD1306_command(SSD1306_DISPLAYALLON_RESUME);           // 0xA4

    SSD1306_command(SSD1306_NORMALDISPLAY);                 // 0xA6
    SSD1306_command(SSD1306_DEACTIVATE_SCROLL);
    SSD1306_command(SSD1306_DISPLAYON);                     //switch on OLED

}
void SSD1306_clear()
{
    SSD1306_command(SSD1306_COLUMNADDR);
    SSD1306_command(0);   // Column start address (0 = reset)
    SSD1306_command(SSD1306_LCDWIDTH - 1); // Column end address (127 = reset)

    SSD1306_command(SSD1306_PAGEADDR);
    SSD1306_command(0); // Page start address (0 = reset)
    SSD1306_command(3); // Page end address
    uint8_t twbrbackup = TWBR;
    TWBR = 12; // upgrade to 400KHz!
    // I2C
    for (uint16_t i = 0; i < (SSD1306_LCDWIDTH * SSD1306_LCDHEIGHT / 8); i++)
    {
        // send a bunch of data in one xmission
        TWI_Start();
        TWI_Write(_i2c_address);
        TWI_Write(0x40);	// This is data stream
        for (uint8_t x = 0; x < 16; x++)
        {
            TWI_Write(0);
            i++;
        }
        i--;
        TWI_Stop();
    }
    TWBR = twbrbackup;
}

void SSD1306_display(uint8_t *buffer)
{
    uint8_t twbrbackup = TWBR;
    TWBR = 12; // upgrade to 400KHz!
    // I2C
    for (uint16_t i = 0; i < (SSD1306_LCDWIDTH * SSD1306_LCDHEIGHT / 8); i++)
    {
        // send a bunch of data in one xmission
        TWI_Start();
        TWI_Write(_i2c_address);
        TWI_Write(0x40);	// This is data stream
        for (uint8_t x = 0; x < 16; x++)
        {
            TWI_Write(buffer[i]);
            i++;
        }
        i--;
        TWI_Stop();
    }
    TWBR = twbrbackup;
}

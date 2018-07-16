/*
 * twi.c
 *
 *  Created on: Jul 13, 2018
 *      Author: Daniel Knezevic
 */

#include "twi.h"

void TWI_Init(void)
{
    //set SCL to 100kHz
    TWSR = 0x00;
    TWBR = 0x48;
    TWDR = 0xFF;
    //enable TWI
    TWCR = (1 << TWEN);
}

void TWI_Start(void)
{
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)))
        ;

}
//send stop signal
void TWI_Stop(void)
{
    TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
}
void TWI_Write(uint8_t u8data)
{
    TWDR = u8data;
    TWCR = (1 << TWINT) | (1 << TWEN);
    while ((TWCR & (1 << TWINT)) == 0)
        ;
}

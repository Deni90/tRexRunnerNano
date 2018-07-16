/*
 * twi.h
 *
 *  Created on: Jul 13, 2018
 *      Author: Daniel Knezevic
 */

#ifndef TWI_TWI_H_
#define TWI_TWI_H_

#include <avr/io.h>

void TWI_Init(void);
void TWI_Start(void);
void TWI_Stop(void);
void TWI_Write(uint8_t u8data);

#endif /* TWI_TWI_H_ */

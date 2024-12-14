# Welcome to tRexRunnerNano Project

## About
This project tries to be the smallest implementation of Google's easter egg T-Rex Runner with resolution of only 128x32px.
![alt text](https://raw.githubusercontent.com/Deni90/tRexRunnerNano/master/images/game.png)

This branch contains a port of the game from AVR Atmega88 microcontroller to Raspberry Pi Pico 2W.

Project is created with Visual Studio Code and Raspberry Pi Pico extension.

GPIO configuration:
| Header File | Macro name | rpi Pico 2W GPIO |
| ------- | --- | --- |
| ssd1306/ssd1306.h | I2C_SDA_GPIO | 20 |
| ssd1306/ssd1306.h | I2C_SCL_GPIO | 21 |
| tRexRunner.h | LEFT_BUTTON_GPIO | 1 |
| tRexRunner.h | LEFT_BUTTON_GPIO | 0 |

## DM DIY-MORE OLED-091
Popular I2C OLED 32x128 display from diy-more has an issue. It doesn't properly start after power on, but starts without problems after reset.
To solve this problem swap R3 and R4 resistors.

Solution found at:
https://youtu.be/__3InV5tdzM


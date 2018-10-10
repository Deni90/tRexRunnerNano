[![Build Status](https://travis-ci.org/Deni90/tRexRunnerNano.svg?branch=master)](https://travis-ci.org/Deni90/tRexRunnerNano)

# Welcome to tRexRunnerNano Project

## About
This project tries to be the smallest implementation of Google's easter egg T-Rex Runner with resolution of only 128x32px.
![alt text](https://raw.githubusercontent.com/Deni90/tRexRunnerNano/master/images/game.png)

The goal is to make a small game console by using Atmels Atmega88 microcontroller and SSSD1306 oled display.

## Schematic  & PCB ##
Made with AUTODESK EAGLE 9.0.0

## Building ##
```
cd firmware
make hex
```

## Flashing the MCU
Before flashing set up the AVR programmer by changing the value of PROGRAMMER variable in firmware/Makefile.
By default it is set to usbasp.
```
PROGRAMMER = usbasp
```
To flash both fuses and firmware:
```
cd firmware
make program
```

To flash firmware only:
```
cd firmware
make flash
```

For more information:
```
cd firmware
make
```
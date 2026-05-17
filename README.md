# Welcome to tRexRunnerNano Project

## About
This project tries to be the smallest implementation of Google's easter egg T-Rex Runner with resolution of only 128x32px.

![](https://raw.githubusercontent.com/Deni90/tRexRunnerNano/master/images/game.png)

The goal is to make a small game console by using WCH CH32V003 microcontroller and SSSD1306 oled display.

## Schematic  & PCB
Made with KiCAD 10

## Firmware

Firmware is written in C using **ch32fun**, an open source development environment for the CH32V003.

WCH CH-Link is used for flashing the firmware

### Building and flashing

```
make
```

## DM DIY-MORE OLED-091
Popular I2C OLED 32x128 display from diy-more has an issue. It doesn't properly start after power on, but starts without problems after reset.
To solve this problem swap R3 and R4 resistors.

Solution found at:
https://youtu.be/__3InV5tdzM

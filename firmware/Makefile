all : flash

TARGET:=tRexRunner

TARGET_MCU?=CH32V003

ADDITIONAL_C_FILES+=ssd1306.c

include ch32fun/ch32fun/ch32fun.mk

# LDFLAGS += -lm

flash : cv_flash
clean : cv_clean

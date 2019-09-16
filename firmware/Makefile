DEVICE  = atmega88
F_CPU   = 8000000      # in Hz
FUSE_L  = 0xe2
FUSE_H  = 0xd5

PROGRAMMER = usbasp

AVRDUDE = avrdude -c ${PROGRAMMER} -p $(DEVICE) # edit this line for your programmer

CFLAGS  = 
OBJECTS = twi/twi.o ssd1306/ssd1306.o tRexRunner.o

COMPILE = avr-gcc -Wall -Os -std=gnu99 -DF_CPU=$(F_CPU) $(CFLAGS) -mmcu=$(DEVICE)

# symbolic targets:
help:
	@echo "This Makefile has no default rule. Use one of the following:"
	@echo "make hex ........... to build main.hex"
	@echo "make eeprom......... to build main.eep"
	@echo "make program ....... to flash fuses irmware and eeprom"
	@echo "make fuse .......... to flash the fuses"
	@echo "make flash ......... to flash the firmware (use this on metaboard)"
	@echo "make eeprom ........ to load the eeprom image"
	@echo "make clean ..... to delete objects and hex file"

hex: main.hex

eeprom: main.eep

program: flash fuse eeprom

# rule for programming fuse bits:
fuse:
	@[ "$(FUSE_H)" != "" -a "$(FUSE_L)" != "" ] || \
		{ echo "*** Edit Makefile and choose values for FUSE_L and FUSE_H!"; exit 1; }
	$(AVRDUDE) -U hfuse:w:$(FUSE_H):m -U lfuse:w:$(FUSE_L):m

# rule for uploading firmware:
flash: main.hex
	$(AVRDUDE) -U flash:w:main.hex:i

# rule for uploading eeprom:
eeprom: main.eep
	$(AVRDUDE) -U eeprom:w:main.eep:i

# rule for deleting dependent files (those which can be built by Make):
clean:
	rm -f main.hex main.eep main.lst main.obj main.cof main.list main.map main.eep.hex main.elf *.o twi/*.o ssd1306/*.o

# Generic rule for compiling C files:
.c.o:
	$(COMPILE) -c $< -o $@


# file targets:

main.elf: $(OBJECTS)
	$(COMPILE) -o main.elf $(OBJECTS)

main.hex: main.elf
	rm -f main.hex main.eep.hex
	avr-objcopy -j .text -j .data -O ihex main.elf main.hex
	avr-size main.hex

main.eep: main.elf
	avr-objcopy -j .eeprom --set-section-flags=.eeprom=alloc,load --change-section-lma .eeprom=0 --no-change-warnings -O ihex "main.elf" "main.eep"

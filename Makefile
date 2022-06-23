#BUILDS=atmega8_snesmote atmega168 atmega168_13button
#BUILDS=atmega168
BUILDS=atmega168_13button_12MHz

all: $(addsuffix .hex,$(BUILDS))

%.hex: *.c *.h
	$(MAKE) -f Makefile.$*

clean:
	rm *.hex objs-*/*.o *.elf

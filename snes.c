/* Nes/Snes to Wiimote
 * Original Copyright (C) 2012 Raphaël Assénat
 * Changes Copyright by (C) 2022 Akerasoft
 * Credits:
 * Robert Kolski is the programmer for Akerasoft.
 *
 * Based on earlier work:
 *
 * Nes/Snes/Genesis/SMS/Atari to USB
 * Copyright (C) 2006-2007 Raphaël Assénat
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * The original author may be contacted at raph@raphnet.net
 * The author for changes is robert.kolski@akerasoft.com
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <string.h>
#include "gamepads.h"
#include "snes.h"

#define GAMEPAD_BYTES	2

/******** IO port definitions **************/
#define NES_8_BUTTONS_DDR  DDRD
#define NES_8_BUTTONS_PORT PORTD
#define NES_8_BUTTONS_PIN  PIND

#define CLSC_7_BUTTONS_DDR   DDRB
#define CLSC_7_BUTTONS_PORT  PORTB
#define CLSC_7_BUTTONS_PIN   PINB
#define CLSC_7_BUTTONS_MASK  0xFE

#define SNES_5_BUTTONS_DDR   DDRB
#define SNES_5_BUTTONS_PORT  PORTB
#define SNES_5_BUTTONS_PIN   PINB
#define SNES_5_BUTTONS_MASK  0x1F
#define SNES_5_BUTTONS_SHIFT 3

#define SNES_4_BUTTONS_DDR   DDRB
#define SNES_4_BUTTONS_PORT  PORTB
#define SNES_4_BUTTONS_PIN   PINB
#define SNES_4_BUTTONS_MASK  0x0F
#define SNES_4_BUTTONS_SHIFT 4

#define HOME_BUTTON_PORT   PORTB
#define HOME_BUTTON_DDR    DDRB
#define HOME_BUTTON_PIN    PINB
#define HOME_BUTTON_BIT    (1<<4)

#define HOME_BUTTON_DATA_BIT (1<<3)


/*********** prototypes *************/
static char snesInit(void);
static char snesUpdate(void);


// the most recent bytes we fetched from the controller
static unsigned char last_read_controller_bytes[GAMEPAD_BYTES];
// the most recently reported bytes
static unsigned char last_reported_controller_bytes[GAMEPAD_BYTES];

static char nes_mode = 0;

static char snesInit(void)
{
	unsigned char sreg;
	sreg = SREG;
	cli();

	// 8 NES buttons are input - all bits off
	NES_8_BUTTONS_DDR = 0;

	// 8 NES buttons are normally high - all bits one
	NES_8_BUTTONS_PORT = 0xFF;

#if WITH_15_BUTTONS
	// 7 classic buttons are input - those 7 bits are off
	CLSC_7_BUTTONS_DDR &= ~CLSC_7_BUTTONS_MASK;

	// 7 classic buttons are normally high - all bits one
	CLSC_7_BUTTONS_PORT |= CLSC_7_BUTTONS_MASK;
#elif WITH_13_BUTTONS
	// 5 SNES buttons are input - those 5 bits are off
	SNES_5_BUTTONS_DDR &= ~SNES_5_BUTTONS_MASK;

	// 5 SNES buttons are normally high - all bits one
	SNES_5_BUTTONS_PORT |= SNES_5_BUTTONS_MASK;
#elif WITH_12_BUTTONS
	// 4 SNES buttons are input - those 4 bits are off
	SNES_4_BUTTONS_DDR &= ~SNES_4_BUTTONS_MASK;

	// 4 SNES buttons are normally high - all bits one
	SNES_4_BUTTONS_PORT |= SNES_4_BUTTONS_MASK;
#elif WITH_9_BUTTONS
	// home button is input
	HOME_BUTTON_DDR &= ~(HOME_BUTTON_BIT);
	
	// home button is normally high
	HOME_BUTTON_PORT |= HOME_BUTTON_BIT;
#else
	// for 8 buttons there is no home button
#endif

	snesUpdate();

	SREG = sreg;

	return 0;
}


/*
 *
       Bit position     Button Reported           NES Button Reported        PIN
        ===========     ===============           =====================      ===================
        0               B                         A (Mistake = B)            PD7
        1               Y                         B (Mistake = A)            PD6
        2               Select                    Select                     PD5
        3               Start                     Start                      PD4
        4               Up on joypad              Up                         PD3
        5               Down on joypad            Down                       PD2
        6               Left on joypad            Left                       PD1
        7               Right on joypad           Right                      PD0
        8               A                         NA                         PB3
        9               X                         NA                         PB2
        10              L                         NA                         PB1
        11              R                         NA                         PB0
        12              HOME                      Home                       PB4
        13              none (always high)        NA                         PB5 (not used)
        14              none (always high)        NA                         PB6 (so ignored)
        15              none (always high)        NA                         PB7 (so ignored)
	
// 15 button design	
        8               A                         NA                         PB7 (crystal cannot be used)
        9               X                         NA                         PB6 (crystal cannot be used)
        10              L                         NA                         PB5
        11              R                         NA                         PB4
        12              HOME                      Home                       PB3
        13              ZL                        NA                         PB2
        14              ZR                        NA                         PB1
        15              none (always high)        NA                         PB0 (unused)

 *
 */
 
// bug fix
// for left and right favor right
// for up and down favor up
char dpadmap[16] = {0, 1, 2, 1, 4, 5, 6, 5, 8, 9, 10, 9, 8, 9, 10, 9};

static char snesUpdate(void)
{
	unsigned char tmp=0;
	unsigned char tmpraw=0;

#if WITH_15_BUTTONS
	tmpraw = ~NES_8_BUTTONS_PIN;
    tmpraw = (tmpraw & 0xF0) | (dpadmap[tmpraw & 0x0F]);
	last_read_controller_bytes[0] = tmpraw;
	tmp = ((~CLSC_7_BUTTONS_PIN) & CLSC_7_BUTTONS_MASK);
#elif WITH_13_BUTTONS
	tmpraw = ~NES_8_BUTTONS_PIN;
    tmpraw = (tmpraw & 0xF0) | (dpadmap[tmpraw & 0x0F]);
	last_read_controller_bytes[0] = tmpraw;
	tmpraw = ((~SNES_5_BUTTONS_PIN) & SNES_5_BUTTONS_MASK);
	// 4 buttons shift is correct here
	tmp = (tmpraw << SNES_4_BUTTONS_SHIFT) & 0xF0;
      // home is handled here
	  if ((tmpraw & HOME_BUTTON_BIT))
      {
          tmp |= HOME_BUTTON_DATA_BIT;
      }
#elif WITH_12_BUTTONS
	tmpraw = ~NES_8_BUTTONS_PIN;
    tmpraw = (tmpraw & 0xF0) | (dpadmap[tmpraw & 0x0F]);
	last_read_controller_bytes[0] = tmpraw;
	tmpraw = ((~SNES_4_BUTTONS_PIN) & SNES_4_BUTTONS_MASK);
	tmp = (tmpraw << SNES_4_BUTTONS_SHIFT) & 0xF0;
#elif WITH_9_BUTTONS
	tmpraw = ~NES_8_BUTTONS_PIN;
    tmpraw = (tmpraw & 0xF0) | (dpadmap[tmpraw & 0x0F]);
	//tmp = tmpraw & 0x3F;
    // A and B were transposed in schematic.
	// See comment about mistake
	//if (tmpraw & (1<<7)) { tmp |= (1<<6); }
	//if (tmpraw & (1<<6)) { tmp |= (1<<7); }
	//last_read_controller_bytes[0] = tmp;
	last_read_controller_bytes[0] = tmpraw;
	tmp = ((~HOME_BUTTON_PIN & HOME_BUTTON_BIT)) ? HOME_BUTTON_DATA_BIT : 0;
#else
	// 8 buttons
	tmpraw = ~NES_8_BUTTONS_PIN;
    tmpraw = (tmpraw & 0xF0) | (dpadmap[tmpraw & 0x0F]);
	last_read_controller_bytes[0] = tmpraw;
	// a real NES authentic controller would read 0 (GND) and is inverted
	// so you would get 0xFF here
	tmp = 0xFF;
#endif
	last_read_controller_bytes[1] = tmp;

	return 0;
}

static char snesChanged(void)
{
	return memcmp(last_read_controller_bytes,
					last_reported_controller_bytes, GAMEPAD_BYTES);
}

static void snesGetReport(gamepad_data *dst)
{
	unsigned char h, l;

	if (dst != NULL)
	{
		l = last_read_controller_bytes[0];
		h = last_read_controller_bytes[1];

		// in this version we compile it as NES or SNES
#if WITH_15_BUTTONS
		nes_mode = 0;
		dst->nes.pad_type = PAD_TYPE_CLASSIC;
		dst->snes.buttons = l;
		dst->snes.buttons |= h<<8;
		dst->snes.raw_data[0] = l;
		dst->snes.raw_data[1] = h;
#elif WITH_13_BUTTONS
		nes_mode = 0;
		dst->nes.pad_type = PAD_TYPE_SNES;
		dst->snes.buttons = l;
		dst->snes.buttons |= h<<8;
		dst->snes.raw_data[0] = l;
		dst->snes.raw_data[1] = h;
#elif WITH_12_BUTTONS
		nes_mode = 0;
		dst->nes.pad_type = PAD_TYPE_SNES;
		dst->snes.buttons = l;
		dst->snes.buttons |= h<<8;
		dst->snes.raw_data[0] = l;
		dst->snes.raw_data[1] = h;
#elif WITH_9_BUTTONS
		nes_mode = 1;
		// Nes controllers send the data in this order:
		// A B Sel St U D L R
		// NA NA NA NA HOME NA NA NA
		dst->nes.pad_type = PAD_TYPE_NES;
		dst->nes.buttons = l;
		dst->nes.buttons |= h<<8;
		dst->nes.raw_data[0] = l;
		dst->nes.raw_data[1] = h;
#else
		// 8 buttons
		nes_mode = 1;
		// Nes controllers send the data in this order:
		// A B Sel St U D L R
		// NA NA NA NA HOME NA NA NA
		dst->nes.pad_type = PAD_TYPE_NES;
		dst->nes.buttons = l;
		dst->nes.raw_data[0] = l;
#endif
	}
	memcpy(last_reported_controller_bytes,
			last_read_controller_bytes,
			GAMEPAD_BYTES);
}

static Gamepad SnesGamepad = {
	.init		= snesInit,
	.update		= snesUpdate,
	.changed	= snesChanged,
	.getReport	= snesGetReport
};

Gamepad *snesGetGamepad(void)
{
	return &SnesGamepad;
}

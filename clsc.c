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
#include "clsc.h"

#define GAMEPAD_BYTES	2

/******** IO port definitions **************/
#define NES_8_BUTTONS_DDR  DDRD
#define NES_8_BUTTONS_PORT PORTD
#define NES_8_BUTTONS_PIN  PIND

#define CLSC_7_BUTTONS_DDR   DDRB
#define CLSC_7_BUTTONS_PORT  PORTB
#define CLSC_7_BUTTONS_PIN   PINB
#define CLSC_7_BUTTONS_MASK  0xFE

/*********** prototypes *************/
static char clscInit(void);
static char clscUpdate(void);


// the most recent bytes we fetched from the controller
static unsigned char last_read_controller_bytes[GAMEPAD_BYTES];
// the most recently reported bytes
static unsigned char last_reported_controller_bytes[GAMEPAD_BYTES];

static unsigned char last_read_lx, last_read_ly;
static unsigned char last_read_rx, last_read_ry;
static unsigned char last_read_lt, last_read_rt;
static unsigned char last_reported_lx, last_reported_ly;
static unsigned char last_reported_rx, last_reported_ry;
#if WITH_ANALOG_TRIGGERS
static unsigned char last_reported_lt, last_reported_rt;
#endif

static char clscInit(void)
{
	unsigned char sreg;
	sreg = SREG;
	cli();

	// 8 NES buttons are input - all bits off
	NES_8_BUTTONS_DDR = 0;

	// 8 NES buttons are normally high - all bits one
	NES_8_BUTTONS_PORT = 0xFF;

	// 7 classic buttons are input - those 7 bits are off
	CLSC_7_BUTTONS_DDR &= ~CLSC_7_BUTTONS_MASK;

	// 7 classic buttons are normally high - all bits one
	CLSC_7_BUTTONS_PORT |= CLSC_7_BUTTONS_MASK;
	
	// use 8MHz internal clock and divide by 64.
	ADCSRA &= ~(1<<ADPS0);// Select ADC Prescalar to 64,
	ADCSRA |= (1<<ADPS2) | (1<<ADPS1);// Select ADC Prescalar to 64,
    // 8MHz/64 = 125KHz

	clscUpdate();

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
        8               A                         NA                         PB7 (crystal cannot be used)
        9               X                         NA                         PB6 (crystal cannot be used)
        10              L                         NA                         PB5
        11              R                         NA                         PB4
        12              HOME                      Home                       PB3
        13              ZL                        NA                         PB2
        14              ZR                        NA                         PB1
        15              none (always high)        NA                         PB0 (unused)

		PC0 - LX
		PC1 - LY
		PC2 - RX
		PC3 - RY
		PC4 - I2C
		PC5 - I2C
		ADC6 - LT (TQFP/QFN)
		ADC7 - RT (TQFP/QFN)
 *
 */
 
// bug fix
// for left and right favor right
// for up and down favor up
char dpadmap[16] = {0, 1, 2, 1, 4, 5, 6, 5, 8, 9, 10, 9, 8, 9, 10, 9};

static char clscUpdate(void)
{
	unsigned char tmp=0;
	unsigned char tmpraw=0;
	uint16_t ADC_10bit_Result = 0;

	tmpraw = ~NES_8_BUTTONS_PIN;
    tmpraw = (tmpraw & 0xF0) | (dpadmap[tmpraw & 0x0F]);
	last_read_controller_bytes[0] = tmpraw;
	tmp = ((~CLSC_7_BUTTONS_PIN) & CLSC_7_BUTTONS_MASK);
	last_read_controller_bytes[1] = tmp;
	
	ADMUX = ADMUX & 0xF0;           //Clear MUX0-MUX3,Important in Multichannel conv
	ADMUX  |= 0;         // Select the ADC channel to convert,Aref pin tied to 5V
    ADCSRA |= (1<<ADEN)  | (1<<ADSC);      // Enable ADC and Start the conversion
    while( !(ADCSRA & (1<<ADIF)) );       // Wait for conversion to finish
    ADC_10bit_Result   =  ADC;            // Read the ADC value to 16 bit int   
    ADCSRA |= (1<<ADIF);   // Clear ADIF,ADIF is cleared by writing a 1 to ADSCRA
	
	last_read_lx = ADC_10bit_Result >> 2;
	
	ADMUX = ADMUX & 0xF0;           //Clear MUX0-MUX3,Important in Multichannel conv
	ADMUX  |= 1;         // Select the ADC channel to convert,Aref pin tied to 5V
    ADCSRA |= (1<<ADEN)  | (1<<ADSC);      // Enable ADC and Start the conversion
    while( !(ADCSRA & (1<<ADIF)) );       // Wait for conversion to finish
    ADC_10bit_Result   =  ADC;            // Read the ADC value to 16 bit int   
    ADCSRA |= (1<<ADIF);   // Clear ADIF,ADIF is cleared by writing a 1 to ADSCRA
	
	last_read_ly = ADC_10bit_Result >> 2;

	ADMUX = ADMUX & 0xF0;           //Clear MUX0-MUX3,Important in Multichannel conv
	ADMUX  |= 2;         // Select the ADC channel to convert,Aref pin tied to 5V
    ADCSRA |= (1<<ADEN)  | (1<<ADSC);      // Enable ADC and Start the conversion
    while( !(ADCSRA & (1<<ADIF)) );       // Wait for conversion to finish
    ADC_10bit_Result   =  ADC;            // Read the ADC value to 16 bit int   
    ADCSRA |= (1<<ADIF);   // Clear ADIF,ADIF is cleared by writing a 1 to ADSCRA
	
	last_read_rx = ADC_10bit_Result >> 2;

	ADMUX = ADMUX & 0xF0;           //Clear MUX0-MUX3,Important in Multichannel conv
	ADMUX  |= 3;         // Select the ADC channel to convert,Aref pin tied to 5V
    ADCSRA |= (1<<ADEN)  | (1<<ADSC);      // Enable ADC and Start the conversion
    while( !(ADCSRA & (1<<ADIF)) );       // Wait for conversion to finish
    ADC_10bit_Result   =  ADC;            // Read the ADC value to 16 bit int   
    ADCSRA |= (1<<ADIF);   // Clear ADIF,ADIF is cleared by writing a 1 to ADSCRA
	
	last_read_ry = ADC_10bit_Result >> 2;

#if WITH_ANALOG_TRIGGERS	
	ADMUX = ADMUX & 0xF0;           //Clear MUX0-MUX3,Important in Multichannel conv
	ADMUX  |= 6;         // Select the ADC channel to convert,Aref pin tied to 5V
    ADCSRA |= (1<<ADEN)  | (1<<ADSC);      // Enable ADC and Start the conversion
    while( !(ADCSRA & (1<<ADIF)) );       // Wait for conversion to finish
    ADC_10bit_Result   =  ADC;            // Read the ADC value to 16 bit int   
    ADCSRA |= (1<<ADIF);   // Clear ADIF,ADIF is cleared by writing a 1 to ADSCRA
	
	last_read_lt = ADC_10bit_Result >> 2;
	
	ADMUX = ADMUX & 0xF0;           //Clear MUX0-MUX3,Important in Multichannel conv
	ADMUX  |= 7;         // Select the ADC channel to convert,Aref pin tied to 5V
    ADCSRA |= (1<<ADEN)  | (1<<ADSC);      // Enable ADC and Start the conversion
    while( !(ADCSRA & (1<<ADIF)) );       // Wait for conversion to finish
    ADC_10bit_Result   =  ADC;            // Read the ADC value to 16 bit int   
    ADCSRA |= (1<<ADIF);   // Clear ADIF,ADIF is cleared by writing a 1 to ADSCRA
	
	last_read_rt = ADC_10bit_Result >> 2;
#endif
	
	return 0;
}

static char clscChanged(void)
{
	char val = memcmp(last_read_controller_bytes,
					last_reported_controller_bytes, GAMEPAD_BYTES);
					
	if (val != 0)
	{
		return val;
	}
	
	val = last_read_lx - last_reported_lx;
	
	if (val != 0)
	{
		return val;
	}

	val = last_read_ly - last_reported_ly;
	
	if (val != 0)
	{
		return val;
	}

	val = last_read_rx - last_reported_rx;
	
	if (val != 0)
	{
		return val;
	}

	val = last_read_ry - last_reported_ry;
	
#if WITH_ANALOG_TRIGGERS
	if (val != 0)
	{
		return val;
	}

	val = last_read_lt - last_reported_lt;
	
	if (val != 0)
	{
		return val;
	}

	val = last_read_rt - last_reported_rt;
#endif
	
	return val;
}

static void clscGetReport(gamepad_data *dst)
{
	unsigned char h, l;

	if (dst != NULL)
	{
		l = last_read_controller_bytes[0];
		h = last_read_controller_bytes[1];

		dst->classic.pad_type = PAD_TYPE_CLASSIC;
		dst->classic.buttons = l;
		dst->classic.buttons |= h<<8;
		dst->classic.controller_raw_data[0] = l;
		dst->classic.controller_raw_data[1] = h;
		dst->classic.controller_raw_data[2] = last_read_lx;
		dst->classic.controller_raw_data[3] = last_read_ly;
		dst->classic.controller_raw_data[4] = last_read_rx;
		dst->classic.controller_raw_data[5] = last_read_ry;
#if WITH_ANALOG_TRIGGERS	
		dst->classic.controller_raw_data[6] = last_read_lt;
		dst->classic.controller_raw_data[7] = last_read_rt;
#else
		dst->classic.controller_raw_data[6] = 0;
		dst->classic.controller_raw_data[7] = 0;
#endif
		dst->classic.lx = last_read_lx;
		dst->classic.ly = last_read_ly;
		dst->classic.rx = last_read_rx;
		dst->classic.ry = last_read_ry;
#if WITH_ANALOG_TRIGGERS	
		dst->classic.lt = last_read_lt;
		dst->classic.rt = last_read_rt;
#endif
	}
	memcpy(last_reported_controller_bytes,
			last_read_controller_bytes,
			GAMEPAD_BYTES);
	last_reported_lx = last_read_lx;
	last_reported_ly = last_read_ly;
	last_reported_rx = last_read_rx;
	last_reported_rx = last_read_ry;
#if WITH_ANALOG_TRIGGERS	
	last_reported_lt = last_read_lt;
	last_reported_rt = last_read_rt;
#endif
}

static Gamepad ClscGamepad = {
	.init		= clscInit,
	.update		= clscUpdate,
	.changed	= clscChanged,
	.getReport	= clscGetReport
};

Gamepad *clscGetGamepad(void)
{
	return &ClscGamepad;
}
